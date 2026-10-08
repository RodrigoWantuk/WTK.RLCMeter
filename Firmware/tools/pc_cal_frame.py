#!/usr/bin/env python3
"""Fetch the active BRINGUP OSL set over COM for host campaign binding."""

from __future__ import annotations

import argparse
from pathlib import Path
import time

from inspect_calibration_record import MAX_FRAME_BYTES, OslFrameSummary, decode_full_rev1_frame


BEGIN = "CAL_FRAME_BEGIN v=1"
END = "CAL_FRAME_END st=OK"
MAX_LINE_BYTES = 48


def receive_one(link, timeout_seconds: float = 30.0) -> tuple[bytes, OslFrameSummary]:
    if timeout_seconds <= 0.0:
        raise ValueError("timeout must be positive")
    deadline = time.monotonic() + timeout_seconds
    frame = bytearray()
    started = False
    short_chunk_seen = False
    while time.monotonic() < deadline:
        raw = link.readline(MAX_LINE_BYTES + 1)
        if not raw:
            continue
        if len(raw) > MAX_LINE_BYTES:
            raise ValueError("calibration frame line exceeds limit")
        line = raw.decode("ascii", errors="strict").strip()
        if not started:
            if line in ("CAL_FRAME_BUSY", "CAL_FRAME_ERROR", "CAL_FRAME_UNAVAILABLE"):
                raise ValueError(f"device rejected frame export: {line}")
            if line != BEGIN:
                continue
            started = True
            continue
        if line == END:
            return bytes(frame), decode_full_rev1_frame(bytes(frame))
        if line == BEGIN or line.startswith("CAL_FRAME_END"):
            raise ValueError("duplicate or unsuccessful calibration frame boundary")
        if (short_chunk_seen or not line.startswith("D,") or
                not 2 < len(line) <= 34 or len(line) % 2 != 0 or
                any(digit not in "0123456789ABCDEF" for digit in line[2:])):
            raise ValueError("malformed calibration frame data line")
        chunk = bytes.fromhex(line[2:])
        if len(frame) + len(chunk) > MAX_FRAME_BYTES:
            raise ValueError("calibration frame exceeds maximum size")
        frame.extend(chunk)
        short_chunk_seen = len(chunk) < 16
    raise TimeoutError("no complete calibration frame received")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="BRINGUP COM port")
    parser.add_argument("--out", type=Path, required=True, help="write validated active frame")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError:
        parser.error("pyserial is required for COM transfer")
    try:
        with serial.Serial(port=args.port, baudrate=args.baud, timeout=0.25) as link:
            link.reset_input_buffer()
            link.write(b"lab cal frame\r\n")
            frame, summary = receive_one(link, args.timeout)
    except (OSError, UnicodeError, ValueError, TimeoutError) as exc:
        parser.error(str(exc))
    args.out.write_bytes(frame)
    print(f"frame={args.out} bytes={len(frame)} sequence={summary.sequence} "
          f"sha256={summary.sha256}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
