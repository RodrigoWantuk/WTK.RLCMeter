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
SAMPLE_RATES = {100: 6400, 1000: 64000, 10000: 160000}
ROW_HEADER = "i,v1,r1,v2,rh,vm1,vm2"


def validate_dut_capture(text: str) -> RawCapture:
    try:
        lines = text.encode("ascii").decode("ascii").splitlines()
    except UnicodeError as exc:
        raise ValueError("raw dump must be ASCII") from exc
    if (len(lines) > MAX_CAPTURE_LINES or
            any(len(line) > MAX_LINE_BYTES for line in lines)):
        raise ValueError("raw dump exceeds bounded format")
    if not lines or lines[0] != "RAW_BEGIN v=1" or lines[-1] != "RAW_END st=OK":
        raise ValueError("missing successful RAW v1 frame boundary")
    if lines.count("RAW_BEGIN v=1") != 1 or lines.count("RAW_END st=OK") != 1 or \
            lines.count(ROW_HEADER) != 1 or lines.count("DSP_BEGIN v=1") != 1 or \
            lines.count("DSP_END") != 1:
        raise ValueError("raw dump must contain one complete DUT/DSP frame")
    seen = set()
    for line in lines[1:lines.index(ROW_HEADER)]:
        if "=" in line and not line.startswith("DSP_BEGIN"):
            key = line.split("=", 1)[0]
            if key in seen:
                raise ValueError(f"duplicate raw metadata: {key}")
            seen.add(key)
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
    frequency = int(metadata.get("frequency_hz", "0"))
    if frequency not in SAMPLE_RATES:
        raise ValueError("unsupported frequency")
    if int(metadata.get("sample_rate_hz", "0")) != SAMPLE_RATES[frequency]:
        raise ValueError("sample rate does not match the Rev.1 acquisition profile")
    if int(metadata.get("amplitude_mvrms", "0")) not in (100, 500):
        raise ValueError("unsupported amplitude")
    if metadata.get("range") not in ("10R", "100R", "1K", "10K", "100K", "1M"):
        raise ValueError("unsupported range")
    if metadata["range"] == "10R" and metadata["amplitude_mvrms"] == "500":
        raise ValueError("forbidden 10R/500mV condition")
    if not all(key in metadata for key in ("pi", "pv", "k1op", "k1rel",
                                            "calibration_sequence", "dsp_status")):
        raise ValueError("missing DUT permit/relay metadata")
    issued, validated = int(metadata["pi"]), int(metadata["pv"])
    if (not 0 <= issued <= 0xFFFFFFFF or not 0 <= validated <= 0xFFFFFFFF or
            ((validated - issued) & 0xFFFFFFFF) > 5):
        raise ValueError("permit validation is outside its 5 ms window")
    if int(metadata["k1op"]) != 10 or int(metadata["k1rel"]) != 8:
        raise ValueError("K1 guard metadata does not match Rev.1")
    if int(metadata["calibration_sequence"]) < 0:
        raise ValueError("invalid active calibration sequence")
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
    raw = text.encode("ascii")
    args.out.write_bytes(raw)
    capture_id = hashlib.sha256(raw).hexdigest()
    print(f"captured {len(capture.rows)} DUT instants to {args.out} sha256={capture_id}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
