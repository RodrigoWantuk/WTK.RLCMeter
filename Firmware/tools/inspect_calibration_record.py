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
SCHEMA_VERSION = 1
COMMIT_MARKER = 0x54494D43
HEADER_BYTES = 64
CRC_OFFSET = 56
COMMIT_OFFSET = 60
SET_PAYLOAD_HEADER_BYTES = 4
RECORD_BYTES = 112


def crc_frame(frame: bytes, payload_length: int) -> int:
    data = frame[:CRC_OFFSET] + frame[CRC_OFFSET + 4 : COMMIT_OFFSET]
    data += frame[HEADER_BYTES : HEADER_BYTES + payload_length]
    return binascii.crc32(data) & 0xFFFFFFFF


def decode_record(data: bytes, index: int) -> dict[str, object]:
    off = 0
    hardware, model = struct.unpack_from("<IH", data, off)
    off += 6
    range_id, frequency, amplitude, ret_channel, ret_strategy, record_type = struct.unpack_from(
        "<BBBBBB", data, off
    )
    off += 6
    temperature_mC, condition_id, flags = struct.unpack_from("<iII", data, off)
    off += 12
    floats = struct.unpack_from("<" + ("f" * 20), data, off)
    return {
        "index": index,
        "hardware_revision": f"0x{hardware:08X}",
        "model_version": model,
        "range_id": range_id,
        "frequency": frequency,
        "amplitude": amplitude,
        "ret_channel": ret_channel,
        "ret_strategy": ret_strategy,
        "record_type": record_type,
        "temperature_mC": temperature_mC,
        "condition_id": f"0x{condition_id:08X}",
        "flags": f"0x{flags:08X}",
        "ret_hg": complex(floats[12], floats[13]),
        "zref": complex(floats[14], floats[15]),
        "output_scale": complex(floats[16], floats[17]),
        "output_offset": complex(floats[18], floats[19]),
    }


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
    print(f"record_count={count}")
    print(f"required_count={required_count}")
    for i in range(count):
        begin = SET_PAYLOAD_HEADER_BYTES + (i * RECORD_BYTES)
        rec = decode_record(payload[begin : begin + RECORD_BYTES], i)
        print(
            "record[{index}] hw={hardware_revision} model={model_version} "
            "range={range_id} freq={frequency} amp={amplitude} ret={ret_channel} "
            "type={record_type} temp_mC={temperature_mC} condition={condition_id} "
            "flags={flags} ret_hg={ret_hg} zref={zref}".format(**rec)
        )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frame", type=Path, help="binary calibration frame or slot image")
    args = parser.parse_args()
    return inspect(args.frame)


if __name__ == "__main__":
    raise SystemExit(main())
