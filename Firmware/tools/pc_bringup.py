#!/usr/bin/env python3
"""Read-only Rev.1 BRINGUP diagnostics. No active commands or hardware resets.

The text console has no request IDs or prompt. A fresh passive boot banner is
required, commands are serialized, and ambiguous/late responses stop the session.
This tool records evidence; it never grants a measurement permit or qualification.
"""
from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import time

COMMANDS = ("lab fault status", "lab charger status", "lab sensors status",
            "lab safety status", "lab range status", "lab adc status",
            "lab flash info", "lab mvp status", "lab cal status")
STATES = ("PASS", "STOP", "WARNING", "NOT_TESTED", "UNKNOWN")
MAX_LINE = 256
MAX_RX = 65536
MAX_EVENTS = 4096
MAX_FILE = 2 * 1024 * 1024
FIXTURE = Path(__file__).with_name("fixtures") / "bringup_rev1.json"

# Acceptance windows are A05 Stage 2 engineering gates, not accuracy claims.
# Other checks require an operator's documented interpretation, never nominal data.
MANUAL = {
    "assembly": ("Stage 1 complete: population, DNP, orientation, continuity", "inspection", None),
    "R02": ("R_TFT_LED open; PB0 load unknown until independently verified", "inspection", None),
    "R05": ("Bluepill 40-pin alignment, 5V/3V3/GND/VBAT/SWD/regulator/HSE/PA11/12", "inspection", None),
    "power": ("Stage 2 current-limited power and sequencing reviewed", "inspection", None),
    "safe_boot": ("PB9/PB8/PA8 inactive at boot/reset, K1 NC contacts verified", "oscilloscope", None),
    "uart_cable": ("3.3V UART: GND pin1, adapter RX to pin2, TX to pin3, no power/reset leads", "inspection", None),
    "j_pwr_5v": ("J_PWR.2 +5V_SYS / J_PWR.3 GND", "DMM", (4.75, 5.25)),
    "5v_a": ("COUT1.1 +5V_A / GND", "DMM", (4.75, 5.25)),
    "3v3": ("UFLASH.8 +3V3 / UFLASH.4 GND", "DMM", (3.0, 3.6)),
    "vmid": ("VMID U4.2 / GND; guard window, not scale qualification", "DMM", (1.35, 1.95)),
    "supply_current": ("J_PWR supply current, current limit and fitted loads recorded", "DMM", None),
    "pb9_k1": ("PB9/K1_BASE low and de-energized NC contact continuity", "oscilloscope", None),
    "pb8_range": ("PB8/U2.6 RANGE_EN inactive, all VGS off", "oscilloscope", None),
    "pa8_excitation": ("PA8 inactive without any excitation command", "oscilloscope", None),
    "pa15_charger": ("PA15/USTM32.10 vs J_PWR.4; identify VBUS wiring first", "DMM", None),
    "w25q_supply": ("UFLASH.8 supply / .4 GND and CS/WP/HOLD idle", "DMM", (3.0, 3.6)),
    "powerup_waveforms": ("A05-Y04: +5V_A and +3V3 simultaneous power-up traces", "oscilloscope", None),
    "powerdown_waveforms": ("A05-Y04: +5V_A and +3V3 simultaneous power-down traces", "oscilloscope", None),
    "signal_backfeed": ("A05-Y04/Y05: rail rise with regulator inactive; reviewed signal attachment only", "DMM", None),
    "analog_adc_voltages": ("A05-Y04: paired U4/U5 output and corresponding ADC input / GND", "DMM", None),
    "bat54s_conduction": ("A05-Y04: later specifically authorized capture only; authorization and trace required", "oscilloscope", None),
    "3v3_stability": ("A05-Y04: rail stability across documented tested conditions", "oscilloscope", None),
    "buzzer": ("Purchased 5V buzzer type/current/drive/flyback compatibility", "inspection", None),
    "modules": ("Exact charger/protection/boost/cell identities, wiring and ratings", "inspection", None),
}
SERIAL_GATES = ("assembly", "R02", "R05", "power", "safe_boot", "uart_cable",
                "j_pwr_5v", "5v_a", "3v3", "w25q_supply", "modules")


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def load_json(path):
    path = Path(path)
    if path.stat().st_size > MAX_FILE:
        raise ValueError("evidence file exceeds 2 MiB bound")
    return json.loads(path.read_text(encoding="utf-8"),
                      parse_constant=lambda x: (_ for _ in ()).throw(ValueError("nonfinite JSON")))


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, ensure_ascii=True, allow_nan=False) + "\n", encoding="utf-8")


def template():
    return {"schema": 1, "created_utc": utc_now(), "board": {
        "revision": "Rev1", "fabrication_identity": "UNKNOWN", "photographs": [],
        "population_notes": "UNKNOWN", "dnp_verification": "NOT_TESTED"},
        "firmware": {"expected_sha": "UNKNOWN", "profile": "UNKNOWN"},
        "modules": {name: "UNKNOWN" for name in ("bluepill", "tft", "charger", "protection", "boost", "battery", "buzzer")},
        "instruments": {"DMM": "UNKNOWN", "oscilloscope": "Hantek DSO2C10 (settings/probes UNKNOWN)",
                        "serial_adapter": "UNKNOWN"},
        "manual_history": [],
        "manual": {key: {"status": "NOT_TESTED", "point": spec[0], "method": spec[1],
                          "value": None, "unit": None, "instrument": "UNKNOWN", "operator": "UNKNOWN",
                          "timestamp_utc": None, "evidence": [], "notes": "", "simulated": False,
                          "authorization_reference": "NOT_AUTHORIZED" if key == "bat54s_conduction" else None}
                   for key, spec in MANUAL.items()}, "notes": [], "pending_or_rejected": []}


