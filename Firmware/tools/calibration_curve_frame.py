#!/usr/bin/env python3
"""Build or inspect a non-qualified, post-OSL curve candidate frame.

This is not a device upload command. A frame remains unqualified even when all
host interval and held-out checks pass. The OSL sequence binds it to one active
OSL set; physical qualification and transactional installation are separate.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import zlib
from pathlib import Path
from typing import Any

from calibration_campaign import all_osl_conditions, condition_key


MAGIC = 0x56524357  # WCRV
COMMIT = 0x54494D43
SCHEMA_VERSION = 1
HARDWARE_REVISION = 0x00010001
OSL_MODEL_VERSION = 4
HEADER = struct.Struct("<IHHIHHIII")
RECORD = struct.Struct("<BBBB12fI")
TRAILER = struct.Struct("<II")
RANGES = (10, 100, 1000, 10000, 100000, 1000000)
FREQUENCIES = (100, 1000, 10000)
AMPLITUDES = (100, 500)


class CurveFrameError(ValueError):
    pass


def build_candidate(report: dict[str, Any], curve_sequence: int) -> bytes:
    if (report.get("status") != "HOST_PROVISIONAL_NOT_FLASH_READY" or
            report.get("qualification") != "UNQUALIFIED" or
            report.get("capture_binding") != "RAW_SHA256_BOUND"):
        raise CurveFrameError("candidate needs a successful SHA-bound, unqualified host report")
    osl_sequence = report.get("supplied_osl_sequence")
    osl_frame_crc32 = report.get("supplied_osl_frame_crc32")
    frame_hash = report.get("supplied_osl_frame_sha256")
    if (not isinstance(osl_sequence, int) or osl_sequence <= 0 or osl_sequence > 0xFFFFFFFF or
            not isinstance(curve_sequence, int) or curve_sequence <= 0 or
            curve_sequence > 0xFFFFFFFF or
            not isinstance(osl_frame_crc32, int) or not 0 <= osl_frame_crc32 <= 0xFFFFFFFF or
            not isinstance(frame_hash, str) or
            len(frame_hash) != 64 or any(c not in "0123456789abcdef" for c in frame_hash.lower())):
        raise CurveFrameError("OSL provenance and positive 32-bit sequences are required")
    groups = report.get("conditions")
    if not isinstance(groups, list) or not 0 < len(groups) <= 33:
        raise CurveFrameError("expected 1..33 condition curves")
    records = []
    seen = set()
    for group in groups:
        try:
            rref, frequency, amplitude = condition_key(group["condition"])
            curve = group["curve"]
            validation = group["validation"]
            float32_validation = group["float32_validation"]
            validation_count = group["validation_count"]
            fit_count = group["fit_count"]
        except (KeyError, TypeError) as exc:
            raise CurveFrameError("incomplete condition evidence") from exc
        key = (rref, frequency, amplitude)
        if key not in all_osl_conditions() or key in seen:
            raise CurveFrameError("unsupported or duplicate Rev.1 condition")
        seen.add(key)
        if validation != "PASS" or not isinstance(validation_count, int) or validation_count < 1:
            raise CurveFrameError("each exported condition requires held-out validation")
        if float32_validation != "PASS":
            raise CurveFrameError("stored float32 coefficients must satisfy all declared intervals")
        if not isinstance(fit_count, int) or fit_count < 1:
            raise CurveFrameError("each exported condition requires FIT standards")
        if not isinstance(curve, list) or len(curve) != 12 or not all(
                isinstance(value, (int, float)) and math.isfinite(value) for value in curve):
            raise CurveFrameError("curve must have 12 finite coefficients")
        try:
            content = struct.pack("<BBBB12f", RANGES.index(rref),
                                  FREQUENCIES.index(frequency),
                                  AMPLITUDES.index(amplitude), 0, *curve)
            record = content + struct.pack("<I", zlib.crc32(content))
        except (OverflowError, struct.error) as exc:
            raise CurveFrameError("curve coefficient is not representable as float32") from exc
        if not all(math.isfinite(value) for value in RECORD.unpack(record)[4:16]):
            raise CurveFrameError("curve coefficient overflows float32")
        records.append((key, record))
    records.sort(key=lambda entry: entry[0])
    body = HEADER.pack(MAGIC, SCHEMA_VERSION, len(records), HARDWARE_REVISION,
                       OSL_MODEL_VERSION, 0, osl_sequence, curve_sequence,
                       osl_frame_crc32)
    body += b"".join(record for _, record in records)
    return body + TRAILER.pack(zlib.crc32(body), COMMIT)


def inspect_candidate(frame: bytes) -> dict[str, Any]:
    if len(frame) < HEADER.size + TRAILER.size or len(frame) > 4096:
        raise CurveFrameError("curve frame size invalid")
    magic, version, count, hardware, model, flags, osl_sequence, curve_sequence, osl_crc = \
        HEADER.unpack_from(frame)
    if (magic != MAGIC or version != SCHEMA_VERSION or hardware != HARDWARE_REVISION or
            model != OSL_MODEL_VERSION or flags != 0 or osl_sequence == 0 or
            curve_sequence == 0 or
            count == 0 or count > 33 or
            len(frame) != HEADER.size + count * RECORD.size + TRAILER.size):
        raise CurveFrameError("curve frame header incompatible or frame truncated")
    crc, commit = TRAILER.unpack_from(frame, len(frame) - TRAILER.size)
    if crc != zlib.crc32(frame[:-TRAILER.size]) or commit != COMMIT:
        raise CurveFrameError("curve frame CRC or commit invalid")
    conditions = []
    seen = set()
    allowed = all_osl_conditions()
    for index in range(count):
        range_id, frequency_id, amplitude_id, reserved, *tail = RECORD.unpack_from(
            frame, HEADER.size + index * RECORD.size)
        curve, record_crc = tail[:12], tail[12]
        record_bytes = frame[HEADER.size + index * RECORD.size:
                             HEADER.size + (index + 1) * RECORD.size]
        if (range_id >= len(RANGES) or frequency_id >= len(FREQUENCIES) or
                amplitude_id >= len(AMPLITUDES) or reserved != 0 or
                record_crc != zlib.crc32(record_bytes[:-4]) or
                not all(math.isfinite(value) for value in curve)):
            raise CurveFrameError("curve frame record invalid")
        key = (RANGES[range_id], FREQUENCIES[frequency_id], AMPLITUDES[amplitude_id])
        if key not in allowed or key in seen:
            raise CurveFrameError("curve frame condition forbidden or duplicate")
        seen.add(key)
        conditions.append({"range_ohms": key[0], "frequency_hz": key[1],
                           "amplitude_mv_rms": key[2], "curve": curve})
    return {"osl_sequence": osl_sequence, "osl_frame_crc32": osl_crc,
            "curve_sequence": curve_sequence,
            "qualified": False, "record_count": count, "conditions": conditions}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, help="SHA-bound campaign diagnostic JSON")
    parser.add_argument("--curve-sequence", type=int, help="positive candidate revision number")
    parser.add_argument("--out", type=Path, help="output non-qualified candidate .bin")
    parser.add_argument("--inspect", type=Path, help="inspect a candidate frame instead")
    args = parser.parse_args()
    if args.inspect is not None:
        try:
            print(json.dumps(inspect_candidate(args.inspect.read_bytes()), indent=2))
        except (OSError, CurveFrameError) as exc:
            parser.error(str(exc))
        return 0
    if args.report is None or args.curve_sequence is None or args.out is None:
        parser.error("--report, --curve-sequence and --out are required for candidate export")
    try:
        frame = build_candidate(json.loads(args.report.read_text(encoding="utf-8")),
                                args.curve_sequence)
        args.out.write_bytes(frame)
    except (OSError, ValueError, TypeError, json.JSONDecodeError) as exc:
        parser.error(str(exc))
    print(f"candidate={args.out} bytes={len(frame)} qualification=UNQUALIFIED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
