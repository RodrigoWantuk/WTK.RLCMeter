#!/usr/bin/env python3
"""Inspect a WTK.RLCMeter calibration frame.

The firmware stores calibration frames as explicit little-endian fields. This
tool intentionally duplicates only the portable framing/parser logic needed for
diagnostics; production firmware does not depend on Python.
"""

from __future__ import annotations

import argparse
import binascii
import struct
from pathlib import Path


MAGIC = 0x434C4157
SCHEMA_VERSION = 2
MODEL_OSL_MOBIUS_V1 = 3
COMMIT_MARKER = 0x54494D43
HEADER_BYTES = 64
CRC_OFFSET = 56
COMMIT_OFFSET = 60
SET_PAYLOAD_HEADER_BYTES = 56
RECORD_BYTES = 80


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
        "ret_hg": complex(floats[0], floats[1]),
        "zref": complex(floats[2], floats[3]),
        "ret_1x_output_scale": complex(floats[4], floats[5]),
        "ret_1x_output_offset": complex(floats[6], floats[7]),
        "ret_hg_output_scale": complex(floats[8], floats[9]),
        "ret_hg_output_offset": complex(floats[10], floats[11]),
    }
    if model == MODEL_OSL_MOBIUS_V1:
        record.update(
            {
                "osl_ret_hg_transfer": complex(floats[0], floats[1]),
                "osl_load_z": complex(floats[2], floats[3]),
                "osl_t_short": complex(floats[4], floats[5]),
                "osl_t_open": complex(floats[6], floats[7]),
                "osl_k": complex(floats[8], floats[9]),
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
            "flags={flags} ret_hg={ret_hg} zref={zref}"
        ).format(**rec)
        if rec["model_version"] == MODEL_OSL_MOBIUS_V1:
            line += (
                " osl_t_short={osl_t_short} osl_t_open={osl_t_open} "
                "osl_k={osl_k} osl_load_z={osl_load_z}"
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