def validate_session(session):
    if not isinstance(session, dict) or session.get("schema") != 1 or not isinstance(session.get("manual"), dict):
        raise ValueError("unsupported session schema")
    for name in ("board", "firmware", "modules", "instruments"):
        if not isinstance(session.get(name), dict):
            raise ValueError("missing session metadata: " + name)
    if not isinstance(session["board"].get("photographs"), list) or not isinstance(session.get("notes"), list) or not isinstance(session.get("pending_or_rejected"), list):
        raise ValueError("invalid evidence/notes lists")
    if session.get("firmware", {}).get("profile") not in ("UNKNOWN", "BRINGUP", "PRODUCT", "BRINGUP_CAL"):
        raise ValueError("unknown declared firmware profile")
    sha = session["firmware"].get("expected_sha")
    if sha != "UNKNOWN" and (not isinstance(sha, str) or re.fullmatch(r"[0-9a-f]{12,40}", sha) is None):
        raise ValueError("invalid expected firmware SHA")
    if session["manual"].keys() != MANUAL.keys():
        raise ValueError("manual check set differs from schema")
    for key, spec in MANUAL.items():
        entry = session["manual"].get(key)
        if not isinstance(entry, dict) or entry.get("status") not in STATES:
            raise ValueError("missing/invalid manual check: " + key)
        if not isinstance(entry.get("evidence"), list) or any(not isinstance(path, str) or not path for path in entry["evidence"]) or type(entry.get("simulated")) is not bool:
            raise ValueError("invalid evidence provenance: " + key)
        value = entry.get("value")
        if isinstance(value, float) and not math.isfinite(value):
            raise ValueError("nonfinite measurement")
        if entry["status"] == "PASS":
            if key == "bat54s_conduction" and entry.get("authorization_reference") in (None, "", "UNKNOWN", "NOT_AUTHORIZED"):
                raise ValueError("later clamp capture needs explicit authorization reference")
            if (entry.get("method") != spec[1] or not entry.get("evidence") or
                    not entry.get("notes") or not entry.get("timestamp_utc") or
                    entry.get("operator") in (None, "", "UNKNOWN") or
                    entry.get("instrument") in (None, "", "UNKNOWN")):
                raise ValueError("PASS needs method, instrument, operator, dated evidence and rationale: " + key)
            if spec[2] is not None and (type(value) not in (int, float) or
                    entry.get("unit") != "V" or not spec[2][0] <= value <= spec[2][1]):
                raise ValueError("PASS violates A05 voltage gate: " + key)
    return session


def require_serial_gates(session):
    validate_session(session)
    if session["firmware"]["profile"] != "BRINGUP":
        raise ValueError("text diagnostics require declared BRINGUP; PRODUCT/BRINGUP_CAL/UNKNOWN refused")
    if any(entry["status"] == "STOP" for entry in session["manual"].values()):
        raise ValueError("manual STOP outstanding; resolve before serial snapshot")
    for key in SERIAL_GATES:
        if session["manual"][key]["status"] != "PASS" or session["manual"][key].get("simulated") is not False:
            raise ValueError("real physical evidence gate outstanding: " + key)


def command_bytes(command):
    if command not in COMMANDS:
        raise ValueError("command is outside the immutable read-only allowlist")
    return command.encode("ascii") + b"\n"


class ProtocolError(ValueError):
    pass


class ResetError(ProtocolError):
    pass


class Reader:
    def __init__(self, transport, events, clock=time.monotonic, stamp=utc_now):
        self.transport, self.events, self.clock, self.stamp = transport, events, clock, stamp
        self.pending = bytearray()
        self.rx_bytes = 0

    def event(self, kind, **fields):
        if len(self.events) >= (MAX_EVENTS if kind in ("ERROR", "END") else MAX_EVENTS - 2):
            raise ProtocolError("event bound exceeded")
        self.events.append(dict(kind=kind, timestamp_utc=self.stamp(), monotonic_s=self.clock(), **fields))

    def line(self, deadline):
        while self.clock() < deadline:
            for i, byte in enumerate(self.pending):
                if byte in (10, 13):
                    raw = bytes(self.pending[:i]); del self.pending[:i+1]
                    if len(raw) > MAX_LINE:
                        raise ProtocolError("line exceeds 256-byte bound")
                    try:
                        line = raw.decode("ascii")
                    except UnicodeError as exc:
                        raise ProtocolError("non-ASCII text / wrong firmware mode") from exc
                    if any(ord(c) < 32 or ord(c) > 126 for c in line):
                        raise ProtocolError("non-text serial byte")
                    if line:
                        return line
                    break
            else:
                if len(self.pending) > MAX_LINE:
                    raise ProtocolError("unterminated line exceeds bound")
                data = self.transport.read(128)
                if data:
                    self.event("RX", hex=data.hex())
                    self.rx_bytes += len(data)
                    if self.rx_bytes > MAX_RX:
                        raise ProtocolError("session RX exceeds 64 KiB")
                    self.pending.extend(data)
                continue
        raise TimeoutError("no complete response within deadline")


def identity_from_lines(lines):
    if "WTK.RLCMeter" not in lines:
        raise ProtocolError("no detailed WTK boot banner; no commands sent")
    fields = {}
    for line in lines[lines.index("WTK.RLCMeter")+1:]:
        if ": " in line:
            key, value = line.split(": ", 1)
            if key in fields:
                raise ProtocolError("duplicate boot identity")
            fields[key] = value
    required = {"firmware", "git", "build", "profile", "hardware", "reset", "clock_status",
                "clock_source", "sysclk_hz", "hclk_hz", "pclk1_hz", "pclk2_hz", "adc_hz",
                "swd", "boot_state", "watchdog_policy"}
    if not required <= fields.keys() or fields["profile"] != "BRINGUP":
        raise ProtocolError("missing identity or unsupported text target (requires BRINGUP)")
    if (not re.fullmatch(r"[0-9a-f]{12,40}", fields["git"]) or fields["boot_state"] != "SAFE_BOOT" or
            fields["hardware"] != "Rev1-STM32F103C8T6-BluePill" or fields["firmware"] != "0.1.0"):
        raise ProtocolError("invalid firmware identity / boot state")
    for key in ("sysclk_hz", "hclk_hz", "pclk1_hz", "pclk2_hz", "adc_hz"):
        if not fields[key].isdigit():
            raise ProtocolError("invalid boot clock field")
        integer(fields[key])
    return fields


def match(pattern, line):
    found = re.fullmatch(pattern, line)
    if found is None:
        raise ProtocolError("malformed diagnostic line: " + line)
    return found.groups()


def enum(value, choices):
    if value not in choices:
        raise ProtocolError("unknown state: " + value)
    return value


def integer(value, maximum=0xffffffff):
    result = int(value, 16 if value.startswith("0x") else 10)
    if not 0 <= result <= maximum:
        raise ProtocolError("out of bounds integer")
    return result


