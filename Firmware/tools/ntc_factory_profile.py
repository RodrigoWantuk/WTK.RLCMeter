"""Versioned, unqualified per-unit NTC factory LUT; no device access."""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct

FORMAT = "WTK_NTC_FACTORY_PROFILE_V1"


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("utf-8")


def number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError("finite numeric calibration value required")
    return float(value)


def compile_profile(profile):
    if profile.get("format") != FORMAT or profile.get("qualified") is not False:
        raise ValueError("version-1 engineering profile must remain unqualified")
    if not re.fullmatch("[0-9a-f]{24}", profile.get("uid", "")):
        raise ValueError("explicit lowercase MCU UID required")
    if profile.get("owner_authorized") is not True:
        raise ValueError("owner authorization provenance required")
    points = profile.get("points")
    if not isinstance(points, list) or len(points) != 2:
        raise ValueError("exactly two reference points required")
    points = sorted([(number(p["temperature_c"]), number(p["adc_raw"])) for p in points])
    (cold, raw_cold), (warm, raw_warm) = points
    if not (-20 <= cold < warm <= 80 and warm-cold >= 5 and 16 < raw_warm < raw_cold < 4079):
        raise ValueError("separated temperatures and decreasing unclipped NTC ADC required")
    rc, rw = raw_cold/(4095-raw_cold), raw_warm/(4095-raw_warm)
    beta = math.log(rc/rw)/(1/(cold+273.15)-1/(warm+273.15))
    ratio25 = math.exp(math.log(rc)-beta*(1/(cold+273.15)-1/298.15))
    if not (math.isfinite(beta) and beta > 0 and math.isfinite(ratio25) and ratio25 > 0):
        raise ValueError("nonphysical/nonfinite effective NTC model")
    grid = [float(t) for t in range(-20, 81, 5)]
    references = (cold, warm)
    for t in references:
        if t not in grid:
            removable = [v for v in grid if v not in references and v not in (-20, 80)]
            grid.remove(min(removable, key=lambda v: abs(v-t)))
            grid.append(t)
    grid.sort()
    rows = [(t, 100000*ratio25*math.exp(beta*(1/(t+273.15)-1/298.15))) for t in grid]
    blob = b"".join(struct.pack("<2f", *row) for row in rows)
    rows = [struct.unpack_from("<2f", blob, i*8) for i in range(21)]
    if any(not all(math.isfinite(v) for v in row) or row[1] <= 0 for row in rows):
        raise ValueError("invalid float32 LUT")
    if any(a[0] >= b[0] or a[1] <= b[1] for a, b in zip(rows, rows[1:])):
        raise ValueError("LUT loses float32 monotonicity")
    return dict(schema="WTK_NTC_COMPILED_V1", profile=profile,
                uid=profile["uid"], profile_sha256=hashlib.sha256(canonical(profile)).hexdigest(),
                effective_beta_k=beta, effective_r25_over_rtop=ratio25,
                qualified=False, lut_le_hex=blob.hex())


def validate_binary(metadata, binary, flags):
    if flags.get("WTK_NTC_FACTORY_CALIBRATION", "0u") not in ("0u", "1u"):
        raise ValueError("invalid NTC build-selection flag")
    enabled = flags.get("WTK_NTC_FACTORY_CALIBRATION", "0u") == "1u"
    if not enabled:
        if metadata is not None or flags.get("WTK_NTC_FACTORY_UID", "") or flags.get("WTK_NTC_PROFILE_SHA256", ""):
            raise ValueError("unexpected NTC factory metadata in default build")
        return
    if not isinstance(metadata, dict) or compile_profile(metadata["profile"]) != metadata:
        raise ValueError("NTC compiled profile/provenance mismatch")
    if metadata["uid"] != flags.get("WTK_NTC_FACTORY_UID") or metadata["profile_sha256"] != flags.get("WTK_NTC_PROFILE_SHA256"):
        raise ValueError("NTC profile differs from build configuration")
    if binary.count(bytes.fromhex(metadata["lut_le_hex"])) != 1:
        raise ValueError("expected NTC float32 table not uniquely present in linked binary")


def require_target(metadata, uid):
    if metadata is not None and uid != metadata["uid"]:
        raise ValueError("NTC factory firmware requires its explicit matching target UID")


def c_float(value):
    text = format(value, ".9g")
    if "." not in text and "e" not in text:
        text += ".0"
    return text + "f"


def generate(profile_path, header_path, metadata_path):
    raw = Path(profile_path).read_bytes()
    if len(raw) > 65536:
        raise ValueError("NTC profile exceeds bound")
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate NTC profile key")
            result[key] = value
        return result
    metadata = compile_profile(json.loads(raw, object_pairs_hook=unique))
    blob = bytes.fromhex(metadata["lut_le_hex"])
    rows = [struct.unpack_from("<2f", blob, i*8) for i in range(21)]
    header = "/* Generated engineering NTC factory data; not full-curve qualification. */\n"
    header += "static const ntc_lut_point_t k_ntc_lut[] = {\n"
    header += "".join("    {.temperature_c = "+c_float(t)+", .resistance_ohm = "+c_float(r)+"},\n" for t, r in rows)
    header += "};\n"
    Path(header_path).write_text(header, encoding="ascii")
    Path(metadata_path).write_text(json.dumps(metadata, indent=2, allow_nan=False)+"\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--header", required=True)
    parser.add_argument("--metadata", required=True)
    args = parser.parse_args()
    generate(args.profile, args.header, args.metadata)
