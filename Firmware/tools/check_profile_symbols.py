#!/usr/bin/env python3
"""Check that STM32 firmware profiles do not link the wrong application shell."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


FORBIDDEN = {
    "PRODUCT": (
        re.compile(r"app_bringup_console"),
        re.compile(r"app_calibration_campaign"),
        re.compile(r"g_bringup_console"),
        re.compile(r"\blab_"),
    ),
    "BRINGUP": (
        re.compile(r"app_product_"),
        re.compile(r"ui_product_"),
    ),
    "BRINGUP_CAL": (
        re.compile(r"app_product_|ui_product_|app_bringup_console|app_calibration_campaign|\blab_"),
    ),
}

CURVE_SOURCES = {"measurement_cal_curve.c", "measurement_cal_curve_frame.c",
                 "measurement_cal_curve_store.c"}
CURVE_SYMBOLS = re.compile(r"measurement_cal_curve_|product_(?:curve_|load_curve_|active_osl_crc32)|g_curve_store")


def capture_composition_errors(commands: list[dict], profile: str) -> list[str]:
    present = {Path(row["file"].replace("\\", "/")).name for row in commands}
    service = {"app_cal_capture_service.c", "app_cal_capture_shell.c"}
    if profile != "BRINGUP_CAL":
        return ["capture development service compiled in " + profile] if present & service else []
    forbidden = {"app_shell.c", "app_bringup_console.c", "app_calibration_campaign.c",
                 "app_product.c", "ui_product.c", "resource_receiver.c", "resource_store.c",
                 "app_settings_store.c", "ili9341.c", "ui_fallback_renderer.c"}
    errors = ["BRINGUP_CAL missing capture source: " + name for name in sorted(service-present)]
    errors += ["BRINGUP_CAL unrelated source: " + name for name in sorted(present & forbidden)]
    return errors


def curve_composition_errors(symbols: str, commands: list[dict], enabled: bool) -> list[str]:
    """Check source membership as well as symbols, which may disappear under LTO."""
    present = {Path(row["file"].replace("\\", "/")).name for row in commands} & CURVE_SOURCES
    errors = []
    if enabled and present != CURVE_SOURCES:
        errors.append("enabled curve runtime sources missing: " + ", ".join(sorted(CURVE_SOURCES - present)))
    if not enabled:
        if present:
            errors.append("disabled curve runtime sources compiled: " + ", ".join(sorted(present)))
        errors.extend(line for line in symbols.splitlines() if CURVE_SYMBOLS.search(line))
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--profile", required=True, choices=sorted(FORBIDDEN))
    parser.add_argument("--nm-tool", default="arm-none-eabi-nm")
    parser.add_argument("--supplementary-curves", choices=("ON", "OFF"))
    parser.add_argument("--compile-commands", type=Path)
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
    if args.supplementary_curves is not None:
        if args.compile_commands is None:
            parser.error("--supplementary-curves requires --compile-commands")
        try:
            commands = json.loads(args.compile_commands.read_text(encoding="utf-8"))
            matches.extend(capture_composition_errors(commands, args.profile))
            matches.extend(curve_composition_errors(completed.stdout, commands,
                                                    args.supplementary_curves == "ON"))
        except (OSError, ValueError, KeyError, TypeError) as error:
            print(f"error: cannot verify curve source composition: {error}", file=sys.stderr)
            return 2
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