def kv(line, prefix, keys):
    if not line.startswith(prefix):
        raise ProtocolError("unexpected field")
    result = {}
    for item in line[len(prefix):].split():
        if "=" not in item:
            raise ProtocolError("malformed field")
        key, value = item.split("=", 1)
        if key in result or key not in keys or not value:
            raise ProtocolError("duplicate/unknown field")
        result[key] = value
    if not keys <= result.keys():
        raise ProtocolError("missing field")
    return result


def complete(command, lines):
    if not lines:
        return False
    if command == "lab sensors status":
        return lines[-1].startswith("lab sensors ntc:") or lines[-1] == "lab sensors: INVALID"
    if command == "lab mvp status":
        return lines[-1] == "MVP_END"
    if command == "lab cal status":
        return lines[-1] == "CAL_SLOT unavailable" or lines[-1].startswith("CAL_S B ")
    return True


PREFIXES = ("lab fault:", "charger:", "lab sensors", "lab safety:", "lab range:",
            "lab adc:", "lab flash info:", "MVP_BEGIN", "CAL sch=")
STARTUP_KEYS = ("k1", "k2", "range", "charger_init", "adc", "aux_sensors", "charger", "k2_topology",
                "safety_faults", "spi2", "backlight", "backlight_pwm_hz", "buzzer", "w25q", "calibration",
                "calibration_state", "w25q_part", "w25q_jedec", "w25q_capacity", "w25q_test_sector",
                "safety_state", "safety_block", "display", "fallback_ui")


def asynchronous(line):
    return (re.fullmatch(r"\[\d+\] .+", line) is not None or
            re.fullmatch(r"(?:safety_state|safety_block|display|fallback_ui|button): [A-Z0-9_ ]+", line) is not None)


def startup_line(reader, line):
    if "WTK.RLCMeter" in line or line.startswith("profile:"):
        raise ResetError("unexpected reset during peripheral startup")
    key, separator, value = line.partition(": ")
    if separator and key in STARTUP_KEYS:
        reader.event("STARTUP", key=key, value=value)
        return key
    if asynchronous(line):
        reader.event("INTERLEAVED", line=line)
        return None
    raise ProtocolError("unexpected startup output: " + line)


