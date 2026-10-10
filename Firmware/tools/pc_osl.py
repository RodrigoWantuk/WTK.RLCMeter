"""PC model-4 OSL backend. Candidates are never physically qualified by this tool."""

from __future__ import annotations

from dataclasses import dataclass
import json
import math
import struct
import zlib

import inspect_calibration_record as wire
from reference_impedance import HG_NOMINAL

MIN_SEPARATION = 1.0e-5
CHANNELS = ("vexc_1", "ret_1x", "vexc_2", "ret_hg", "vmid_adc1", "vmid_adc2")


def finite(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError("expected a finite number")
    return float(value)


def integer(value, low, high):
    if isinstance(value, bool) or not isinstance(value, int) or not low <= value <= high:
        raise ValueError(f"expected integer in {low}..{high}")
    return value


def complex_value(value):
    if not isinstance(value, (list, tuple)) or len(value) != 2:
        raise ValueError("complex quantity must be [real, imaginary]")
    return complex(finite(value[0]), finite(value[1]))


def pair(value):
    return [value.real, value.imag]


def f32(value):
    try:
        result = struct.unpack("<f", struct.pack("<f", finite(value)))[0]
    except (OverflowError, struct.error) as exc:
        raise ValueError("quantity exceeds float32 range") from exc
    if not math.isfinite(result):
        raise ValueError("nonfinite float32 quantity")
    return result


def c32(value):
    return complex(f32(value.real), f32(value.imag))


@dataclass(frozen=True, order=True)
class Key:
    rref_ohms: int
    frequency_hz: int
    amplitude_mvrms: int

    def __post_init__(self):
        for value in (self.rref_ohms, self.frequency_hz, self.amplitude_mvrms):
            integer(value, 1, 1000000)
        if (self.rref_ohms not in wire.RANGES or self.frequency_hz not in wire.FREQUENCIES or
                self.amplitude_mvrms not in wire.AMPLITUDES or
                (self.rref_ohms == 10 and self.amplitude_mvrms == 500)):
            raise ValueError("unsupported or forbidden Rev.1 condition")

    @classmethod
    def from_json(cls, item):
        return cls(item["rref_ohms"], item["frequency_hz"], item["amplitude_mvrms"])

    def ids(self):
        return (wire.RANGES.index(self.rref_ohms), wire.FREQUENCIES.index(self.frequency_hz),
                wire.AMPLITUDES.index(self.amplitude_mvrms))

    def as_json(self):
        return dict(rref_ohms=self.rref_ohms, frequency_hz=self.frequency_hz,
                    amplitude_mvrms=self.amplitude_mvrms)


KEYS = tuple(Key(r, f, a) for r in wire.RANGES for f in wire.FREQUENCIES
             for a in wire.AMPLITUDES if not (r == 10 and a == 500))


@dataclass(frozen=True)
class Standard:
    key: Key
    standard: str
    t_1x: complex
    t_hg_raw: complex
    ret_1x_valid: bool
    ret_hg_valid: bool
    hg_observed: bool
    temperature_mC: int | None
    load: complex = 0j
    tolerance_pct: float = 0.0
    reference_id: str = ""


def standard_from_capture(item):
    key = Key.from_json(item["condition"])
    standard = item["standard"]
    if standard not in ("OPEN", "SHORT", "LOAD"):
        raise ValueError("unknown standard")
    if item.get("stable") is not True:
        raise ValueError("unstable or unqualified capture stability")
    if item.get("safe_capture") is not True:
        raise ValueError("capture lacks safe acquisition acknowledgement")
    phasors = {name: complex_value(item["phasors"][name]) for name in CHANNELS}
    vmid = (phasors["vmid_adc1"] + phasors["vmid_adc2"]) * 0.5
    vs1, vs2 = phasors["vexc_1"] - vmid, phasors["vexc_2"] - vmid
    if min(abs(vs1), abs(vs2)) <= 1.0e-6:
        raise ValueError("source too small for normalized calibration capture")
    t1 = (phasors["ret_1x"] - vmid) / vs1
    th = (phasors["ret_hg"] - vmid) / vs2
    quality = item["quality"]
    for flag in ("ret_1x_valid", "ret_hg_valid", "hg_observed"):
        if type(quality[flag]) is not bool:
            raise ValueError("path quality flags must be boolean")
    if quality["hg_observed"] and not (quality["ret_1x_valid"] and quality["ret_hg_valid"]):
        raise ValueError("HG overlap requires both usable paths")
    temperature = item.get("temperature_mC")
    if temperature is not None:
        integer(temperature, -40000, 125000)
    load, tolerance, ref_id = 0j, 0.0, ""
    if standard == "LOAD":
        ref = item["reference"]
        load = complex_value(ref["impedance_ohms"])
        tolerance = finite(ref["tolerance_pct"])
        ref_id = ref["id"]
        if load.real < 0.0 or abs(load) <= MIN_SEPARATION or not 0.0 < tolerance < 100.0:
            raise ValueError("LOAD requires nonzero passive impedance and explicit nonzero tolerance")
        if not isinstance(ref_id, str) or not ref_id.strip():
            raise ValueError("LOAD requires a reference ID")
    # Reject ratios which cannot be sent to the C solver as finite binary32.
    t1, th = c32(t1), c32(th)
    return Standard(key, standard, t1, th, quality["ret_1x_valid"],
                    quality["ret_hg_valid"], quality["hg_observed"], temperature,
                    c32(load), tolerance, ref_id)


@dataclass(frozen=True)
class Solution:
    key: Key
    hg: complex
    load: complex
    short: complex
    opened: complex
    k: complex
    hg_observed: bool
    fit_channel: str
    temperature_mC: int | None
    reference_id: str
    tolerance_pct: float
    min_separation: float

    def values(self):
        return [v for c in (self.hg, self.load, self.short, self.opened, self.k, 0j)
                for v in pair(c)]

    def apply(self, transfer):
        if abs(transfer - self.opened) <= MIN_SEPARATION:
            raise ValueError("near-OPEN OSL singularity")
        result = self.k * (transfer - self.short) / (transfer - self.opened)
        if not math.isfinite(result.real) or not math.isfinite(result.imag):
            raise ValueError("nonfinite corrected impedance")
        return result


def solve(standards):
    if len(standards) != 3 or {s.standard for s in standards} != {"OPEN", "SHORT", "LOAD"}:
        raise ValueError("exactly one OPEN, SHORT and LOAD is required")
    by_type = {s.standard: s for s in standards}
    opened, short, load = (by_type[name] for name in ("OPEN", "SHORT", "LOAD"))
    if not (opened.key == short.key == load.key):
        raise ValueError("OSL condition mismatch")
    overlaps = [s.t_hg_raw / s.t_1x for s in standards
                if s.hg_observed and abs(s.t_1x) > MIN_SEPARATION]
    hg = sum(overlaps) / len(overlaps) if overlaps else HG_NOMINAL
    if abs(hg) <= MIN_SEPARATION:
        raise ValueError("invalid effective HG transfer")
    paths = []
    if all(s.ret_1x_valid for s in (opened, short, load)):
        paths.append(("1X", [s.t_1x for s in (opened, short, load)]))
    if overlaps and all(s.ret_hg_valid for s in (opened, short, load)):
        paths.append(("HG", [s.t_hg_raw / hg for s in (opened, short, load)]))
    for channel, (to, ts, tl) in paths:
        separation = min(abs(to-ts), abs(tl-ts), abs(tl-to))
        if separation <= MIN_SEPARATION:
            continue
        k = load.load * (tl-to) / (tl-ts)
        temps = [s.temperature_mC for s in standards if s.temperature_mC is not None]
        result = Solution(load.key, c32(hg), load.load, c32(ts), c32(to), c32(k),
                          bool(overlaps), channel, int(sum(temps)/len(temps)) if temps else None,
                          load.reference_id, load.tolerance_pct, separation)
        if (abs(result.opened-result.short) <= MIN_SEPARATION or
                min(abs(result.k), abs(result.hg), abs(result.load)) <= 1.0e-6):
            raise ValueError("float32 OSL collapse")
        # Verify the serialized fit, rather than only the host-precision solution.
        if abs(result.apply(tl)-load.load) > max(1.0e-5, abs(load.load)*5.0e-5):
            raise ValueError("float32 OSL fit residual exceeds serialization limit")
        return result
    raise ValueError("no stable nondegenerate OSL acquisition path")


def campaign_standards(document):
    if document.get("format") != "WTK_PC_OSL_CAPTURE_V1":
        raise ValueError("unsupported capture document format")
    groups = {}
    for item in document["captures"]:
        standard = standard_from_capture(item)
        group = groups.setdefault(standard.key, {})
        if standard.standard in group:
            raise ValueError("duplicate standard/condition capture")
        group[standard.standard] = standard
    if set(groups) != set(KEYS):
        raise ValueError("campaign must cover all 33 Rev.1 keys")
    if any(set(group) != {"OPEN", "SHORT", "LOAD"} for group in groups.values()):
        raise ValueError("campaign contains incomplete OSL triplets")
    return [[groups[key][name] for name in ("OPEN", "SHORT", "LOAD")] for key in KEYS]


def validate_frame(frame):
    summary = wire.decode_full_rev1_frame(frame)
    integer(summary.sequence, 1, 0xffffffff)
    total = wire.HEADER_BYTES + struct.unpack_from("<H", frame, 10)[0]
    if len(frame) != total:
        raise ValueError("provisioning candidate must be exactly one frame")
    adc = struct.unpack_from("<12f", frame, 72)
    if any(v <= 0.0 for v in adc[::2]):
        raise ValueError("ADC scales must be positive")
    return summary


def serialize(solutions, sequence, adc_values):
    integer(sequence, 1, 0xffffffff)
    if len(solutions) != 33 or {s.key for s in solutions} != set(KEYS):
        raise ValueError("serialization requires 33 unique conditions")
    adc = [f32(v) for v in adc_values]
    if len(adc) != 12 or any(v <= 0.0 for v in adc[::2]):
        raise ValueError("six ADC scale/offset pairs required")
    payload_length = wire.SET_PAYLOAD_HEADER_BYTES + 33*wire.RECORD_BYTES
    frame = bytearray(wire.HEADER_BYTES+payload_length)
    struct.pack_into("<IHHHHIIHH", frame, 0, wire.MAGIC, 1, 2, 64, payload_length,
                     sequence, wire.REV1_HARDWARE, 4, 1)
    struct.pack_into("<HHI12f", frame, 64, 33, 0, 1, *adc)
    for i, solution in enumerate(sorted(solutions, key=lambda s: s.key)):
        rid, fid, aid = solution.key.ids()
        condition_id = zlib.crc32(struct.pack("<IHBBB3x", wire.REV1_HARDWARE, 4, rid, fid, aid))
        flags = wire.FLAG_OSL_MODEL | wire.FLAG_LOAD_REFERENCE
        if solution.hg_observed:
            flags |= wire.FLAG_HG_OBSERVED
        if solution.temperature_mC is not None:
            flags |= 1 << 6
        offset = 120+i*80
        struct.pack_into("<IHBBBBiII12f", frame, offset, wire.REV1_HARDWARE, 4,
                         rid, fid, aid, 2, solution.temperature_mC or 0, condition_id,
                         flags, *solution.values())
    struct.pack_into("<I", frame, 56, wire.crc_frame(frame, payload_length))
    struct.pack_into("<I", frame, 60, wire.COMMIT_MARKER)
    validate_frame(bytes(frame))
    return bytes(frame)


def build_candidate(document, sequence):
    groups = campaign_standards(document)
    solutions = [solve(group) for group in groups]
    adc = document["adc"]["values"]
    provenance = document["adc"].get("provenance")
    if not isinstance(provenance, str) or not provenance.strip():
        raise ValueError("ADC calibration provenance is required")
    return serialize(solutions, sequence, adc), solutions


def report(document, frame, solutions):
    summary = validate_frame(frame)
    lines = ["# PC OSL calibration candidate", "", f"Sequence: {summary.sequence}",
             f"Frame: {len(frame)} bytes; SHA-256: {summary.sha256}",
             "Evidence: " + str(document.get("evidence", "UNQUALIFIED_IMPORTED")),
             "Qualification: UNQUALIFIED / REQUIRES_BENCH_VALIDATION", "",
             "LOAD central values are fit anchors, not certified true values. Printed tolerance",
             "remains a reference interval; it is not an instrument error specification.", "",
             "| RREF / Hz / mVrms | Path | Reference | LOAD center (ohm) | Tolerance | Minimum transfer separation |",
             "| --- | --- | --- | --- | --- | --- |"]
    for s in solutions:
        lines.append(f"| {s.key.rref_ohms} / {s.key.frequency_hz} / {s.key.amplitude_mvrms} | "
                     f"{s.fit_channel} | {s.reference_id} | {s.load} | +/-{s.tolerance_pct:g}% | {s.min_separation:.6g} |")
    return "\n".join(lines)+"\n"


def load_document(path):
    # Do not let duplicate JSON fields silently replace safety/reference metadata.
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON field: " + key)
            result[key] = value
        return result
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)


def c_solver_input(document, sequence):
    """Test-bridge format only; never sent to a device."""
    groups = campaign_standards(document)
    data = bytearray(b"PCOS"+struct.pack("<I12f", sequence, *document["adc"]["values"]))
    for group in groups:
        data += struct.pack("<3Bx2f", *group[0].key.ids(), *pair(group[2].load))
        for s in group:
            flags = int(s.ret_1x_valid) | (int(s.ret_hg_valid)<<1) | (int(s.hg_observed)<<2)
            if s.temperature_mC is not None:
                flags |= 8
            data += struct.pack("<4fiI", *pair(s.t_1x), *pair(s.t_hg_raw), s.temperature_mC or 0, flags)
    return bytes(data)
