#!/usr/bin/env python3
"""Inspect a WTK.RLCMeter calibration frame.

The firmware stores calibration frames as explicit little-endian fields. This
tool intentionally duplicates only the portable framing/parser logic needed for
diagnostics; production firmware does not depend on Python.
"""

from __future__ import annotations

import argparse
import binascii
import hashlib
import math
import struct
from pathlib import Path
from typing import NamedTuple


MAGIC = 0x434C4157
SCHEMA_VERSION = 2
MODEL_CURRENT = 4
FLAG_HG_OBSERVED = 1 << 7
FLAG_QUALIFIED = 1 << 8
FLAG_OSL_MODEL = 1 << 5
FLAG_LOAD_REFERENCE = 1 << 9
COMMIT_MARKER = 0x54494D43
HEADER_BYTES = 64
CRC_OFFSET = 56
COMMIT_OFFSET = 60
SET_PAYLOAD_HEADER_BYTES = 56
RECORD_BYTES = 80
MAX_FRAME_BYTES = 3072
SLOT_BYTES = 4096
REV1_HARDWARE = 0x00010001
RANGES = (10, 100, 1000, 10000, 100000, 1000000)
FREQUENCIES = (100, 1000, 10000)
AMPLITUDES = (100, 500)


class OslFrameSummary(NamedTuple):
    sequence: int
    sha256: str
    conditions: frozenset[tuple[int, int, int]]


def decode_full_rev1_frame(blob: bytes) -> OslFrameSummary:
    """Check one supplied active-set image, not the physical OSL campaign history."""
    if not HEADER_BYTES <= len(blob) <= SLOT_BYTES:
        raise ValueError("expected one calibration frame or 4096-byte slot image")
    magic, record_type, schema, header_size, payload_len, sequence, hardware, model = \
        struct.unpack_from("<IHHHHIIH", blob)
    total = header_size + payload_len
    if (magic, record_type, schema, header_size, hardware, model) != \
            (MAGIC, 1, SCHEMA_VERSION, HEADER_BYTES, REV1_HARDWARE, MODEL_CURRENT):
        raise ValueError("incompatible Rev.1 calibration frame")
    if total > len(blob) or total > MAX_FRAME_BYTES or \
            payload_len != SET_PAYLOAD_HEADER_BYTES + 33 * RECORD_BYTES:
        raise ValueError("calibration frame has an incomplete or invalid payload")
    if struct.unpack_from("<I", blob, COMMIT_OFFSET)[0] != COMMIT_MARKER or \
            struct.unpack_from("<I", blob, CRC_OFFSET)[0] != crc_frame(blob, payload_len):
        raise ValueError("calibration frame is uncommitted or CRC-corrupt")
    count = struct.unpack_from("<H", blob, HEADER_BYTES)[0]
    adc_flags = struct.unpack_from("<I", blob, HEADER_BYTES + 4)[0]
    adc_values = struct.unpack_from("<12f", blob, HEADER_BYTES + 8)
    if count != 33 or not (adc_flags & 1) or not all(math.isfinite(v) for v in adc_values):
        raise ValueError("calibration frame has invalid ADC or record count")

    keys: set[tuple[int, int, int]] = set()
    for index in range(count):
        offset = HEADER_BYTES + SET_PAYLOAD_HEADER_BYTES + index * RECORD_BYTES
        rec_hw, rec_model, range_id, freq_id, amp_id, rec_type, _, condition_id, flags = \
            struct.unpack_from("<IHBBBBiII", blob, offset)
        if (rec_hw, rec_model, rec_type) != (REV1_HARDWARE, MODEL_CURRENT, 2) or \
                range_id >= len(RANGES) or freq_id >= len(FREQUENCIES) or \
                amp_id >= len(AMPLITUDES) or (range_id == 0 and amp_id == 1):
            raise ValueError(f"invalid Rev.1 condition record {index}")
        key_bytes = struct.pack("<IHBBB3x", rec_hw, rec_model, range_id, freq_id, amp_id)
        if condition_id != binascii.crc32(key_bytes) & 0xFFFFFFFF:
            raise ValueError(f"condition ID mismatch in record {index}")
        values = struct.unpack_from("<12f", blob, offset + 22)
        hg = complex(*values[0:2])
        load = complex(*values[2:4])
        short = complex(*values[4:6])
        opened = complex(*values[6:8])
        k = complex(*values[8:10])
        if (not all(math.isfinite(v) for v in values) or
                (flags & (FLAG_OSL_MODEL | FLAG_LOAD_REFERENCE)) !=
                (FLAG_OSL_MODEL | FLAG_LOAD_REFERENCE) or
                flags & 0x1E or min(abs(hg), abs(load), abs(k)) <= 1.0e-6 or
                abs(opened - short) <= 1.0e-5):
            raise ValueError(f"invalid OSL coefficients in record {index}")
        key = (RANGES[range_id], FREQUENCIES[freq_id], AMPLITUDES[amp_id])
        if key in keys:
            raise ValueError(f"duplicate OSL condition {key}")
        keys.add(key)
    expected = {(r, f, a) for r in RANGES for f in FREQUENCIES
                for a in AMPLITUDES if not (r == 10 and a == 500)}
    if keys != expected:
        raise ValueError("calibration frame does not cover all 33 Rev.1 conditions")
    return OslFrameSummary(sequence, hashlib.sha256(blob[:total]).hexdigest(), frozenset(keys))