def parse_response(command, lines):
    """Strict whole-response grammar; an incomplete or unknown format cannot PASS."""
    if not complete(command, lines):
        raise ProtocolError("incomplete response")
    if command == COMMANDS[0]:
        return {"fault_mask": integer(match(r"lab fault: (0x[0-9A-F]{8})", lines[0])[0])}
    if command == COMMANDS[1]:
        return {"charger": enum(match(r"charger: (\w+)", lines[0])[0], ("ABSENT", "PRESENT", "UNKNOWN"))}
    if command == COMMANDS[2]:
        if lines == ["lab sensors: INVALID"]:
            return {"invalid": True}
        if len(lines) != 10:
            raise ProtocolError("sensor field count")
        raw, valid = match(r"lab sensors vmid_raw: (\d+) valid=([01])", lines[0])
        result = {"vmid_raw": integer(raw, 4095), "vmid_valid": valid == "1", "source": "MCU_ADC_NOMINAL_3V3_SCALE"}
        for pos, name in ((1, "vmid"), (3, "safe_hi"), (4, "safe_lo"), (5, "residual_diff"), (7, "battery")):
            result[name+"_mv"] = int(match(r"lab sensors " + name + r": (-?\d+)mV", lines[pos])[0])
        hi, lo = match(r"lab sensors ov_raw: hi=(\d+) lo=(\d+)", lines[2])
        result.update(ov_hi_raw=integer(hi, 4095), ov_lo_raw=integer(lo, 4095))
        state, count, age = match(r"lab sensors residual_state: (\w+) safe_count=(\d+) age_ms=(\d+)", lines[6])
        result.update(residual=enum(state, ("SAFE", "UNSAFE", "UNKNOWN", "SATURATED")),
                      safe_count=integer(count, 255), residual_age_ms=integer(age))
        state, raw, age = match(r"lab sensors battery_state: (\w+) raw=(\d+) age_ms=(\d+)", lines[8])
        result.update(battery=enum(state, ("OK", "LOW", "CRITICAL", "UNKNOWN")), battery_raw=integer(raw, 4095), battery_age_ms=integer(age))
        raw, mv, resistance, valid, temperature, temperature_valid, age = match(
            r"lab sensors ntc: raw=(\d+) mv=(-?\d+) resistance_ohm=(\d+) valid=([01]) ntc_temperature_mC=(-?\d+) ntc_temperature_valid=([01]) age_ms=(\d+)", lines[9])
        result.update(ntc_raw=integer(raw, 4095), ntc_mv=int(mv), ntc_resistance_ohm=integer(resistance),
                      ntc_valid=valid == "1", ntc_temperature_mc=int(temperature),
                      ntc_temperature_valid=temperature_valid == "1", ntc_age_ms=integer(age))
        for key in ("vmid_mv", "safe_hi_mv", "safe_lo_mv", "residual_diff_mv", "battery_mv", "ntc_mv", "ntc_temperature_mc"):
            if not -2147483648 <= result[key] <= 2147483647:
                raise ProtocolError("sensor number out of int32 range")
        return result
    if command == COMMANDS[3]:
        blocker, flags, allowed = match(r"lab safety: (\w+) flags=(\d+) allowed=([01])", lines[0])
        return {"blocker": enum(blocker, ("MEASURE_ALLOWED", "BLOCKED_FAULT", "BLOCKED_CHARGER", "BLOCKED_SENSOR_INVALID",
                                         "BLOCKED_RESIDUAL", "BLOCKED_SUPPLY", "BLOCKED_RANGE")),
                "flags": integer(flags), "allowed": allowed == "1"}
    if command == COMMANDS[4]:
        state, requested, current = match(r"lab range: (\w+) requested=(\w+) current=(\w+)", lines[0])
        ranges = ("10R", "100R", "1K", "10K", "100K", "1M", "INVALID")
        return {"state": enum(state, ("DISABLED", "DEAD_TIME", "SETTLING", "READY", "INVALID")),
                "requested": enum(requested, ranges), "current": enum(current, ranges)}
    if command == COMMANDS[5]:
        state, channel, last = match(r"lab adc: (\w+) channel=(\w+) last=(\w+)", lines[0])
        return {"state": enum(state, ("IDLE", "BUSY")), "channel": enum(channel, ("VMID", "OV_HI", "OV_LO", "BAT", "NTC", "INVALID")),
                "last": enum(last, ("OK", "BUSY", "TIMEOUT", "ERROR", "INVALID_ARG", "NOT_SUPPORTED"))}
    if command == COMMANDS[6]:
        if lines == ["lab flash info: NOT_DETECTED"]:
            return {"detected": False}
        part, jedec, capacity = match(r"lab flash info: (\S+) (0x[0-9A-F]{8}) (\d+)", lines[0])
        return {"detected": True, "part": part, "jedec": integer(jedec), "capacity_bytes": integer(capacity)}
    if command == COMMANDS[7]:
        if len(lines) != 13 or lines[0] != "MVP_BEGIN":
            raise ProtocolError("MVP framing / count")
        result = {}
        for line in lines[1:-1]:
            for item in line.split():
                key, sep, value = item.partition("=")
                if not sep:
                    raise ProtocolError("MVP field syntax")
                # Safety/current keys recur in distinct rows; keep qualified row keys.
                key = line.split("=", 1)[0] + "." + key
                if key in result:
                    raise ProtocolError("duplicate MVP field")
                result[key] = value
        required = {"clock.clock", "clock.sysclk_hz", "flash.flash", "display.display", "charger.charger",
                    "safety.safety", "safety.blocker", "safety.faults", "k1.k1", "range_state.range_state",
                    "range_state.current", "range_state.safety", "calibration.calibration", "calibration.active",
                    "calibration.sequence", "residual.residual", "residual.battery", "resource.resource", "last_metrology.last_metrology"}
        if not required <= result.keys() or result["resource.resource"] != "BRINGUP_NA":
            raise ProtocolError("MVP missing / incompatible profile fields")
        allowed_keys = required | {"flash.jedec"}
        if result.keys() - allowed_keys:
            raise ProtocolError("unexpected MVP fields")
        for key, choices in (("k1.k1", ("SAFE", "MEASURE", "UNKNOWN")),
                             ("charger.charger", ("ABSENT", "PRESENT", "UNKNOWN")),
                             ("flash.flash", ("DETECTED", "NOT_DETECTED")),
                             ("display.display", ("READY", "NOT_READY")),
                             ("safety.safety", ("MEASURE_ALLOWED", "BLOCKED")),
                             ("range_state.range_state", ("DISABLED", "READY", "INVALID", "DEAD_TIME", "SETTLING")),
                             ("range_state.safety", ("DISABLED", "READY", "TRANSITIONING", "INVALID")),
                             ("residual.residual", ("SAFE", "UNSAFE", "UNKNOWN", "SATURATED")),
                             ("residual.battery", ("OK", "LOW", "CRITICAL", "UNKNOWN")),
                             ("calibration.active", ("0", "1"))):
            enum(result[key], choices)
        for key in ("clock.sysclk_hz", "safety.faults", "calibration.sequence"):
            integer(result[key])
        enum(result["clock.clock"], ("HSE_PLL", "HSI", "UNKNOWN"))
        enum(result["calibration.calibration"], ("UNINITIALIZED", "READY", "STORAGE_UNAVAILABLE", "NO_VALID_CALIBRATION", "ACTIVE_VALID",
                                               "WORKFLOW_ACTIVE", "STORE_BUSY", "CANDIDATE_DIRTY", "ERROR"))
        enum(result["range_state.current"], ("INVALID", "10R", "100R", "1K", "10K", "100K", "1M"))
        enum(result["safety.blocker"], ("MEASURE_ALLOWED", "BLOCKED_FAULT", "BLOCKED_CHARGER", "BLOCKED_SENSOR_INVALID",
                                      "BLOCKED_RESIDUAL", "BLOCKED_SUPPLY", "BLOCKED_RANGE"))
        return result
    if command == COMMANDS[8]:
        if len(lines) not in (4, 7):
            raise ProtocolError("CAL response structure")
        fields = set(("sch", "model", "hw", "svc", "act"))
        if " act=1" in lines[0]:
            fields |= {"slot", "seq", "rec"}
        result = kv(lines[0], "CAL ", fields)
        for key in ("sch", "model", "hw", "act"):
            result[key] = integer(result[key])
        if result["act"] not in (0, 1):
            raise ProtocolError("invalid active flag")
        enum(result["svc"], ("UNINITIALIZED", "READY", "STORAGE_UNAVAILABLE", "NO_VALID_CALIBRATION", "ACTIVE_VALID",
                             "WORKFLOW_ACTIVE", "STORE_BUSY", "CANDIDATE_DIRTY", "ERROR"))
        for value in kv(lines[1], "CALST ", {"max", "ctx", "rt", "svc", "lab"}).values():
            integer(value)
        workflow = kv(lines[2], "CALWF ", {"st", "res"})
        enum(workflow["st"], ("IDLE", "CAPTURE_REQUESTED", "WAIT_CAPTURE", "COMPLETE", "FAILED", "CANCELING", "CANCELED"))
        enum(workflow["res"], ("NONE", "OK", "INVALID_REQUEST", "UNSUPPORTED_CONDITION", "UNSTABLE", "TOO_MANY_REJECTS", "PHASE05_ERROR", "SAFETY_ABORT", "CANCELED"))
        if result["act"]:
            enum(result["slot"], ("A", "B"))
            integer(result["seq"]); integer(result["rec"], 33)
        if lines[-1] == "CAL_SLOT unavailable":
            if len(lines) != 4:
                raise ProtocolError("CAL response structure")
        else:
            if len(lines) != 7:
                raise ProtocolError("CAL response structure")
            for pos, slot in ((3, "A"), (4, "B")):
                for value in kv(lines[pos], "CAL_SLOT " + slot + " ", {"start", "size"}).values():
                    integer(value)
            for pos, slot in ((5, "A"), (6, "B")):
                info = kv(lines[pos], "CAL_S " + slot + " ", {"fr", "seq", "sch", "hw", "model", "val", "fl", "rec"})
                enum(info["fr"], ("VALID", "INVALID"))
                enum(info["val"], ("VALID", "MISSING", "CORRUPT", "INCOMPATIBLE_SCHEMA", "INCOMPATIBLE_HARDWARE", "INCOMPATIBLE_MODEL", "INCOMPLETE"))
                for key in ("seq", "sch", "hw", "model", "fl", "rec"):
                    integer(info[key])
        return result
    raise ProtocolError("unknown command")


