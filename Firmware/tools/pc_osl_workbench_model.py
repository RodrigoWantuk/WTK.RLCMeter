"""Desktop operations over the canonical Campaign; no Qt, fitting or wire encoder here."""
from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
import threading

import pc_bringup as bringup
import pc_cal_capture as capture
import pc_osl
import pc_osl_campaign as guided
from pc_osl_install import Installer

# Later, specifically authorized clamp conduction is intentionally not a
# prerequisite for a first capture. Its unresolved risk must be reviewed below.
PHYSICAL_CHECKS = tuple(dict.fromkeys(bringup.SERIAL_GATES + (
    "vmid", "pb9_k1", "pb8_range", "pa8_excitation", "pa15_charger",
    "powerup_waveforms", "powerdown_waveforms", "signal_backfeed",
    "analog_adc_voltages", "3v3_stability")))
ACKNOWLEDGEMENTS = {
    "assembly": "Population/DNP and R02/R05 evidence belongs to this board",
    "interlocks": "Charger/residual and K1/range bench checks permit this passive fixture",
    "clamp": "Reviewed A05-Y04 5 V to 3.3 V clamp risk and authorized this capture scope",
    "fixture": "Only passive, discharged fixtures; scope grounds only to actual GND",
    "deployment": "Separately deployed BRINGUP_CAL over SWD, preserving W25Q",
}


def inventory_save(path, inventory):
    guided.references(inventory)
    guided.atomic_write(path, (json.dumps(inventory, indent=2, allow_nan=False) + "\n").encode())


def import_evidence(path):
    """Retain the B01 session and checks, not just a generic validation boolean."""
    path = Path(path)
    raw = path.read_bytes() if path.stat().st_size <= bringup.MAX_FILE else b""
    if not raw:
        raise ValueError("empty or oversized B01 evidence")
    value = guided.read_json(path)
    session = bringup.validate_session(value.get("session", value))
    if session["firmware"]["profile"] != "BRINGUP":
        raise ValueError("B01 evidence must declare conventional BRINGUP, not BRINGUP_CAL")
    simulated = value.get("SIMULATED", False) or value.get("OFFLINE_REPLAY", False) or any(
        entry["simulated"] for entry in session["manual"].values())
    checks = value.get("checks", [])
    if not isinstance(checks, list) or any(not isinstance(c, dict) or c.get("status") not in bringup.STATES for c in checks):
        raise ValueError("invalid B01 checks")
    return dict(path=str(path.resolve()), sha256=hashlib.sha256(raw).hexdigest(),
                imported_utc=guided.utc(), SIMULATED=bool(simulated), session=session,
                checks=checks, identity=value.get("identity"))


def physical_missing(evidence):
    if evidence is None:
        return ["Import a B01 session or report with actual bench evidence"]
    missing = []
    if evidence["SIMULATED"]:
        missing.append("SIMULATED/replayed evidence cannot authorize physical capture")
    session = bringup.validate_session(evidence["session"])
    board = session["board"]
    if board.get("revision") != "Rev1":
        missing.append("Inspected board must be Rev1")
    for field in ("fabrication_identity", "population_notes", "dnp_verification"):
        try:
            guided.text(board.get(field), "board " + field)
        except ValueError as exc:
            missing.append(str(exc))
    if not board["photographs"]:
        missing.append("Board photographs missing")
    for key in PHYSICAL_CHECKS:
        entry = session["manual"][key]
        if entry["status"] != "PASS" or entry["simulated"]:
            missing.append(key + ": " + entry["status"] + " / physical evidence required")
    for key, entry in session["manual"].items():
        if entry["status"] == "STOP":
            missing.append(key + ": unresolved STOP")
    for check in evidence["checks"]:
        if check["status"] in ("STOP", "UNKNOWN"):
            missing.append(str(check.get("check")) + ": " + check["status"])
    for category in ("modules", "instruments"):
        for name, value in session[category].items():
            try:
                guided.text(value, category + " " + name)
            except ValueError as exc:
                missing.append(str(exc))
    return missing


