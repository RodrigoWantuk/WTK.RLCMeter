#!/usr/bin/env python3
"""Check that STM32 firmware profiles do not link the wrong application shell."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


FORBIDDEN = {
    "PRODUCT": (
        re.compile(r"app_bringup_console"),
        re.compile(r"g_bringup_console"),
        re.compile(r"\blab_"),
    ),
    "BRINGUP": (
        re.compile(r"app_product_"),
        re.compile(r"ui_product_"),
    ),
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--profile", required=True, choices=sorted(FORBIDDEN))
    parser.add_argument("--nm-tool", default="arm-none-eabi-nm")
    args = parser.parse_args()

    if not args.elf.exists():
        print(f"error: ELF not found: {args.elf}", file=sys.stderr)
        return 2

    completed = subprocess.run(
        [args.nm_tool, "--defined-only", str(args.elf)],
        check=False,
        text=True,
        capture_output=True,
    )
    if completed.returncode != 0:
        print(completed.stderr, file=sys.stderr, end="")
        return completed.returncode

    matches: list[str] = []
    for line in completed.stdout.splitlines():
        for pattern in FORBIDDEN[args.profile]:
            if pattern.search(line):
                matches.append(line)
                break

    if matches:
        print(f"error: {args.profile} profile links forbidden symbols:", file=sys.stderr)
        for line in matches[:40]:
            print(f"  {line}", file=sys.stderr)
        if len(matches) > 40:
            print(f"  ... {len(matches) - 40} more", file=sys.stderr)
        return 1

    print(f"profile symbol check: {args.profile} OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