def collect(transport, session, simulated=False, timeout=3.0, boot_timeout=15.0,
            clock=time.monotonic, stamp=utc_now):
    if not math.isfinite(timeout) or not 0.01 <= timeout <= 60 or not math.isfinite(boot_timeout) or not 0.01 <= boot_timeout <= 60:
        raise ValueError("timeouts must be finite, positive and at most 60 seconds")
    validate_session(session)
    if not simulated:
        require_serial_gates(session)
    if session["firmware"]["profile"] != "BRINGUP":
        raise ValueError("declare BRINGUP; binary calibration and PRODUCT targets refused")
    events = []
    reader = Reader(transport, events, clock, stamp)
    reader.event("BEGIN", simulated=simulated, session_sha256=hashlib.sha256(json.dumps(session, sort_keys=True).encode()).hexdigest())
    try:
        banner = []
        deadline = clock() + boot_timeout
        for _ in range(80):
            line = reader.line(deadline)
            banner.append(line)
            if line.startswith("watchdog_policy:"):
                break
        identity = identity_from_lines(banner)
        expected = session["firmware"].get("expected_sha", "UNKNOWN")
        if expected != "UNKNOWN" and not (re.fullmatch(r"[0-9a-f]{12,40}", expected or "") and expected.startswith(identity["git"])):
            raise ProtocolError("firmware SHA differs from session expectation")
        reader.event("IDENTITY", fields=identity)
        # The banner precedes app_shell peripheral initialization. Its first
        # safety_block diagnostic proves entry to app_step, after console init.
        for _ in range(80):
            if startup_line(reader, reader.line(deadline)) == "safety_block":
                break
        else:
            raise ProtocolError("no console-loop startup marker")
        # Passive one-second settling window lets initial auxiliary sweeps and
        # display startup finish. It creates no permission or readiness claim.
        settle_deadline = min(deadline, clock()+1.0)
        try:
            for _ in range(80):
                startup_line(reader, reader.line(settle_deadline))
            raise ProtocolError("startup output bound exceeded")
        except TimeoutError:
            pass
        for command, prefix in zip(COMMANDS, PREFIXES):
            packet = command_bytes(command)
            reader.event("TX", command=command, hex=packet.hex())
            if transport.write(packet) != len(packet):
                raise ProtocolError("partial command write; no retries")
            lines = []
            deadline = clock() + timeout
            for _ in range(80):
                line = reader.line(deadline)
                if "WTK.RLCMeter" in line or line.startswith("profile:"):
                    raise ResetError("unexpected reset; snapshot invalidated")
                if asynchronous(line):
                    reader.event("INTERLEAVED", line=line)
                    continue
                if not lines and not line.startswith(prefix):
                    raise ProtocolError("unexpected/stale output: " + line)
                lines.append(line)
                if complete(command, lines):
                    values = parse_response(command, lines)
                    reader.event("RESPONSE", command=command, lines=lines, values=values)
                    break
            else:
                raise ProtocolError("response exceeds line bound")
        # Include late output after the final command; a duplicate final response
        # must not turn an ambiguous session into success. No additional TX.
        deadline = clock() + 0.2
        try:
            for _ in range(20):
                line = reader.line(deadline)
                if "WTK.RLCMeter" in line:
                    raise ResetError("unexpected reset after final response")
                if not asynchronous(line):
                    raise ProtocolError("unexpected trailing/stale output: " + line)
                reader.event("INTERLEAVED", line=line)
            raise ProtocolError("trailing log bound exceeded")
        except TimeoutError:
            pass
    except (OSError, ValueError, TimeoutError) as exc:
        reader.event("ERROR", status="STOP" if isinstance(exc, ResetError) or (isinstance(exc, OSError) and not isinstance(exc, TimeoutError)) else "UNKNOWN", reason=str(exc))
    reader.event("END")
    return events


class FakeSerial:
    """Source-derived synthetic text, injectable packet splits/delay/disconnection."""
    def __init__(self, fixture=None, chunk=17, delay_reads=0):
        self.fixture = copy.deepcopy(fixture or load_json(FIXTURE))
        self.pending = bytearray((self.fixture["banner"]+self.fixture.get("startup", "")).encode("ascii"))
        self.chunk, self.delay_reads, self.wait_reads = chunk, delay_reads, 0
        self.writes = []
        self.now = 0.0

    def read(self, size):
        self.now += 0.01
        if self.wait_reads:
            self.wait_reads -= 1
            return b""
        data = bytes(self.pending[:min(size, self.chunk)])
        del self.pending[:len(data)]
        return data

    def write(self, data):
        command = data.decode("ascii").rstrip("\n")
        if data != command_bytes(command):
            raise ValueError("fake received non-allowlisted packet")
        self.writes.append(data)
        self.pending.extend(self.fixture["responses"].get(command, "").encode("ascii"))
        self.wait_reads = self.delay_reads
        return len(data)


def replay(events):
    """Re-run the collector through recorded RX/TX bytes, not saved parsed values."""
    if not isinstance(events, list) or not events or len(events) > MAX_EVENTS:
        raise ValueError("invalid bounded event transcript")
    for event in events:
        if not isinstance(event, dict) or event.get("kind") not in ("BEGIN", "RX", "TX", "IDENTITY", "STARTUP", "RESPONSE", "INTERLEAVED", "ERROR", "END"):
            raise ValueError("invalid transcript event")
        if event["kind"] == "ERROR" and (event.get("status") not in ("STOP", "UNKNOWN") or not isinstance(event.get("reason"), str)):
            raise ValueError("invalid original failure status")
        if event["kind"] in ("RX", "TX"):
            raw = event.get("hex")
            if not isinstance(raw, str) or len(raw) > 256 or re.fullmatch(r"(?:[0-9a-fA-F]{2})*", raw) is None:
                raise ValueError("invalid bounded transcript packet")
    wire = [event for event in events if event.get("kind") in ("RX", "TX", "END")]

    class ReplaySerial:
        def __init__(self):
            self.index, self.pending, self.now = 0, bytearray(), 0.0

        def read(self, size):
            self.now += 0.01
            if not self.pending and self.index < len(wire) and wire[self.index]["kind"] == "RX":
                raw = wire[self.index].get("hex", "")
                if len(raw) > 256:
                    raise ProtocolError("oversized RX event")
                self.pending.extend(bytes.fromhex(raw)); self.index += 1
            result = bytes(self.pending[:size]); del self.pending[:len(result)]
            return result

        def write(self, data):
            if self.pending or self.index >= len(wire) or wire[self.index]["kind"] != "TX" or bytes.fromhex(wire[self.index]["hex"]) != data:
                raise ProtocolError("replay command ordering mismatch")
            self.index += 1
            return len(data)

    # Historical timing/status belongs to the original transcript. Replay parses
    # its wire grammar, but cannot restore real-world timing or qualify hardware.
    link = ReplaySerial()
    session = template(); session["created_utc"] = "REPLAY"; session["firmware"]["profile"] = "BRINGUP"
    parsed = collect(link, session, simulated=True, timeout=3, boot_timeout=3,
                     clock=lambda: link.now, stamp=lambda: "REPLAY")
    # Connection loss and elapsed-time failures are original observations. Wire
    # replay cannot upgrade them by pretending to recreate the physical link.
    original_errors = [copy.deepcopy(e) for e in events if e.get("kind") == "ERROR"]
    if events[0]["kind"] != "BEGIN" or events[-1]["kind"] != "END":
        original_errors.append(dict(kind="ERROR", status="UNKNOWN", reason="Original transcript is incomplete", timestamp_utc="REPLAY"))
    return parsed[:-1] + original_errors + parsed[-1:]


