#!/usr/bin/env python3
"""Capture one experimental BRINGUP 1 MOhm DC pilot over COM.

This is evidence collection, not a calibrated DCR or insulation test.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import time


MAX_LINE_BYTES = 160
MAX_LINES = 6
RESULT_STATES = {
    "EXPLORATORY", "CURRENT_UNRESOLVED", "DUT_VOLTAGE_UNRESOLVED",
    "CLIPPED", "SOURCE_OUT_OF_RANGE", "INVALID",
}


def validate_pilot(text: str) -> dict[str, str]:
    lines = text.splitlines()
    if (not 3 <= len(lines) <= MAX_LINES or
            any(len(line) > MAX_LINE_BYTES or not line.isascii() for line in lines)):
        raise ValueError("invalid bounded DC pilot frame")
    if (not lines[0].startswith("DC_PILOT_BEGIN range=1m ac=1khz,100mv ccr=81 adc_cal=") or
            lines[0].split("adc_cal=", 1)[1] not in ("PERSISTED", "IDEAL_UNQUALIFIED") or
            lines.count("DC_PILOT_AC_OK residual=REQUALIFIED") != 1 or
            lines[-1] != "DC_PILOT_END status=SAFE"):
        raise ValueError("pilot did not complete two safe transactions")
    result_lines = [line for line in lines if line.startswith("DC_PILOT_RESULT status=")]
    if len(result_lines) != 1 or len(lines) != 4:
        raise ValueError("pilot result missing or repeated")
    fields = {}
    for item in result_lines[0].split()[1:]:
        key, separator, value = item.partition("=")
        if not separator or key in fields:
            raise ValueError("invalid or duplicate DC pilot result field")
        fields[key] = value
    if fields.get("status") not in RESULT_STATES:
        raise ValueError("unknown DC pilot result state")
    measured = fields["status"] in {
        "EXPLORATORY", "CURRENT_UNRESOLVED", "DUT_VOLTAGE_UNRESOLVED",
    }
    numeric_fields = ("source_uv", "dut_uv", "current_na")
    if measured != all(key in fields for key in numeric_fields):
        raise ValueError("incomplete DC pilot voltage/current evidence")
    for key in numeric_fields:
        if key in fields:
            value = int(fields[key])
            if abs(value) > 1000000:
                raise ValueError("DC pilot value exceeds bounded format")
    if fields["status"] == "EXPLORATORY":
        resistance = fields.get("resistance_ohm")
        if resistance is None or (resistance != "OUT_OF_RANGE" and int(resistance) < 0):
            raise ValueError("exploratory result needs nonnegative resistance")
    elif "resistance_ohm" in fields:
        raise ValueError("unresolved pilot must not report resistance")
    return {"adc_cal": lines[0].split("adc_cal=", 1)[1], **fields}


def receive_pilot(link, timeout_seconds: float = 10.0) -> tuple[str, dict[str, str]]:
    if timeout_seconds <= 0.0:
        raise ValueError("timeout must be positive")
    deadline = time.monotonic() + timeout_seconds
    lines: list[str] = []
    while time.monotonic() < deadline:
        raw = link.readline(MAX_LINE_BYTES + 1)
        if not raw:
            continue
        if len(raw) > MAX_LINE_BYTES:
            raise ValueError("serial line exceeds DC pilot format")
        line = raw.decode("ascii", errors="strict").strip()
        if not lines and not line.startswith("DC_PILOT_BEGIN "):
            continue
        lines.append(line)
        if len(lines) > MAX_LINES:
            raise ValueError("DC pilot frame too long")
        if line.startswith("DC_PILOT_END "):
            text = "\n".join(lines) + "\n"
            return text, validate_pilot(text)
    raise TimeoutError("no complete safe DC pilot result received")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--ack-current-limited-bench", action="store_true",
                        help="explicitly authorize a physical BRINGUP pilot with controlled DUT")
    args = parser.parse_args()
    if not args.ack_current_limited_bench:
        parser.error("a current-limited bench and explicit acknowledgement are required")
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError:
        parser.error("pyserial is required for COM capture")
    try:
        with serial.Serial(port=args.port, baudrate=args.baud, timeout=0.25) as link:
            link.write(b"lab dc pilot\r\n")
            text, result = receive_pilot(link, args.timeout)
    except (OSError, UnicodeError, ValueError, TimeoutError) as exc:
        parser.error(str(exc))
    raw = text.encode("ascii")
    args.out.write_bytes(raw)
    print(f"captured provisional {result['status']} pilot to {args.out} "
          f"sha256={hashlib.sha256(raw).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
