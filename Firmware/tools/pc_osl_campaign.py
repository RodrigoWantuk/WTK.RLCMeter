#!/usr/bin/env python3
"""Human-confirmed, resumable PLC1 OSL campaigns; never installs calibration."""
from __future__ import annotations

import argparse
import copy
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import tempfile

import pc_cal_capture as capture
import pc_osl
from pc_osl_install import Installer, validate_candidate

FORMAT = "WTK_GUIDED_OSL_V1"
MAX_DOCUMENT = 8 * 1024 * 1024


def utc():
    return datetime.now(timezone.utc).isoformat()


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                                    allow_nan=False).encode("utf-8")).hexdigest()


def atomic_write(path, data):
    """Same-directory flush/fsync/replace; an interrupted save keeps the old file."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


@contextmanager
def progress_lock(path):
    """OS-released advisory lock; a PC crash cannot leave a stale ownership claim."""
    with open(str(path) + ".lock", "a+b") as stream:
        if stream.seek(0, os.SEEK_END) == 0:
            stream.write(b"0")
            stream.flush()
        stream.seek(0)
        if os.name == "nt":
            import msvcrt
            acquire = lambda: msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            release = lambda: msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl
            acquire = lambda: fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            release = lambda: fcntl.flock(stream.fileno(), fcntl.LOCK_UN)
        try:
            acquire()
        except OSError as exc:
            raise ValueError("campaign progress is busy in another process") from exc
        try:
            yield
        finally:
            stream.seek(0)
            release()


def read_json(path):
    path = Path(path)
    if path.stat().st_size > MAX_DOCUMENT:
        raise ValueError("campaign/reference file exceeds 8 MiB")
    return pc_osl.load_document(path)


def text(value, field):
    if not isinstance(value, str) or not value.strip() or value.strip().upper() in ("UNKNOWN", "NOT_TESTED"):
        raise ValueError(field + " must be explicitly identified")
    return value


def reference_template():
    return dict(format="WTK_OSL_REFERENCES_V1", open_fixture="UNKNOWN", short_fixture="UNKNOWN",
                references=[dict(id=f"R{r}", rref_ohms=r, nominal_impedance_ohms=[r, 0],
                                 actual_impedance_ohms=None, unit="ohm", published_tolerance_pct=None,
                                 measured_interval_ohms=None, measurement=None,
                                 temperature_assumptions="UNKNOWN", frequency_assumptions="UNKNOWN",
                                 manufacturer=None, part=None, fixture_id="UNKNOWN", notes="")
                            for r in sorted({key.rref_ohms for key in pc_osl.KEYS})])


def synthetic_inventory():
    inventory = reference_template()
    inventory.update(open_fixture="SYNTHETIC_OPEN", short_fixture="SYNTHETIC_SHORT")
    for ref in inventory["references"]:
        pc_osl.integer(ref["rref_ohms"], 10, 1000000)
        ref.update(published_tolerance_pct=1.0, fixture_id="SYNTHETIC_" + ref["id"],
                   temperature_assumptions="Synthetic 25 C; not measured",
                   frequency_assumptions="Synthetic resistance; no physical frequency qualification")
    return inventory


def references(inventory):
    if inventory["format"] != "WTK_OSL_REFERENCES_V1":
        raise ValueError("unsupported reference inventory")
    text(inventory["open_fixture"], "OPEN fixture")
    text(inventory["short_fixture"], "SHORT fixture")
    result, ids = {}, set()
    for ref in inventory["references"]:
        rid = text(ref["id"], "reference ID")
        if rid in ids or ref["rref_ohms"] in result:
            raise ValueError("duplicate reference ID/range")
        ids.add(rid)
        if ref["unit"] != "ohm":
            raise ValueError("reference unit must be ohm")
        text(ref["fixture_id"], "LOAD fixture")
        text(ref["temperature_assumptions"], "temperature assumptions")
        text(ref["frequency_assumptions"], "frequency assumptions")
        nominal = pc_osl.complex_value(ref["nominal_impedance_ohms"])
        center = pc_osl.complex_value(ref["actual_impedance_ohms"]) if ref["actual_impedance_ohms"] is not None else nominal
        tolerance = pc_osl.finite(ref["published_tolerance_pct"])
        if nominal.real < 0 or center.real < 0 or min(abs(nominal), abs(center)) <= pc_osl.MIN_SEPARATION or not 0 < tolerance < 100:
            raise ValueError("passive LOAD and explicit published tolerance required")
        # Printed tolerance is around NOMINAL. Re-centering at an entered value
        # cannot silently shrink that interval. Complex tolerance is a radial bound.
        radius = abs(nominal) * tolerance / 100 + abs(center - nominal)
        interval = ref["measured_interval_ohms"]
        if interval is not None and ref["actual_impedance_ohms"] is None:
            raise ValueError("measured interval requires an actual entered resistance")
        if ref["actual_impedance_ohms"] is not None or interval is not None:
            measurement = ref["measurement"]
            if not isinstance(measurement, dict):
                raise ValueError("entered actual value needs independent measurement provenance")
            for field in ("instrument", "operator", "date", "evidence"):
                text(measurement.get(field), "measurement " + field)
        if interval is not None:
            if center.imag or not isinstance(interval, list) or len(interval) != 2:
                raise ValueError("measured resistance interval needs two real endpoints")
            low, high = map(pc_osl.finite, interval)
            if not 0 < low < center.real < high:
                raise ValueError("actual LOAD must lie inside its nonzero measured interval")
            radius = max(center.real - low, high - center.real)
        effective = radius / abs(center) * 100
        if not 0 < effective < 100:
            raise ValueError("reference interval is unusable for existing OSL tolerance contract")
        result[ref["rref_ohms"]] = dict(id=rid, impedance_ohms=pc_osl.pair(center), tolerance_pct=effective)
    if set(result) != {key.rref_ohms for key in pc_osl.KEYS}:
        raise ValueError("inventory must cover all six RREF LOAD categories")
    return result


def plan(inventory, order="standard"):
    refs = references(inventory)
    pairs = [(key, standard) for standard in capture.STANDARDS for key in pc_osl.KEYS]
    if order == "condition":
        pairs = [(key, standard) for key in pc_osl.KEYS for standard in capture.STANDARDS]
    elif order != "standard":
        raise ValueError("unsupported capture order")
    return [dict(condition=key.as_json(), standard=standard,
                 fixture=inventory[standard.lower() + "_fixture"] if standard != "LOAD" else
                 next(r["fixture_id"] for r in inventory["references"] if r["rref_ohms"] == key.rref_ohms),
                 reference=refs[key.rref_ohms] if standard == "LOAD" else None)
            for key, standard in pairs]


def pair_id(row):
    return pc_osl.Key.from_json(row["condition"]), row["standard"]


def quality(row):
    standard = pc_osl.standard_from_capture(row)
    if not (standard.ret_1x_valid or standard.ret_hg_valid):
        raise ValueError("unobservable: both return paths unusable")
    if row["acquisition"]["hardware_error"]:
        raise ValueError("hardware acquisition error")
    vmid = sum(pc_osl.complex_value(row["phasors"][name]) for name in ("vmid_adc1", "vmid_adc2")) / 2
    return dict(source_peak_v=abs(pc_osl.complex_value(row["phasors"]["vexc_1"]) - vmid),
                return_1x_peak_v=abs(pc_osl.complex_value(row["phasors"]["ret_1x"]) - vmid),
                return_hg_peak_v=abs(pc_osl.complex_value(row["phasors"]["ret_hg"]) - vmid),
                usable_paths=[name for name, valid in (("1X", standard.ret_1x_valid), ("HG", standard.ret_hg_valid)) if valid],
                clipping_mask=row["acquisition"]["clipping_mask"],
                hg_observed=standard.hg_observed, temperature_mC=row["temperature_mC"],
                repeats=row["acquisition"]["accepted"], adc_provenance=row["adc"]["provenance"])


class Campaign:
    def __init__(self, path, document):
        self.path, self.document = Path(path), document
        self.expected_seal = digest(document) if self.path.exists() else None

    @classmethod
    def create(cls, path, inventory, order="standard"):
        if Path(path).exists():
            raise ValueError("campaign exists; use resume")
        plan(inventory, order)
        campaign = cls(path, dict(format=FORMAT, capture_format="WTK_PC_OSL_CAPTURE_V1",
                                 inventory=inventory, order=order, device=None, adc=None,
                                 successor=None, active_sequence=None, captures=[], audit=[], attempts=[],
                                 connections=[], confirmations=[], candidate=None, created_utc=utc(),
                                 physically_qualified=False))
        campaign.expected_seal = None
        campaign.save()
        return campaign

    @classmethod
    def load(cls, path):
        document = read_json(path)
        seal = document.pop("integrity_sha256", None)
        if seal != digest(document) or document["format"] != FORMAT or document["physically_qualified"] is not False:
            raise ValueError("campaign integrity/schema mismatch; preserve file and investigate")
        campaign = cls(path, document)
        campaign.verify()
        return campaign

    def save(self):
        data = dict(self.document, integrity_sha256=digest(self.document))
        encoded = (json.dumps(data, indent=2, allow_nan=False) + "\n").encode("utf-8")
        if len(encoded) > MAX_DOCUMENT:
            raise ValueError("campaign exceeds 8 MiB; archive evidence before further captures")
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with progress_lock(self.path):
            if self.path.exists():
                old = read_json(self.path)
                seal = old.pop("integrity_sha256", None)
                if seal != self.expected_seal or seal != digest(old):
                    raise ValueError("progress changed since load; reload before continuing, never overwrite evidence")
            elif self.expected_seal is not None:
                raise ValueError("progress disappeared; preserve in-memory evidence and investigate")
            atomic_write(self.path, encoded)
        self.expected_seal = data["integrity_sha256"]

    def capture_document(self):
        return dict(format=self.document["capture_format"], adc=self.document["adc"],
                    captures=self.document["captures"], evidence=self.evidence())

    def evidence(self):
        return "SIMULATED / SYNTHETIC_NOT_PHYSICALLY_QUALIFIED" if (self.document["device"] or {}).get("synthetic") else "REQUIRES_BENCH_VALIDATION"

    def verify(self):
        if self.document["capture_format"] != "WTK_PC_OSL_CAPTURE_V1":
            raise ValueError("unsupported capture document format")
        expected = {pair_id(row): row for row in plan(self.document["inventory"], self.document["order"])}
        if self.document["captures"]:
            capture.verify_campaign(self.capture_document(), self.document["device"])
        for row in self.document["captures"] + [entry["replaced"] for entry in self.document["audit"]]:
            wanted = expected[pair_id(row)]
            if row.get("reference") != wanted["reference"]:
                raise ValueError("wrong LOAD reference binding")
            capture.verify_campaign(dict(captures=[row], adc=self.document["adc"]), self.document["device"])
            quality(row)
        # Validate every already-complete triplet, even before final construction.
        for key in pc_osl.KEYS:
            group = [pc_osl.standard_from_capture(row) for row in self.document["captures"] if pc_osl.Key.from_json(row["condition"]) == key]
            if len(group) == 3:
                pc_osl.solve(group)

    def missing(self):
        done = {pair_id(row) for row in self.document["captures"]}
        return [row for row in plan(self.document["inventory"], self.document["order"]) if pair_id(row) not in done]

    def progress(self):
        counts = {}
        for row in self.document["captures"]:
            key = pc_osl.Key.from_json(row["condition"])
            counts[key] = counts.get(key, 0) + 1
        return dict(captured=len(self.document["captures"]), required=99,
                    complete_conditions=sum(value == 3 for value in counts.values()),
                    missing=self.missing(), evidence=self.evidence(), installed=False)

    def connect(self, client, physical=False, operator="SYNTHETIC_OPERATOR"):
        identity = client.identify()  # Fresh negotiation on every COM/reboot connection.
        if identity["profile"] != 1 or identity["protocol_version"] != 1 or identity["capabilities"] & 15 != 15:
            raise ValueError("BRINGUP_CAL capture/status/cancel capabilities required")
        if physical and identity["synthetic"]:
            raise ValueError("synthetic identity cannot be used as physical evidence")
        if physical and operator == "SYNTHETIC_OPERATOR":
            raise ValueError("physical campaign requires an operator identity")
        text(operator, "operator")
        if self.document["device"] is not None and self.document["device"] != identity:
            raise ValueError("campaign device/firmware/ADC identity mismatch; start a new campaign")
        status = self.safe_status(client)
        if not identity["capabilities"] & 16:
            raise ValueError("successor sequence cannot be established without installation status capability")
        installation = Installer(client).status()  # Read only; no install command is issued.
        if not installation["storage_available"] or installation["state"] in (1, 2, 3) or not installation["next_sequence"]:
            raise ValueError("storage busy/unavailable or sequence exhausted")
        binding = (installation["next_sequence"], status["calibration_sequence"])
        if self.document["successor"] is not None and binding != (self.document["successor"], self.document["active_sequence"]):
            raise ValueError("calibration sequence changed; do not replay campaign against a new active calibration")
        self.document.update(device=identity, successor=binding[0], active_sequence=binding[1])
        self.document["connections"].append(dict(utc=utc(), device=identity, status=status,
                                                installation=installation, next_request_id=client.next_id, operator=operator))
        self.save()
        return status

    def safe_status(self, client):
        if client.identity != self.document["device"] and self.document["device"] is not None:
            raise ValueError("connected identity mismatch")
        state = client.status()
        if state["busy"] or not state["transfer_safe"] or state["safety_faults"] or state["safety_blocks"] & ~8:
            raise ValueError("unsafe/busy calibration service; no capture sent: " + str(state))
        if self.document["active_sequence"] is not None and state["calibration_sequence"] != self.document["active_sequence"]:
            raise ValueError("active calibration sequence changed")
        return state

    def accept(self, row, replace=False):
        expected = next((wanted for wanted in plan(self.document["inventory"], self.document["order"]) if pair_id(wanted) == pair_id(row)), None)
        if expected is None or expected["reference"] != row.get("reference"):
            raise ValueError("wrong condition/reference")
        capture.verify_campaign(dict(captures=[row], adc=row["adc"]), self.document["device"])
        if bool(row["acquisition"]["flags"] & 512) != self.document["device"]["synthetic"]:
            raise ValueError("artifact synthetic provenance differs from device identity")
        if self.document["adc"] is not None and row["adc"] != self.document["adc"]:
            raise ValueError("ADC provenance mismatch")
        quality(row)
        old = next((item for item in self.document["captures"] if pair_id(item) == pair_id(row)), None)
        if old is not None and not replace:
            raise ValueError("duplicate capture; use explicit recapture")
        others = [item for item in self.document["captures"] if pair_id(item) != pair_id(row)]
        group = [pc_osl.standard_from_capture(item) for item in others + [row] if item["condition"] == row["condition"]]
        if len(group) == 3:
            pc_osl.solve(group)  # An unobservable LOAD cannot complete a condition.
        previous = copy.deepcopy(self.document)
        if old is not None:
            self.document["audit"].append(dict(utc=utc(), replaced=old, replacement_sha256=row["sha256"]))
        self.document.update(adc=row["adc"], captures=others + [row], candidate=None)
        try:
            self.save()
        except (OSError, ValueError):
            self.document = previous
            raise

    def run(self, client, confirm, output=print, limit=None, recapture=None):
        """Only this human-confirmed operation sends START; simulation supplies its own operator."""
        rows = self.missing()
        if recapture is not None:
            rows = [row for row in plan(self.document["inventory"], self.document["order"]) if pair_id(row) == recapture]
            if len(rows) != 1:
                raise ValueError("recapture must identify a supported condition/standard")
        for index, request in enumerate(rows):
            if limit is not None and index >= limit:
                break
            key = pc_osl.Key.from_json(request["condition"])
            description = f'{request["standard"]} fixture {request["fixture"]}; RREF {key.rref_ohms} ohm / {key.frequency_hz} Hz / {key.amplitude_mvrms} mVrms'
            if request["reference"]:
                description += f'; LOAD {request["reference"]}'
            output(description + "; passive, discharged fixture only. Confirm each capture; never change fixture while busy.")
            if not confirm(request):
                output("Paused; accepted evidence preserved.")
                return False
            self.document["confirmations"].append(dict(utc=utc(), request=request, recapture=recapture is not None,
                    operator=self.document["connections"][-1]["operator"]))
            self.save()
            raw = None
            capture_attempted = False
            try:
                if client.identify() != self.document["device"]:
                    raise ValueError("device/firmware identity changed")
                self.safe_status(client)
                capture_attempted = True
                raw = client.capture(key, request["standard"], request["reference"])
                self.accept(raw, replace=recapture is not None)
            except KeyboardInterrupt:
                # Client.capture does not catch KeyboardInterrupt; cancel its last
                # known request through existing bounded protocol, never reset hardware.
                canceled = cancel_pending(client) if capture_attempted else None
                self.document["attempts"].append(dict(utc=utc(), request=request, reason="CANCELED / transport may be ambiguous", raw=raw, cancellation=canceled))
                self.save()
                raise
            except (ValueError, TimeoutError, OSError) as exc:
                # START may have reached the device even when its reply was lost.
                # Query/cancel is bounded and does not replay acquisition.
                canceled = cancel_pending(client) if capture_attempted else None
                self.document["attempts"].append(dict(utc=utc(), request=request, reason=str(exc), raw=raw, cancellation=canceled))
                self.save()
                output("REJECTED: " + str(exc) + ". No automatic retry. Investigate and resume explicitly.")
                if canceled is not None and (not canceled["transfer_safe"] or canceled.get("busy")):
                    output("Teardown is UNKNOWN/unfinished; verify SAFE after reconnection before changing the fixture.")
                raise
            progress = self.progress()
            output(f'PASS (acquisition only): {quality(raw)}\nCaptured {progress["captured"]}/99 observations — {progress["complete_conditions"]}/33 complete conditions; {self.evidence()}')
        return not self.missing()

    def build(self, sequence=None):
        self.verify()
        if sequence is not None and sequence != self.document["successor"]:
            raise ValueError("candidate sequence must equal connected device successor")
        if self.document["successor"] is None or self.missing():
            raise ValueError("campaign incomplete or successor sequence unknown")
        frame, solutions = pc_osl.build_candidate(self.capture_document(), self.document["successor"])
        summary = validate_candidate(frame)
        self.document["candidate"] = dict(sequence=summary.sequence, sha256=hashlib.sha256(frame).hexdigest(),
                                          bytes=len(frame), conditions=len(solutions), installed=False)
        self.save()
        return frame, solutions

    def report(self):
        progress = self.progress()
        lines = ["# Guided OSL campaign", "", self.evidence(),
                 "UNQUALIFIED / REQUIRES_BENCH_VALIDATION; no installation performed.",
                 f'Captured {progress["captured"]}/99; complete conditions {progress["complete_conditions"]}/33.',
                 "Device: `" + json.dumps(self.document["device"], sort_keys=True) + "`",
                 f'Expected successor: {self.document["successor"]}; candidate: {self.document["candidate"]}',
                 "ADC: `" + json.dumps(self.document["adc"]) + "`", "",
                 "Connection/operator evidence: `" + json.dumps(self.document["connections"], sort_keys=True) + "`",
                 f'Explicit capture confirmations: {len(self.document["confirmations"])} (retained in campaign JSON).',
                 "Printed tolerance is an interval, not an exact reference or instrument error bound.",
                 "Missing observations:", *[json.dumps(row, sort_keys=True) for row in self.missing()], "",
                 "Reference inventory (includes fixture, assumptions, intervals and measurement provenance):",
                 "```json", json.dumps(self.document["inventory"], indent=2), "```", "",
                 "| Standard / RREF / Hz / mVrms | Artifact SHA-256 | Quality |",
                 "| --- | --- | --- |"]
        for row in self.document["captures"]:
            key = pc_osl.Key.from_json(row["condition"])
            lines.append(f'| {row["standard"]} / {key.rref_ohms} / {key.frequency_hz} / {key.amplitude_mvrms} | {row["sha256"]} | {quality(row)} |')
        lines += ["", "Rejected attempts: " + json.dumps(self.document["attempts"], sort_keys=True),
                  f'Replaced observations retained in audit: {len(self.document["audit"])}',
                  "Integrity hashes detect accidental editing; they do not authenticate physical fixtures or an operator."]
        return "\n".join(lines) + "\n"


def cancel_pending(client):
    try:
        state = client.status()
        if state["busy"]:
            client.cancel(state["capture_id"])
            state = client.status()
        return dict(transfer_safe=state["transfer_safe"], busy=state["busy"], capture_id=state["capture_id"])
    except (ValueError, TimeoutError, OSError) as exc:
        return dict(transfer_safe=False, status="UNKNOWN", reason=str(exc),
                    next_action="Verify safe teardown after reconnect; firmware independent timeout remains authoritative")


def write_outputs(campaign, out):
    out = Path(out)
    if out.exists():
        raise ValueError("use a new output directory; existing evidence is never overwritten")
    frame, solutions = campaign.build()
    out.mkdir(parents=True)
    atomic_write(out / "candidate.bin", frame)
    atomic_write(out / "captures.json", (json.dumps(campaign.capture_document(), indent=2) + "\n").encode())
    progress = campaign.path.read_bytes()
    atomic_write(out / "campaign.json", progress)
    atomic_write(out / "report.md", (campaign.report() + "\n" + pc_osl.report(campaign.capture_document(), frame, solutions)).encode())
    atomic_write(out / "report.json", (json.dumps(dict(progress=campaign.progress(), device=campaign.document["device"],
                 candidate=campaign.document["candidate"], inventory=campaign.document["inventory"],
                 campaign_sha256=hashlib.sha256(progress).hexdigest(), provenance_file="campaign.json",
                 qualification="REQUIRES_BENCH_VALIDATION", physically_qualified=False), indent=2) + "\n").encode())
    return campaign.document["candidate"]


def simulate(bridge, directory):
    directory = Path(directory)
    if directory.exists():
        raise ValueError("simulation needs a new directory")
    path = directory / "campaign.json"
    campaign = Campaign.create(path, synthetic_inventory())
    transport = capture.FakeSerial(bridge)
    try:
        client = capture.Client(transport)
        if campaign.connect(client)["calibrated"]:
            raise ValueError("synthetic fixture must be blank")
        campaign.run(client, lambda request: True, output=lambda message: None, limit=42)
        hashes = [row["sha256"] for row in campaign.document["captures"]]
        transport.control(4, struct.pack("<H", 0))  # C-fixture reset only; not a PLC1 command.
        transport.pending.clear()
        campaign = Campaign.load(path)
        campaign.connect(capture.Client(transport))
        client = capture.Client(transport)  # A second reconnect also negotiates request IDs.
        campaign.connect(client)
        campaign.run(client, lambda request: True, output=lambda message: None)
        if [row["sha256"] for row in campaign.document["captures"][:42]] != hashes:
            raise ValueError("interruption lost accepted evidence")
        result = write_outputs(campaign, directory / "output")
        return dict(result, captures=99, resumed_after=42, complete_conditions=33,
                    evidence=campaign.evidence(), physically_qualified=False,
                    active_calibration_unchanged=not client.status()["calibrated"])
    finally:
        transport.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("references", "plan", "start", "resume", "status", "inspect", "build", "report", "simulate"))
    parser.add_argument("--campaign", type=Path)
    parser.add_argument("--inventory", type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--order", choices=("standard", "condition"), default="standard")
    parser.add_argument("--port")
    parser.add_argument("--operator", help="required for physical start/resume")
    parser.add_argument("--bridge", type=Path)
    parser.add_argument("--sequence", type=int)
    parser.add_argument("--recapture", nargs=4, metavar=("RREF", "HZ", "MVRMS", "STANDARD"))
    args = parser.parse_args(argv)
    try:
        if args.bridge and args.action != "simulate":
            raise ValueError("--bridge is test-only and requires simulate; physical start/resume cannot use it")
        if args.action == "references":
            if args.inventory:
                print(json.dumps(references(read_json(args.inventory)), indent=2))
            elif args.out:
                if args.out.exists():
                    raise ValueError("inventory exists; edit it explicitly")
                atomic_write(args.out, (json.dumps(reference_template(), indent=2) + "\n").encode())
            else:
                raise ValueError("--out for editable template or --inventory for validation")
            return 0
        if args.action == "simulate":
            if not args.bridge or not args.out:
                raise ValueError("--bridge and --out required for C-backed simulation")
            print(json.dumps(simulate(args.bridge, args.out), indent=2))
            return 0
        if args.action == "plan":
            if not args.inventory:
                raise ValueError("--inventory required")
            print(json.dumps(plan(read_json(args.inventory), args.order), indent=2))
            return 0
        if not args.campaign:
            raise ValueError("--campaign required")
        if args.action == "start":
            if not args.inventory:
                raise ValueError("--inventory required")
            campaign = Campaign.create(args.campaign, read_json(args.inventory), args.order)
        else:
            campaign = Campaign.load(args.campaign)
        if args.action == "status":
            print(json.dumps(campaign.progress(), indent=2))
        elif args.action == "inspect":
            print(json.dumps(dict(progress=campaign.progress(), device=campaign.document["device"],
                adc=campaign.document["adc"], inventory=campaign.document["inventory"],
                observations=[dict(condition=row["condition"], standard=row["standard"], sha256=row["sha256"],
                                   quality=quality(row)) for row in campaign.document["captures"]],
                rejected=campaign.document["attempts"], replaced=campaign.document["audit"]), indent=2))
        elif args.action == "report":
            if not args.out or args.out.exists():
                raise ValueError("--out must name a new Markdown report")
            atomic_write(args.out, campaign.report().encode())
        elif args.action == "build":
            if not args.out:
                raise ValueError("--out required")
            if args.sequence is not None and args.sequence != campaign.document["successor"]:
                raise ValueError("wrong candidate sequence")
            print(json.dumps(write_outputs(campaign, args.out), indent=2))
            print(f'Next SEPARATE installation: python Firmware/tools/pc_osl_install.py install --port COM5 --candidate "{args.out / "candidate.bin"}" --report "{args.out / "installation.json"}" --readback "{args.out / "installed.bin"}"')
            print("Query installer status again before installation; a changed sequence requires a new campaign.")
        else:
            if not args.port:
                raise ValueError("select --port COM5 (use pc_cal_capture.py ports for discovery)")
            text(args.operator, "--operator")
            import serial
            transport = serial.Serial(port=None, baudrate=115200, timeout=0.05, write_timeout=1)
            transport.dtr = False
            transport.rts = False
            transport.port = args.port
            try:
                transport.open()
                client = capture.Client(transport)
                state = campaign.connect(client, physical=True, operator=args.operator)
                print("PHYSICAL acquisition / UNQUALIFIED; " + ("CALIBRATED record present" if state["calibrated"] else "UNCALIBRATED device"))
                print("Earlier A05/B01 electrical gates must be complete. No energized DUT; no automatic retry or installation.")
                recapture = None
                if args.recapture:
                    recapture = (pc_osl.Key(*map(int, args.recapture[:3])), args.recapture[3])
                complete = campaign.run(client, lambda request: input("Type CAPTURE to confirm this exact passive fixture and acquire (anything else pauses): ").strip() == "CAPTURE", recapture=recapture)
                if complete:
                    out = args.out or args.campaign.parent / (args.campaign.stem + "-candidate-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ"))
                    print(json.dumps(write_outputs(campaign, out), indent=2))
                    print(f'Candidate ready, UNQUALIFIED. Installation is separate: python Firmware/tools/pc_osl_install.py install --port {args.port} --candidate "{out / "candidate.bin"}" --report "{out / "installation.json"}" --readback "{out / "installed.bin"}"')
            finally:
                transport.close()
        return 0
    except KeyboardInterrupt:
        print("Canceled; accepted evidence preserved. Verify safe status before explicit resume.")
        return 2
    except (ValueError, KeyError, TypeError, OSError, TimeoutError) as exc:
        print("REJECTED: " + str(exc))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