def check(key, category, status, reason, evidence=None):
    return dict(check=key, category=category, status=status, rationale=reason, evidence=evidence)


def diagnostic_checks(command, value):
    def result(status, reason):
        return check(command, {0: "firmware", 1: "interlock", 2: "sensors", 3: "interlock", 4: "relay_range",
                               5: "ADC", 6: "flash", 7: "readiness", 8: "calibration"}[COMMANDS.index(command)], status, reason, value)
    if command == COMMANDS[0]:
        return result("STOP" if value["fault_mask"] else "PASS", "Firmware fault mask only; zero does not authorize MEASURE")
    if command == COMMANDS[1]:
        return result({"ABSENT": "PASS", "PRESENT": "STOP", "UNKNOWN": "UNKNOWN"}[value["charger"]], "Charger state; PRESENT prohibits active operations")
    if command == COMMANDS[2]:
        if value.get("invalid"):
            return result("UNKNOWN", "Sensor context invalid")
        status = "PASS"
        if value["residual"] in ("UNSAFE", "SATURATED") or value["battery"] == "CRITICAL":
            status = "STOP"
        elif (not value["vmid_valid"] or not 1350 <= value["vmid_mv"] <= 1950 or not value["ntc_valid"] or not value["ntc_temperature_valid"] or
              value["residual"] == "UNKNOWN" or value["battery"] == "UNKNOWN" or
              value["residual_age_ms"] > 50 or value["battery_age_ms"] > 2000 or value["ntc_age_ms"] > 5000):
            status = "UNKNOWN"
        elif value["battery"] == "LOW":
            status = "WARNING"
        return result(status, "MCU ADC estimates use assumed 3.300V VDDA; NOT a measurement of the 3V3 rail or DUT temperature")
    if command == COMMANDS[3]:
        blocker = value["blocker"]
        bit = {"BLOCKED_FAULT": 32, "BLOCKED_CHARGER": 1, "BLOCKED_RESIDUAL": 2,
               "BLOCKED_SENSOR_INVALID": 4, "BLOCKED_SUPPLY": 16, "BLOCKED_RANGE": 8}.get(blocker, 0)
        consistent = ((value["allowed"] and blocker == "MEASURE_ALLOWED" and value["flags"] == 0) or
                      (not value["allowed"] and bit and value["flags"] & bit and value["flags"] <= 63))
        if blocker == "BLOCKED_RANGE" and value["flags"] != 8:
            consistent = False
        if not consistent:
            return result("STOP", "Inconsistent permission/blocker/flags")
        status = {"MEASURE_ALLOWED": "STOP", "BLOCKED_RANGE": "PASS", "BLOCKED_CHARGER": "STOP", "BLOCKED_FAULT": "STOP",
                  "BLOCKED_RESIDUAL": "STOP", "BLOCKED_SUPPLY": "WARNING", "BLOCKED_SENSOR_INVALID": "UNKNOWN"}[blocker]
        return result(status, "First-startup expects range disabled and no active permission; active testing remains manual")
    if command == COMMANDS[4]:
        return result("PASS" if value["state"] == "DISABLED" else "STOP", "Commanded range must be DISABLED; not physical VGS evidence")
    if command == COMMANDS[5]:
        return result("PASS" if value["last"] in ("OK", "BUSY") else "UNKNOWN", "Auxiliary ADC may be BUSY during its normal sensor sweep")
    if command == COMMANDS[6]:
        status = "STOP" if not value["detected"] else ("PASS" if value["jedec"] == 0xef4017 and value["capacity_bytes"] == 8388608 else "WARNING")
        return result(status, "Expected Winbond W25Q64 EF4017 / 8388608 bytes; no erase or write test")
    if command == COMMANDS[7]:
        safe = value["k1.k1"] == "SAFE" and value["range_state.range_state"] == "DISABLED" and value["safety.safety"] == "BLOCKED" and integer(value["safety.faults"]) == 0
        status = "PASS" if safe else "STOP"
        if value["k1.k1"] == "UNKNOWN":
            status = "UNKNOWN"
        if value["charger.charger"] == "PRESENT" or value["residual.residual"] in ("UNSAFE", "SATURATED") or value["residual.battery"] == "CRITICAL":
            status = "STOP"
        elif safe and (value["charger.charger"] == "UNKNOWN" or value["residual.residual"] == "UNKNOWN" or value["residual.battery"] == "UNKNOWN"):
            status = "UNKNOWN"
        elif safe and (value["display.display"] == "NOT_READY" or value["flash.flash"] == "NOT_DETECTED" or value["residual.battery"] == "LOW" or value["clock.clock"] != "HSE_PLL" or integer(value["clock.sysclk_hz"]) != 72000000 or value["last_metrology.last_metrology"] != "NONE"):
            status = "WARNING"
        return result(status, "Commanded SAFE/disabled snapshot only; display and calibration may be absent on first boot")
    if command == COMMANDS[8]:
        compatible = (value["sch"], value["model"], value["hw"]) == (2, 4, 0x00010001)
        status = "UNKNOWN" if not compatible else ("PASS" if value["act"] else "WARNING")
        if value["svc"] in ("ERROR", "STORE_BUSY", "WORKFLOW_ACTIVE", "CANDIDATE_DIRTY"):
            status = "STOP"
        elif value["act"] and (value["svc"] != "ACTIVE_VALID" or int(value["rec"]) != 33):
            status = "WARNING"
        return result(status,
                      "Uncalibrated blank device is expected; active OSL is NOT an accuracy certificate")


