#!/usr/bin/env python3
"""Passively collect one completed BRINGUP raw DUT dump from a COM port."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import time

from reference_impedance import RawCapture, parse_raw_dump


MAX_CAPTURE_LINES = 300
MAX_LINE_BYTES = 160


def validate_dut_capture(text: str) -> RawCapture:
    if "RAW_BEGIN v=1" not in text or "RAW_END st=OK" not in text:
        raise ValueError("missing successful RAW v1 frame boundary")
    capture = parse_raw_dump(text)
    metadata = capture.metadata
    if metadata.get("m") != "DUT_MEASURE":
        raise ValueError("capture is not a DUT measurement")
    if (int(metadata.get("samples", "0")) != 256 or
            int(metadata.get("words_per_sample", "0")) != 3 or
            len(capture.rows) != 256):
        raise ValueError("unexpected raw block dimensions")
    if [row[0] for row in capture.rows] != list(range(256)):
        raise ValueError("raw row sequence is incomplete or repeated")
    if int(metadata.get("frequency_hz", "0")) not in (100, 1000, 10000):
        raise ValueError("unsupported frequency")
    if int(metadata.get("amplitude_mvrms", "0")) not in (100, 500):
        raise ValueError("unsupported amplitude")
    if metadata.get("range") not in ("10R", "100R", "1K", "10K", "100K", "1M"):
        raise ValueError("unsupported range")
    if metadata["range"] == "10R" and metadata["amplitude_mvrms"] == "500":
        raise ValueError("forbidden 10R/500mV condition")
    if not all(key in metadata for key in ("pi", "pv", "k1op", "k1rel")):
        raise ValueError("missing DUT permit/relay metadata")
    for row in capture.rows:
        if any(not 0 <= raw <= 4095 for raw in row[1:]):
            raise ValueError("ADC code outside 12-bit range")
    return capture


def receive_one(link, timeout_seconds: float = 30.0) -> tuple[str, RawCapture]:
    if timeout_seconds <= 0.0:
        raise ValueError("timeout must be positive")
    deadline = time.monotonic() + timeout_seconds
    lines: list[str] = []
    while time.monotonic() < deadline:
        raw_line = link.readline(MAX_LINE_BYTES + 1)
        if not raw_line:
            continue
        if len(raw_line) > MAX_LINE_BYTES:
            raise ValueError("serial line exceeds bounded raw format")
        line = raw_line.decode("ascii", errors="strict").strip()
        if not lines:
            if line != "RAW_BEGIN v=1":
                continue
        lines.append(line)
        if len(lines) > MAX_CAPTURE_LINES:
            raise ValueError("raw dump exceeds bounded line count")
        if line.startswith("RAW_END"):
            text = "\n".join(lines) + "\n"
            return text, validate_dut_capture(text)
    raise TimeoutError("no complete RAW dump received")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="COM port to listen to")
    parser.add_argument("--out", type=Path, required=True, help="write validated raw text")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError:
        parser.error("pyserial is required for COM capture")
    try:
        with serial.Serial(port=args.port, baudrate=args.baud, timeout=0.25) as link:
            text, capture = receive_one(link, args.timeout)
    except (OSError, ValueError, TimeoutError) as exc:
        parser.error(str(exc))
    args.out.write_text(text, encoding="ascii")
    capture_id = hashlib.sha256(text.encode("ascii")).hexdigest()
    print(f"captured {len(capture.rows)} DUT instants to {args.out} sha256={capture_id}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
