#!/usr/bin/env python3
"""Collect reproducible local ARM evidence and an optional compact review snapshot.

Build first using a supported preset. Raw artifacts stay in the build directory.
A diagnostic twin can supply DWARF locations only after BIN byte equality is proven.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

import flash_attribution as attribution


def run(argv):
    return subprocess.run(argv, check=True, text=True, capture_output=True).stdout


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(text)


def enrich_nm(original, diagnostic, original_bin, diagnostic_bin):
    if original_bin != diagnostic_bin:
        raise ValueError("diagnostic twin BIN differs; source attribution refused")
    locations = {(s["address"], s["size"], s["name"]): s["source"] for s in attribution.parse_nm(diagnostic)}
    rows = []
    for s in attribution.parse_nm(original):
        source = locations.get((s["address"], s["size"], s["name"]), s["source"])
        row = f"{s['address']:08x} {s['size']:08x} {s['type']} {s['name']}"
        rows.append(row + ("\t" + source if source != attribution.UNKNOWN else ""))
    return "\n".join(rows) + "\n"


def compact_json(report):
    """One record per line keeps review snapshots small without losing fields."""
    fields = []
    for key, value in sorted(report.items()):
        encoded = json.dumps(value, sort_keys=True)
        if isinstance(value, list) and value:
            encoded = "[\n    " + ",\n    ".join(json.dumps(row, sort_keys=True) for row in value) + "\n  ]"
        fields.append("  " + json.dumps(key) + ": " + encoded)
    return "{\n" + ",\n".join(fields) + "\n}\n"


def build_graph_commands(root, cache):
    # CMake --build --target help can regenerate build.ninja against edited
    # sources. Ninja tool mode only reads the saved graph; never regenerate an
    # evidence directory merely to list its targets or commands.
    if "CMAKE_GENERATOR:INTERNAL=Ninja" in cache:
        return {"build-targets.txt": ["ninja", "-C", str(root), "-t", "targets", "all"],
                "commands.txt": ["ninja", "-C", str(root), "-t", "commands"]}
    return {}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--git-sha", required=True)
    parser.add_argument("--source-build-dir", type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--compact-out", type=Path)
    args = parser.parse_args()
    try:
        root = args.build_dir
        elf, binary = root / "WTK.RLCMeter.elf", (root / "WTK.RLCMeter.bin").read_bytes()
        raw = root / "forensics"
        outputs = {}
        for name, command in {
            "size": ["arm-none-eabi-size", "-A", str(elf)],
            "nm": ["arm-none-eabi-nm", "--print-size", "--size-sort", "--line-numbers", str(elf)],
            "sections": ["arm-none-eabi-objdump", "-h", str(elf)],
            "disassembly": ["arm-none-eabi-objdump", "-d", str(elf)],
        }.items():
            outputs[name] = run(command)
            write(raw / (name + ".txt"), outputs[name])
        if args.source_build_dir:
            diagnostic_bin = (args.source_build_dir / "WTK.RLCMeter.bin").read_bytes()
            diagnostic_nm = run(["arm-none-eabi-nm", "--print-size", "--size-sort", "--line-numbers",
                                 str(args.source_build_dir / "WTK.RLCMeter.elf")])
            outputs["nm"] = enrich_nm(outputs["nm"], diagnostic_nm, binary, diagnostic_bin)
            write(raw / "nm-source.txt", outputs["nm"])
        report = attribution.analyze((root / "WTK.RLCMeter.map").read_text(encoding="utf-8"), outputs["nm"],
                                     outputs["sections"], args.profile, args.git_sha, binary)
        size_report = json.loads((root / "WTK.RLCMeter.size.json").read_text(encoding="utf-8"))
        if report["flash_bytes"] != size_report["flash_bytes"]:
            raise ValueError("existing size tool mismatch: flash_bytes")
        if report["ram_accounted_bytes"] != size_report["ram_accounted_bytes"]:
            # Preserve old baseline metadata rather than rewriting historical evidence.
            duplicate = size_report["reserved_stack_bytes"] + size_report["noinit_bytes"]
            if "size_bss_aggregate_bytes" in size_report or report["ram_accounted_bytes"] + duplicate != size_report["ram_accounted_bytes"]:
                raise ValueError("existing size tool mismatch: ram_accounted_bytes")
            report["legacy_size_tool_ram_accounted_bytes"] = size_report["ram_accounted_bytes"]
            report["legacy_size_tool_double_count_bytes"] = duplicate
        if args.baseline:
            report["comparison"] = attribution.compare(json.loads(args.baseline.read_text(encoding="utf-8")), report)
        report["source_locations"] = "byte-identical diagnostic twin" if args.source_build_dir else "image nm/DWARF only"
        report["versions"] = {tool: run([tool, "--version"]).splitlines()[0]
                              for tool in ("arm-none-eabi-gcc", "arm-none-eabi-ld", "cmake", "ninja")}
        report["versions"]["python"] = sys.version.split()[0]
        report["artifacts"] = {suffix: dict(bytes=(root / ("WTK.RLCMeter." + suffix)).stat().st_size,
                                            sha256=hashlib.sha256((root / ("WTK.RLCMeter." + suffix)).read_bytes()).hexdigest())
                               for suffix in ("elf", "map", "bin", "size.json")}
        # Paths and temporary LTO names are intentionally excluded from review metadata.
        cache = (root / "CMakeCache.txt").read_text(encoding="utf-8")
        keys = ("CMAKE_BUILD_TYPE", "CMAKE_C_FLAGS", "CMAKE_C_FLAGS_DEBUG", "CMAKE_C_FLAGS_RELEASE",
                "CMAKE_C_FLAGS_MINSIZEREL", "CMAKE_EXE_LINKER_FLAGS", "CMAKE_INTERPROCEDURAL_OPTIMIZATION",
                "WTK_FIRMWARE_PROFILE", "WTK_PRODUCT_OPTIMIZATION_LEVEL", "WTK_FLASH_FORENSICS",
                "WTK_ENABLE_SUPPLEMENTARY_CURVES")
        report["configuration"] = dict(re.findall(r"^(" + "|".join(keys) + r"):[^=]+=(.*)$", cache, re.MULTILINE))
        for name, command in build_graph_commands(root, cache).items():
            write(raw / name, run(command))
        write(raw / "attribution.json", json.dumps(report, indent=2, sort_keys=True) + "\n")
        write(raw / "attribution.csv", attribution.csv_report(report))
        write(raw / "attribution.md", attribution.markdown_report(report))
        if args.compact_out:
            compact = {k: v for k, v in report.items() if k not in {"contributions", "symbols", "string_candidates"}}
            compact["symbols"] = report["symbols"][:30]
            compact["constants"] = [s for s in report["symbols"] if s["section"].startswith(".rodata")][:20]
            compact["string_candidates"] = report["string_candidates"][:15]
            compact["sections"] = [s for s in report["sections"] if "ALLOC" in s["flags"]]
            compact["non_allocated_section_bytes"] = sum(s["size"] for s in report["sections"] if "ALLOC" not in s["flags"])
            compact["note"] = "Compact snapshot; symbols/strings truncated. Reproduce full evidence using collect_flash_evidence.py."
            write(args.compact_out, compact_json(compact))
        print(f"{args.profile}: {report['flash_bytes']} B Flash, {report['ram_accounted_bytes']} B RAM; evidence={raw}")
        return 0
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print("error: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