def make_report(session, events, replayed=False):
    validate_session(session)
    simulated = replayed or any(e.get("simulated") is True for e in events) or any(e.get("simulated") is True for e in session["manual"].values())
    checks = []
    identities = [e["fields"] for e in events if e.get("kind") == "IDENTITY"]
    errors = [e for e in events if e.get("kind") == "ERROR"]
    checks.append(check("communication", "communication", "UNKNOWN" if not identities else "PASS", "Passive detailed BRINGUP identity required", identities[-1] if identities else None))
    if identities:
        identity = identities[-1]
        status = "PASS"
        if identity["clock_status"] != "OK" or identity["swd"] != "PRESERVED" or identity["watchdog_policy"] != "IWDG_START_AFTER_UART_BANNER":
            status = "STOP"
        elif identity["clock_source"] != "HSE_PLL" or identity["sysclk_hz"] != "72000000" or identity["reset"] in ("WATCHDOG", "BROWNOUT_OR_LOW_POWER", "UNKNOWN"):
            status = "WARNING"
        checks.append(check("boot_diagnostics", "firmware", status, "Reported clock/SWD/watchdog policy; boot SAFE is not a measured output waveform", identity))
    for error in errors:
        checks.append(check("collection", "communication", error["status"], error["reason"]))
    startup = [e for e in events if e.get("kind") == "STARTUP"]
    for event in startup:
        if ((event["key"] in ("k1", "k2", "range", "adc", "aux_sensors", "spi2") and event["value"] != "OK") or
            (event["key"] == "safety_faults" and integer(event["value"]) != 0) or
            (event["key"] == "safety_block" and event["value"] in ("BLOCKED_FAULT", "BLOCKED_CHARGER", "BLOCKED_RESIDUAL"))):
            checks.append(check("startup_"+event["key"], "firmware", "STOP", "Peripheral initialization/fault diagnostic", event))
    for command in COMMANDS:
        responses = [e for e in events if e.get("kind") == "RESPONSE" and e.get("command") == command]
        if len(responses) != 1:
            checks.append(check(command, "communication", "NOT_TESTED" if not responses else "UNKNOWN", "No unique complete response"))
        else:
            # Revalidate saved raw lines even for direct report calls.
            try:
                checks.append(diagnostic_checks(command, parse_response(command, responses[0]["lines"])))
            except (ValueError, KeyError, IndexError) as exc:
                checks.append(check(command, "communication", "UNKNOWN", str(exc)))
    for key, entry in session["manual"].items():
        checks.append(check(key, "manual_electrical" if MANUAL[key][2] else "manual_inspection", entry["status"], entry["notes"] or MANUAL[key][0], copy.deepcopy(entry)))
    if any(e.get("kind") == "INTERLEAVED" for e in events):
        checks.append(check("interleaved_output", "communication", "WARNING", "Unsolicited diagnostic logs retained; inspect transcript"))
    for event in events:
        if event.get("kind") == "INTERLEAVED":
            line = event["line"]
            if line in ("safety_block: BLOCKED_FAULT", "safety_block: BLOCKED_CHARGER", "safety_block: BLOCKED_RESIDUAL", "safety_state: READY"):
                checks.append(check("safety_transition", "interlock", "STOP", "Safety changed during the snapshot", line))
            elif line == "safety_block: BLOCKED_SENSOR_INVALID":
                checks.append(check("safety_transition", "interlock", "UNKNOWN", "Sensor validity changed during the snapshot", line))
    counts = {status: sum(c["status"] == status for c in checks) for status in STATES}
    overall = next((status for status in ("STOP", "UNKNOWN", "NOT_TESTED", "WARNING", "PASS") if counts[status]), "NOT_TESTED")
    if counts["STOP"]:
        next_step = "STOP active work. Investigate recorded failure with power removed where appropriate; resolve gate and repeat read-only snapshot."
    elif counts["UNKNOWN"] or counts["NOT_TESTED"]:
        next_step = "Resolve missing identity/physical evidence using A05 unpowered inspection and current-limited rail checks; no active capture is authorized."
    else:
        next_step = "Review evidence with the owner against A05 staged bench gates; this snapshot grants no relay, excitation or calibration permission."
    return {"schema": 1, "SIMULATED": simulated, "OFFLINE_REPLAY": replayed,
            "physically_validated": False, "qualification": "REQUIRES_BENCH_VALIDATION",
            "session": copy.deepcopy(session), "identity": identities[-1] if identities else None,
            "checks": checks, "counts": counts, "result": overall, "next_safe_step": next_step,
            "transcript_sha256": hashlib.sha256(json.dumps(events, sort_keys=True).encode()).hexdigest()}


def markdown(report):
    def safe(value):
        return str(value).replace("|", "\\|").replace("\r", " ").replace("\n", " ")
    lines = ["# B01.0 first-startup evidence", "", "**SIMULATED**" if report["SIMULATED"] else "**Physical evidence pending; UART reports are not bench qualification.**",
             "", "Result: **" + report["result"] + "**. REQUIRES_BENCH_VALIDATION.",
             "", "Identity: `" + safe(report["identity"]) + "`", "", "| Check | Status | Rationale / evidence |", "| --- | --- | --- |"]
    lines += ["| " + safe(c["check"]) + " | " + c["status"] + " | " + safe(c["rationale"]) + " " + safe(c["evidence"] or "") + " |" for c in report["checks"]]
    lines += ["", "Next safe step: " + report["next_safe_step"], "", "No active command was authorized. Scope grounds: actual GND only, never VMID; do not float protective earth.", ""]
    return "\n".join(lines)


def save_bundle(out, session, events, replayed=False):
    out = Path(out); out.mkdir(parents=True, exist_ok=False)
    write_json(out / "transcript.json", events)
    raw = b"".join(bytes.fromhex(e["hex"]) for e in events if e.get("kind") == "RX")
    (out / "uart-rx.bin").write_bytes(raw)
    (out / "uart-rx.txt").write_text(raw.decode("ascii", errors="backslashreplace"), encoding="utf-8")
    report = make_report(session, events, replayed)
    report["uart_rx_sha256"] = hashlib.sha256(raw).hexdigest()
    if report["SIMULATED"]:
        (out / "SIMULATED.txt").write_text("SIMULATED / OFFLINE: every file in this bundle is demonstration or replay evidence. No physical validation.\n", encoding="utf-8")
    write_json(out / "report.json", report)
    (out / "report.md").write_text(markdown(report), encoding="utf-8")
    return report