def matrix(campaign):
    """Reconstruct every cell from persisted canonical evidence, including rejections."""
    captures = {guided.pair_id(r): r for r in campaign.document["captures"]}
    attempts = {guided.pair_id(a["request"]): a for a in campaign.document["attempts"]}
    rows = []
    for key in pc_osl.KEYS:
        cells = {}
        for standard in capture.STANDARDS:
            pair = key, standard
            accepted, attempt = captures.get(pair), attempts.get(pair)
            cells[standard] = dict(state="ACCEPTED" if accepted else "REJECTED" if attempt else "PENDING",
                                   observation=accepted, reason=attempt["reason"] if attempt else None)
        group = [pc_osl.standard_from_capture(cells[s]["observation"]) for s in capture.STANDARDS if cells[s]["observation"]]
        solution = pc_osl.solve(group) if len(group) == 3 else None
        rows.append(dict(condition=key.as_json(), cells=cells, complete=solution is not None,
                         solver=None if solution is None else dict(fit_channel=solution.fit_channel,
                         min_separation=solution.min_separation, values=solution.values())))
    return rows


class InterruptibleClient(capture.Client):
    """Check a UI event BETWEEN requests; CANCEL remains on the single serial owner."""
    def __init__(self, transport, cancel_event):
        super().__init__(transport)
        self.cancel_event, self.in_capture = cancel_event, False

    def checkpoint(self):
        if self.in_capture and self.cancel_event.is_set():
            self.cancel_event.clear()  # Campaign.run can now STATUS/CANCEL without interruption.
            raise KeyboardInterrupt

    def status(self):
        self.checkpoint()
        return super().status()

    def capture(self, *args, **kwargs):
        self.checkpoint()
        result = super().capture(*args, **kwargs)
        self.checkpoint()  # Cancel before durable acceptance if it arrived during RESULT.
        return result