def crc_frame(frame: bytes, payload_length: int) -> int:
    data = frame[:CRC_OFFSET] + frame[CRC_OFFSET + 4 : COMMIT_OFFSET]
    data += frame[HEADER_BYTES : HEADER_BYTES + payload_length]
    return binascii.crc32(data) & 0xFFFFFFFF


def decode_record(data: bytes, index: int) -> dict[str, object]:
    off = 0
    hardware, model = struct.unpack_from("<IH", data, off)
    off += 6
    range_id, frequency, amplitude, record_type = struct.unpack_from("<BBBB", data, off)
    off += 4
    temperature_mC, condition_id, flags = struct.unpack_from("<iII", data, off)
    off += 12
    floats = struct.unpack_from("<" + ("f" * 12), data, off)
    record = {
        "index": index,
        "hardware_revision": f"0x{hardware:08X}",
        "model_version": model,
        "range_id": range_id,
        "frequency": frequency,
        "amplitude": amplitude,
        "record_type": record_type,
        "temperature_mC": temperature_mC,
        "condition_id": f"0x{condition_id:08X}",
        "flags": f"0x{flags:08X}",
        "effective_hg_transfer": complex(floats[0], floats[1]),
        "load_z_ohms": complex(floats[2], floats[3]),
        "t_short": complex(floats[4], floats[5]),
        "t_open": complex(floats[6], floats[7]),
        "k": complex(floats[8], floats[9]),
        "reserved": complex(floats[10], floats[11]),
    }
    if model == MODEL_CURRENT:
        record.update(
            {
                "osl_effective_hg_transfer": complex(floats[0], floats[1]),
                "osl_load_reference": complex(floats[2], floats[3]),
                "osl_t_short": complex(floats[4], floats[5]),
                "osl_t_open": complex(floats[6], floats[7]),
                "osl_k": complex(floats[8], floats[9]),
                "osl_hg_observed": "yes" if (flags & FLAG_HG_OBSERVED) else "no",
                "qualified": "yes" if (flags & FLAG_QUALIFIED) else "no",
            }
        )
    return record


def inspect(path: Path) -> int:
    blob = path.read_bytes()
    if len(blob) < HEADER_BYTES:
        raise SystemExit("file is shorter than calibration frame header")

    magic, record_type, schema, header_size, payload_length, sequence, hardware, model = struct.unpack_from(
        "<IHHHHIIH", blob, 0
    )
    crc_stored = struct.unpack_from("<I", blob, CRC_OFFSET)[0]
    commit = struct.unpack_from("<I", blob, COMMIT_OFFSET)[0]
    total = header_size + payload_length
    if len(blob) < total:
        raise SystemExit("file is shorter than declared calibration frame length")

    crc_calc = crc_frame(blob, payload_length)
    print(f"magic=0x{magic:08X}")
    print(f"record_type={record_type}")
    print(f"schema_version={schema}")
    print(f"header_size={header_size}")
    print(f"payload_length={payload_length}")
    print(f"sequence={sequence}")
    print(f"hardware_revision=0x{hardware:08X}")
    print(f"model_version={model}")
    print(f"commit={'VALID' if commit == COMMIT_MARKER else 'MISSING'}")
    print(f"crc32_stored=0x{crc_stored:08X}")
    print(f"crc32_calculated=0x{crc_calc:08X}")
    print(f"crc32={'OK' if crc_stored == crc_calc else 'FAIL'}")

    if magic != MAGIC or schema != SCHEMA_VERSION or header_size != HEADER_BYTES:
        return 2
    if commit != COMMIT_MARKER or crc_stored != crc_calc:
        return 3

    payload = blob[HEADER_BYTES:total]
    count, required_count = struct.unpack_from("<HH", payload, 0)
    adc_flags = struct.unpack_from("<I", payload, 4)[0]
    print(f"record_count={count}")
    print(f"required_count={required_count}")
    print(f"adc_flags=0x{adc_flags:08X}")
    for i in range(count):
        begin = SET_PAYLOAD_HEADER_BYTES + (i * RECORD_BYTES)
        rec = decode_record(payload[begin : begin + RECORD_BYTES], i)
        line = (
            "record[{index}] hw={hardware_revision} model={model_version} "
            "range={range_id} freq={frequency} amp={amplitude} "
            "type={record_type} temp_mC={temperature_mC} condition={condition_id} "
            "flags={flags} effective_hg_transfer={effective_hg_transfer} "
            "load_z_ohms={load_z_ohms} t_short={t_short} t_open={t_open} "
            "k={k} reserved={reserved}"
        ).format(**rec)
        if rec["model_version"] == MODEL_CURRENT:
            line += (
                " osl_t_short={osl_t_short} osl_t_open={osl_t_open} "
                "osl_k={osl_k} osl_load_reference={osl_load_reference} "
                "effective_hg={osl_effective_hg_transfer} hg_observed={osl_hg_observed} "
                "qualified={qualified}"
            ).format(**rec)
        print(line)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frame", type=Path, help="binary calibration frame or slot image")
    args = parser.parse_args()
    return inspect(args.frame)


if __name__ == "__main__":
    raise SystemExit(main())
