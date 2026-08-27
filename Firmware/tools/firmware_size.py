#!/usr/bin/env python3
"""Report STM32 firmware Flash/RAM usage and enforce project budgets."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

FLASH_SILICON_BYTES = 64 * 1024
RAM_SILICON_BYTES = 20 * 1024
PROJECT_FLASH_SOFT_BYTES = 48 * 1024
PROJECT_FLASH_HARD_BYTES = 56 * 1024


def run_tool(argv: list[str]) -> str:
    completed = subprocess.run(argv, check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return completed.stdout


def resolve_tool(explicit: str | None, fallback: str) -> str:
    if explicit:
        return explicit
    found = shutil.which(fallback)
    if found:
        return found
    raise SystemExit(f"error: unable to find {fallback}; pass --{fallback.replace('-', '_')}-tool")


def parse_size(output: str) -> dict[str, int]:
    lines = [line.split() for line in output.splitlines() if line.strip()]
    for parts in lines:
        if len(parts) >= 6 and parts[0].isdigit():
            text = int(parts[0])
            data = int(parts[1])
            bss = int(parts[2])
            return {
                "text": text,
                "data": data,
                "bss": bss,
                "flash_bytes": text + data,
                "ram_static_bytes": data + bss,
            }
    raise ValueError("unable to parse size output")


def parse_sections(output: str) -> dict[str, int]:
    sections: dict[str, int] = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0].isdigit():
            name = parts[1]
            try:
                sections[name] = int(parts[2], 16)
            except ValueError:
                continue
    return sections


def parse_nm(output: str, limit: int) -> list[dict[str, object]]:
    symbols: list[dict[str, object]] = []
    for line in output.splitlines():
        parts = line.split(maxsplit=3)
        if len(parts) != 4:
            continue
        _, size_text, kind, name = parts
        try:
            size = int(size_text, 16)
        except ValueError:
            try:
                size = int(size_text)
            except ValueError:
                continue
        if size == 0:
            continue
        symbols.append({"size": size, "type": kind, "name": name})
    symbols.sort(key=lambda item: int(item["size"]), reverse=True)
    return symbols[:limit]


def percent(value: int, total: int) -> float:
    return (100.0 * float(value)) / float(total)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--budget", choices=("none", "release", "bringup"), default="none")
    parser.add_argument("--json-out", type=Path)
    parser.add_argument("--size-tool")
    parser.add_argument("--objdump-tool")
    parser.add_argument("--nm-tool")
    parser.add_argument("--nm-limit", type=int, default=20)
    args = parser.parse_args()

    if not args.elf.exists():
        raise SystemExit(f"error: ELF not found: {args.elf}")

    size_tool = resolve_tool(args.size_tool, "arm-none-eabi-size")
    objdump_tool = resolve_tool(args.objdump_tool, "arm-none-eabi-objdump")
    nm_tool = resolve_tool(args.nm_tool, "arm-none-eabi-nm")

    size = parse_size(run_tool([size_tool, "--format=berkeley", str(args.elf)]))
    sections = parse_sections(run_tool([objdump_tool, "-h", str(args.elf)]))
    nm = parse_nm(run_tool([nm_tool, "--print-size", "--size-sort", str(args.elf)]), args.nm_limit)

    reserved_stack_bytes = sections.get("._user_heap_stack", 0)
    noinit_bytes = sections.get(".noinit", 0)
    ram_accounted_bytes = size["ram_static_bytes"] + noinit_bytes + reserved_stack_bytes
    report = {
        "elf": str(args.elf),
        "budget": args.budget,
        **size,
        "flash_percent": percent(size["flash_bytes"], FLASH_SILICON_BYTES),
        "ram_static_percent": percent(size["ram_static_bytes"], RAM_SILICON_BYTES),
        "ram_accounted_bytes": ram_accounted_bytes,
        "ram_accounted_percent": percent(ram_accounted_bytes, RAM_SILICON_BYTES),
        "reserved_stack_bytes": reserved_stack_bytes,
        "noinit_bytes": noinit_bytes,
        "sections": sections,
        "largest_symbols": nm,
        "budgets": {
            "flash_soft_bytes": PROJECT_FLASH_SOFT_BYTES,
            "flash_hard_bytes": PROJECT_FLASH_HARD_BYTES,
            "flash_silicon_bytes": FLASH_SILICON_BYTES,
            "ram_silicon_bytes": RAM_SILICON_BYTES,
        },
    }

    print(f"Firmware size: {args.elf}")
    print(
        f"  Flash: {size['flash_bytes']} B / {FLASH_SILICON_BYTES} B "
        f"({report['flash_percent']:.2f}%)"
    )
    print(
        f"  RAM static: {size['ram_static_bytes']} B / {RAM_SILICON_BYTES} B "
        f"({report['ram_static_percent']:.2f}%)"
    )
    print(f"  Reserved stack/heap floor: {reserved_stack_bytes} B")
    print(
        f"  RAM accounted: {ram_accounted_bytes} B / {RAM_SILICON_BYTES} B "
        f"({report['ram_accounted_percent']:.2f}%)"
    )
    if nm:
        print("  Largest symbols:")
        for item in nm[: min(len(nm), args.nm_limit)]:
            print(f"    {item['size']:>6} {item['type']} {item['name']}")

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    failed = False
    if args.budget in {"release", "bringup"}:
        if size["flash_bytes"] > PROJECT_FLASH_HARD_BYTES:
            print(
                f"error: Flash {size['flash_bytes']} B exceeds project hard gate "
                f"{PROJECT_FLASH_HARD_BYTES} B",
                file=sys.stderr,
            )
            failed = True
        elif size["flash_bytes"] > PROJECT_FLASH_SOFT_BYTES:
            print(
                f"warning: Flash {size['flash_bytes']} B exceeds soft target "
                f"{PROJECT_FLASH_SOFT_BYTES} B",
                file=sys.stderr,
            )
        if ram_accounted_bytes > RAM_SILICON_BYTES:
            print(
                f"error: accounted RAM {ram_accounted_bytes} B exceeds silicon limit "
                f"{RAM_SILICON_BYTES} B",
                file=sys.stderr,
            )
            failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