class Workbench:
    """Owned exclusively by one worker. Snapshots are copies, never a second database."""
    def __init__(self, transport_factory=None):
        self.campaign = None
        self.client = None
        self.transport = None
        self.mode = None
        self.state = None
        self.installation = None
        self.operator = None
        self.unknown = False
        self.gate_approved = False
        self.cancel_event = threading.Event()
        self.transport_factory = transport_factory

    def open(self, path):
        if self.client:
            raise ValueError("disconnect before opening another campaign")
        campaign = guided.Campaign.load(path)
        self.campaign = campaign
        self.gate_approved = False
        # A saved record cannot establish the CURRENT electrical state after a
        # crash/disconnect, even if its last capture completed. Fresh connect is required.
        self.unknown = campaign.document["device"] is not None

    def create(self, path, inventory, order="standard"):
        if self.client:
            raise ValueError("disconnect before creating another campaign")
        self.campaign = guided.Campaign.create(path, copy.deepcopy(inventory), order)
        self.gate_approved = False

    def clear(self):
        if self.client:
            raise ValueError("disconnect before closing the campaign")
        self.campaign = None
        self.gate_approved = False

    def connect(self, mode, address, operator):
        if self.client or not self.campaign:
            raise ValueError("open a campaign and disconnect any existing device first")
        if mode not in ("Simulation", "Physical"):
            raise ValueError("explicit Simulation or Physical mode required")
        guided.text(operator, "operator")
        if mode == "Physical" and any(row["fixture"].startswith("SYNTHETIC_") for row in
                                       guided.plan(self.campaign.document["inventory"], self.campaign.document["order"])):
            raise ValueError("synthetic example inventory cannot be used for physical capture; identify actual fixtures")
        if self.transport_factory:
            transport = self.transport_factory(mode, address)
        elif mode == "Simulation":
            transport = capture.FakeSerial(address)
        else:
            import serial
            transport = serial.Serial(port=None, baudrate=115200, timeout=0.05, write_timeout=1)
            transport.dtr = transport.rts = False
            transport.port = address
            transport.open()
        self.cancel_event.clear()
        client = InterruptibleClient(transport, self.cancel_event)
        try:
            state = self.campaign.connect(client, physical=mode == "Physical", operator=operator)
            if mode == "Simulation" and not client.identity["synthetic"]:
                raise ValueError("simulation requires the C fixture's synthetic identity")
            installation = Installer(client).status()
        except BaseException:
            transport.close()
            raise
        self.client, self.transport, self.mode = client, transport, mode
        self.operator, self.state, self.installation = operator, state, installation
        self.unknown, self.gate_approved = False, False

    def status(self):
        if not self.client:
            raise ValueError("device disconnected")
        try:
            if self.client.identify() != self.campaign.document["device"]:
                raise ValueError("device/firmware identity changed; disconnect and investigate")
            self.state = self.campaign.safe_status(self.client)
            self.installation = Installer(self.client).status()
            if (not self.installation["storage_available"] or self.installation["state"] in (1, 2, 3) or
                    self.installation["next_sequence"] != self.campaign.document["successor"]):
                raise ValueError("installation busy, storage unavailable or successor sequence changed")
        except (ValueError, OSError, TimeoutError):
            self.unknown, self.gate_approved = True, False
            raise
        self.unknown = False

    def evidence(self, path):
        if not self.campaign:
            raise ValueError("open a campaign first")
        imported = import_evidence(path)
        evidence = self.campaign.document.setdefault("desktop_evidence", dict(imports=[], decisions=[]))
        evidence["imports"].append(imported)
        self.campaign.save()
        self.gate_approved = False

    def approve(self, board, deployed_sha, review, checks):
        if not self.client or self.mode != "Physical" or self.unknown:
            raise ValueError("a known physical connection is required for operator approval")
        evidence = self.latest_evidence()
        missing = physical_missing(evidence)
        if missing:
            raise ValueError("Physical prerequisites missing: " + "; ".join(missing))
        if board != evidence["session"]["board"]["fabrication_identity"]:
            raise ValueError("declared board differs from imported inspected board")
        import re
        if not re.fullmatch("[0-9a-f]{40}", deployed_sha) or not deployed_sha.startswith(self.client.identity["git"]):
            raise ValueError("record the full deployed BRINGUP_CAL SHA matching the reported prefix")
        guided.text(review, "bench interlock/analog risk review and authorization reference")
        if set(checks) != set(ACKNOWLEDGEMENTS) or not all(checks.values()):
            raise ValueError("explicit acknowledgement of every physical gate required")
        self.status()
        decision = dict(utc=guided.utc(), operator=self.operator, board=board,
                        evidence_sha256=evidence["sha256"], review=review, acknowledgements=checks,
                        from_profile="BRINGUP", from_firmware=evidence["session"]["firmware"],
                        to_profile="BRINGUP_CAL", deployed_sha=deployed_sha,
                        device=self.client.identity, active_sequence=self.state["calibration_sequence"],
                        physically_qualified=False, permission="OPERATOR_AUTHORIZED_PASSIVE_CAPTURE_ONLY")
        self.campaign.document["desktop_evidence"]["decisions"].append(decision)
        self.campaign.save()
        self.gate_approved = True

    def latest_evidence(self):
        imports = (self.campaign.document.get("desktop_evidence", {}).get("imports", []) if self.campaign else [])
        return imports[-1] if imports else None

    def acquire(self, pair=None, recapture=False, confirmed=False, log=lambda message: None):
        if not confirmed:
            raise ValueError("explicit confirmation required for each capture")
        if not self.client or self.unknown:
            raise ValueError("device disconnected or teardown UNKNOWN; reconnect and verify SAFE")
        if self.mode == "Physical" and not self.gate_approved:
            raise ValueError("physical evidence/operator gate not acknowledged for this connection")
        missing = self.campaign.missing()
        if not recapture:
            if not missing or (pair is not None and pair != guided.pair_id(missing[0])):
                raise ValueError("only the displayed next missing observation may be captured")
        elif pair is None:
            raise ValueError("explicit recapture selection required")
        try:
            self.status()  # Fresh capture AND installation status; never START during a competing transaction.
        except (ValueError, OSError, TimeoutError) as exc:
            request = next(row for row in guided.plan(self.campaign.document["inventory"], self.campaign.document["order"])
                           if guided.pair_id(row) == pair) if recapture else missing[0]
            self.campaign.document["attempts"].append(dict(utc=guided.utc(), request=request, reason=str(exc), raw=None, cancellation=None))
            self.campaign.save()
            raise
        self.client.in_capture = True
        try:
            self.campaign.run(self.client, lambda request: confirmed, log, limit=1,
                              recapture=pair if recapture else None)
        except (KeyboardInterrupt, ValueError, OSError, TimeoutError):
            self.gate_approved = False
            attempt = self.campaign.document["attempts"][-1] if self.campaign.document["attempts"] else {}
            cancel = attempt.get("cancellation")
            self.unknown = cancel is None or not cancel["transfer_safe"] or cancel.get("busy", False)
            if isinstance(cancel, dict) and self.unknown:
                log("UNKNOWN teardown: do not change fixture; firmware timeout remains authoritative")
            raise
        finally:
            self.client.in_capture = False
            self.cancel_event.clear()
        self.status()

    def build(self, out):
        if not self.campaign or self.campaign.missing():
            raise ValueError("99 accepted observations / 33 usable triplets required")
        result = guided.write_outputs(self.campaign, out)
        out = Path(out)
        evidence = self.campaign.document.get("desktop_evidence", {})
        provenance = dict(imports=[dict(path=e["path"], sha256=e["sha256"], SIMULATED=e["SIMULATED"],
                          board=e["session"]["board"], manual_status={k: v["status"] for k, v in e["session"]["manual"].items()})
                          for e in evidence.get("imports", [])], decisions=evidence.get("decisions", []),
                          qualification="UNQUALIFIED / REQUIRES_BENCH_VALIDATION")
        report = guided.read_json(out / "report.json")
        report["desktop_evidence"] = provenance
        guided.atomic_write(out / "report.json", (json.dumps(report, indent=2) + "\n").encode())
        guided.atomic_write(out / "report.md", (out / "report.md").read_bytes() +
            ("\n## Desktop physical evidence and operator review\n\nNot a safety or accuracy certificate. Full original evidence is retained in campaign.json.\n\n```json\n" +
             json.dumps(provenance, indent=2) + "\n```\n").encode())
        return result

    def disconnect(self):
        if not self.transport:
            return
        try:
            canceled = guided.cancel_pending(self.client)
            self.unknown = not canceled["transfer_safe"] or canceled.get("busy", False)
        finally:
            self.transport.close()
            self.client = self.transport = None
            self.gate_approved = False

    def snapshot(self):
        document = self.campaign.document if self.campaign else None
        missing = self.campaign.missing() if self.campaign else []
        fixtures = [r["fixture"] for r in missing]
        changes = sum(fixtures[i] != fixtures[i-1] for i in range(1, len(fixtures)))
        return copy.deepcopy(dict(path=str(self.campaign.path) if self.campaign else None,
             document=document, matrix=matrix(self.campaign) if self.campaign else [],
             progress=self.campaign.progress() if self.campaign else None,
             next=missing[0] if missing else None, remaining_changes=changes,
             connected=self.client is not None, mode=self.mode, status=self.state,
             installation=self.installation, unknown=self.unknown, gate_approved=self.gate_approved,
             physical_missing=physical_missing(self.latest_evidence())))