def clamp_estimate(vout, vadc, resistance, voltage_uncertainty, resistance_uncertainty):
    values = (vout, vadc, resistance, voltage_uncertainty, resistance_uncertainty)
    if any(not math.isfinite(x) for x in values) or resistance <= 0 or voltage_uncertainty < 0 or not 0 <= resistance_uncertainty < resistance:
        raise ValueError("need finite measured voltages, positive fitted R and absolute uncertainty bounds")
    delta = vout - vadc
    return {"resistor_current_a": delta/resistance,
            "interval_a": sorted(((delta-2*voltage_uncertainty)/(resistance-resistance_uncertainty),
                                  (delta-2*voltage_uncertainty)/(resistance+resistance_uncertainty),
                                  (delta+2*voltage_uncertainty)/(resistance-resistance_uncertainty),
                                  (delta+2*voltage_uncertainty)/(resistance+resistance_uncertainty)))[::3],
            "meaning": "Series-resistor estimate only; capacitor/ADC currents may contribute. Not proof of BAT54S conduction or electrical safety."}


def open_serial(port):
    import serial  # PC only, optional for offline use
    # No control line may be wired to reset or board power. Set before open to
    # avoid pyserial's default assertion; some adapters still glitch at open.
    link = serial.Serial(port=None, baudrate=115200, bytesize=8, parity="N", stopbits=1,
                         timeout=0.05, write_timeout=1, xonxoff=False, rtscts=False, dsrdtr=False)
    link.dtr = False; link.rts = False; link.port = port; link.open()
    return link


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    prepare = sub.add_parser("prepare"); prepare.add_argument("--session", type=Path, required=True)
    prepare.add_argument("--profile", choices=("UNKNOWN", "BRINGUP", "BRINGUP_CAL", "PRODUCT"), default="UNKNOWN")
    prepare.add_argument("--firmware-sha", default="UNKNOWN")
    record = sub.add_parser("record"); record.add_argument("--session", type=Path, required=True)
    record.add_argument("--check", choices=tuple(MANUAL), required=True)
    record.add_argument("--status", choices=STATES, required=True); record.add_argument("--value", type=float)
    record.add_argument("--unit"); record.add_argument("--instrument", required=True); record.add_argument("--operator", required=True)
    record.add_argument("--evidence", action="append", required=True); record.add_argument("--notes", required=True)
    record.add_argument("--simulated", action="store_true")
    record.add_argument("--authorization-reference", help="Required evidence reference for a later authorized clamp capture")
    sub.add_parser("ports")
    for mode in ("snapshot", "fake", "replay", "report"):
        cmd = sub.add_parser(mode); cmd.add_argument("--session", type=Path, required=True); cmd.add_argument("--out", type=Path, required=True)
        if mode == "snapshot":
            cmd.add_argument("--port", required=True); cmd.add_argument("--timeout", type=float, default=3)
            cmd.add_argument("--boot-timeout", type=float, default=15)
        if mode == "replay":
            cmd.add_argument("--transcript", type=Path, required=True)
    estimate = sub.add_parser("clamp-estimate")
    for name in ("vout", "vadc", "resistance", "voltage-uncertainty", "resistance-uncertainty"):
        estimate.add_argument("--"+name, required=True, type=float)
    args = parser.parse_args(argv)
    try:
        if args.mode == "prepare":
            if args.session.exists():
                raise ValueError("session already exists; will not overwrite")
            if args.firmware_sha != "UNKNOWN" and not re.fullmatch(r"[0-9a-f]{12,40}", args.firmware_sha):
                raise ValueError("firmware SHA must be UNKNOWN or 12..40 hex characters")
            session = template(); session["firmware"].update(profile=args.profile, expected_sha=args.firmware_sha)
            write_json(args.session, session); return 0
        if args.mode == "clamp-estimate":
            print(json.dumps(clamp_estimate(args.vout, args.vadc, args.resistance, args.voltage_uncertainty, args.resistance_uncertainty), indent=2)); return 0
        if args.mode == "ports":
            from serial.tools import list_ports
            for port in list_ports.comports():
                print(port.device, port.description)
            return 0
        session = validate_session(load_json(args.session))
        if args.mode == "record":
            session.setdefault("manual_history", []).append(dict(check=args.check, previous=copy.deepcopy(session["manual"][args.check])))
            session["manual"][args.check].update(status=args.status, value=args.value, unit=args.unit, instrument=args.instrument,
                                                  operator=args.operator, evidence=args.evidence, notes=args.notes,
                                                  timestamp_utc=utc_now(), simulated=args.simulated)
            if args.authorization_reference:
                session["manual"][args.check]["authorization_reference"] = args.authorization_reference
            validate_session(session); write_json(args.session, session); return 0
        # Refuse existing destinations before any serial open or command.
        if args.out.exists():
            raise ValueError("output directory already exists; use a fresh session snapshot destination")
        if args.mode == "snapshot":
            require_serial_gates(session)
            try:
                link = open_serial(args.port)
            except OSError as exc:
                events = [dict(kind="BEGIN", simulated=False, timestamp_utc=utc_now()),
                          dict(kind="ERROR", status="STOP", reason="Serial open failed: "+str(exc), timestamp_utc=utc_now()),
                          dict(kind="END", timestamp_utc=utc_now())]
            else:
                try:
                    events = collect(link, session, timeout=args.timeout, boot_timeout=args.boot_timeout)
                finally:
                    link.close()
        elif args.mode == "fake":
            session["firmware"]["profile"] = "BRINGUP"
            link = FakeSerial()
            events = collect(link, session, simulated=True, clock=lambda: link.now)
        elif args.mode == "replay":
            events = replay(load_json(args.transcript))
        else:
            events = []
        report = save_bundle(args.out, session, events, args.mode == "replay")
        print(report["result"], "SIMULATED" if report["SIMULATED"] else "REQUIRES_BENCH_VALIDATION", args.out)
        return 2 if report["result"] == "STOP" else (3 if report["result"] == "UNKNOWN" else 0)
    except (OSError, ValueError, ImportError) as exc:
        parser.exit(2, str(exc) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
