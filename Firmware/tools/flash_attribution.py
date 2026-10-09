#!/usr/bin/env python3
"""Offline GNU ARM linked-Flash attribution; section totals are authoritative.

Object, module, symbol and string views overlap conceptually: never add views.
Only ALLOC+LOAD+CONTENTS sections with a Flash LMA consume programmed bytes.
DWARF locations identify emitted function origins, including inlined callees;
they are not exclusive feature costs or proof of bytes removable by deletion.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

FLASH_START, FLASH_END = 0x08000000, 0x08010000
RAM_START, RAM_END = 0x20000000, 0x20005000
UNKNOWN = "UNATTRIBUTED"
HEX = r"0x[0-9a-fA-F]+"


def canonical_path(value):
    """Remove machine-local roots; preserve archive members and LTO partition IDs."""
    value = value.replace("\\", "/")
    if ".ltrans" in value:
        match = re.search(r"ltrans\d+(?:\.ltrans)?\.o", value)
        return "LTO/" + (match.group(0) if match else "unknown.o")
    archive = re.search(r"([^/]+\.a)\((.+)\)$", value)
    if archive:
        return archive.group(1) + "(" + archive.group(2) + ")"
    for marker in ("Firmware/src/", "Firmware/third_party/", "/src/", "/third_party/"):
        if marker in value:
            return marker.lstrip("/").removeprefix("Firmware/") + value.split(marker, 1)[1]
    if re.match(r"^(?:[A-Za-z]:/|/)", value):
        return value.rsplit("/", 1)[-1]
    return value


def parse_sections(text):
    rows = []
    lines = text.splitlines()
    for index, line in enumerate(lines):
        match = re.match(r"\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+2\*\*\d+", line)
        if not match:
            continue
        name, size, vma, lma, offset = match.groups()
        flags = set(lines[index + 1].strip().split(", ")) if index + 1 < len(lines) else set()
        rows.append(dict(name=name, size=int(size, 16), vma=int(vma, 16),
                         lma=int(lma, 16), file_offset=int(offset, 16), flags=sorted(flags)))
    if not rows or len({r["name"] for r in rows}) != len(rows):
        raise ValueError("missing or duplicate objdump sections")
    flash = [s for s in rows if is_flash(s)]
    if not flash:
        raise ValueError("no loadable Flash sections")
    end = FLASH_START
    for s in sorted(flash, key=lambda s: s["lma"]):
        if s["lma"] < end or s["lma"] + s["size"] > FLASH_END:
            raise ValueError("overlapping or out-of-silicon Flash sections")
        end = s["lma"] + s["size"]
    for s in rows:
        if s["size"] and {"ALLOC", "LOAD", "CONTENTS"} <= set(s["flags"]) and not is_flash(s):
            raise ValueError("loadable section outside STM32 Flash: " + s["name"])
    return rows


def is_flash(section):
    return (section["size"] > 0 and FLASH_START <= section["lma"] < FLASH_END
            and {"ALLOC", "LOAD", "CONTENTS"} <= set(section["flags"]))


def parse_map(text):
    if "Linker script and memory map" not in text:
        raise ValueError("missing GNU linker memory map")
    prefix, body = text.split("Linker script and memory map", 1)
    inclusions = []
    for line in prefix.split("Discarded input sections", 1)[0].splitlines():
        if re.search(r"\.a\([^)]+\)$", line):
            inclusions.append(canonical_path(line.strip()))
    rows, outputs = [], {}
    parent, pending = None, None
    for line in body.splitlines():
        if line.startswith("Cross Reference Table") or line.startswith("OUTPUT("):
            break
        # GNU ld wraps long input/output section names onto a separate line.
        match = re.match(r"^(\s*)(\.[^\s]+|\*fill\*|COMMON)\s*(.*)$", line)
        if match:
            indent, name, rest = match.groups()
            pending = (bool(indent), name)
        elif pending and re.match(r"\s+0x", line):
            rest = line.strip()
        else:
            pending = None
            continue
        numbers = re.match(rf"({HEX})\s+({HEX})(?:\s+(.*))?$", rest)
        if not numbers:
            if rest:
                pending = None
            continue
        is_input, name = pending
        pending = None
        address, size, owner = numbers.groups()
        address, size = int(address, 16), int(size, 16)
        if not is_input:
            parent = name
            if name in outputs:
                raise ValueError("duplicate map output section: " + name)
            outputs[name] = (address, size)
        elif parent and size:
            owner = "LINKER_ALIGNMENT" if name == "*fill*" else canonical_path(owner or UNKNOWN)
            rows.append(dict(section=parent, input_section=name, address=address, size=size, owner=owner))
    if not outputs:
        raise ValueError("empty or malformed linker map")
    return rows, outputs, sorted(set(inclusions))


def parse_nm(text):
    symbols = []
    for line in text.splitlines():
        match = re.match(r"^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([A-Za-z?])\s+([^\t]+?)(?:\t(.+))?$", line.strip())
        if not match:
            # GNU nm also emits legitimate zero-sized/address-only symbols.
            if line.strip() and not re.match(r"^(?:[0-9a-fA-F]+\s+)?[A-Za-z?]\s+\S+", line.strip()):
                raise ValueError("malformed nm row: " + line[:120])
            continue
        address, size, kind, name, source = match.groups()
        if int(size, 16) and kind.upper() in {"T", "R", "D", "B", "W", "V"}:
            source = re.sub(r":\d+(?:\s+.*)?$", "", source or "")
            symbols.append(dict(address=int(address, 16), size=int(size, 16), type=kind,
                                name=name, source=canonical_path(source) if source else UNKNOWN))
    if not symbols:
        raise ValueError("no sized symbols in nm input")
    return sorted(symbols, key=lambda s: (s["address"], s["size"], s["name"], s["source"]))


def ranking(totals):
    return [dict(name=k, bytes=v) for k, v in sorted(totals.items(), key=lambda p: (-p[1], p[0])) if v]


def category(owner):
    if owner.startswith("libgcc.a("):
        return "libgcc (soft-float / ABI helpers)"
    if re.match(r"lib[cg]_nano\.a\(", owner):
        return "newlib-nano"
    if owner.startswith("libc.a("):
        return "newlib"
    if "third_party" in owner or re.search(r"stm32f1xx_.*\.o", owner):
        return "HAL/CMSIS"
    if owner.startswith("LTO/"):
        return "LTO (mixed source ownership)"
    if owner in {"LINKER_ALIGNMENT", "linker stubs"}:
        return "linker generated/alignment"
    return "project/other" if owner != UNKNOWN else UNKNOWN


def analyze(map_text, nm_text, section_text, profile="unknown", git_sha="unknown", binary=None):
    sections = parse_sections(section_text)
    inputs, outputs, inclusions = parse_map(map_text)
    symbols = parse_nm(nm_text)
    objects, modules, families = defaultdict(int), defaultdict(int), defaultdict(int)
    contributions, warnings, ranked_symbols = [], set(), []
    flash_sections = [s for s in sections if is_flash(s)]
    for sec in flash_sections:
        start, stop = sec["vma"], sec["vma"] + sec["size"]
        if outputs.get(sec["name"]) != (start, sec["size"]):
            raise ValueError("map/ELF output section mismatch: " + sec["name"])
        owned = [r for r in inputs if r["section"] == sec["name"] and r["address"] < stop and r["address"] + r["size"] > start]
        live = [s for s in symbols if s["address"] < stop and s["address"] + s["size"] > start]
        boundaries = {start, stop}
        for row in owned + live:
            boundaries.update((max(start, row["address"]), min(stop, row["address"] + row["size"])))
        # Merged strings may retain pre-relaxation input lengths in GNU maps.
        for row in [r for r in inputs if r["section"] == sec["name"]]:
            if row["address"] < start or row["address"] + row["size"] > stop:
                warnings.add("input range exceeds output (relaxation/merge): " + sec["name"] + "/" + row["owner"])
        boundaries = sorted(boundaries)
        for left, right in zip(boundaries, boundaries[1:]):
            covering = [r for r in owned if r["address"] <= left and r["address"] + r["size"] >= right]
            owners = {r["owner"] for r in covering}
            owner = next(iter(owners)) if len(owners) == 1 else UNKNOWN
            if any(r["address"] < start or r["address"] + r["size"] > stop for r in covering):
                owner = UNKNOWN
            sym = [s for s in live if s["address"] <= left and s["address"] + s["size"] >= right]
            sources = {s["source"] for s in sym}
            source = next(iter(sources)) if len(sources) == 1 else UNKNOWN
            if source == UNKNOWN and len(sources) <= 1 and owner.startswith("src/"):
                source = re.sub(r"\.(?:obj|o)$", "", owner)
            if source == UNKNOWN and len(sources) <= 1 and re.match(r"lib(?:gcc|[cg](?:_nano)?|nosys)\.a\(", owner):
                source = owner
            size = right - left
            objects[owner] += size
            modules[source] += size
            families[category(owner)] += size
            contributions.append(dict(section=sec["name"], address=left, lma=sec["lma"] + left - start,
                                      bytes=size, object=owner, module=source))
        # Exact aliases grouped; overlapping non-identical symbols explicitly flagged.
        grouped = defaultdict(list)
        for s in live:
            if s["address"] >= start and s["address"] + s["size"] <= stop:
                grouped[(s["address"], s["size"])].append(s)
            else:
                warnings.add("symbol extends outside output: " + s["name"])
        for (address, size), aliases in grouped.items():
            other_overlap = any(a < address + size and a + n > address and (a, n) != (address, size) for a, n in grouped)
            ranked_symbols.append(dict(address=address, bytes=size, section=sec["name"],
                                       names=sorted({a["name"] for a in aliases}),
                                       sources=sorted({a["source"] for a in aliases}),
                                       overlaps_other_symbols=other_overlap))
    total = sum(s["size"] for s in flash_sections)
    span = max(s["lma"] + s["size"] for s in flash_sections) - FLASH_START
    ram = sum(s["size"] for s in sections if "ALLOC" in s["flags"] and RAM_START <= s["vma"] < RAM_END)
    if any(s["vma"] + s["size"] > RAM_END for s in sections if "ALLOC" in s["flags"] and RAM_START <= s["vma"] < RAM_END):
        raise ValueError("RAM section exceeds silicon")
    strings, constant_groups = [], defaultdict(list)
    if binary is not None:
        if len(binary) != span:
            raise ValueError("BIN length does not match Flash load span")
        for sec in flash_sections:
            if not sec["name"].startswith(".rodata"):
                continue
            offset = sec["lma"] - FLASH_START
            for match in re.finditer(rb"[\x20-\x7e]{4,}\x00", binary[offset:offset + sec["size"]]):
                strings.append(dict(address=sec["vma"] + match.start(), bytes=len(match.group()),
                                    text=match.group()[:-1].decode("ascii")))
            for symbol in ranked_symbols:
                if symbol["section"] != sec["name"] or symbol["overlaps_other_symbols"]:
                    continue
                begin = offset + symbol["address"] - sec["vma"]
                payload = binary[begin:begin + symbol["bytes"]]
                if len(payload) >= 8:
                    constant_groups[hashlib.sha256(payload).hexdigest()].append(symbol)
    return dict(schema_version=1, profile=profile, git_sha=git_sha,
                flash_bytes=total, flash_load_span_bytes=span, flash_gap_bytes=span-total,
                flash_remaining_bytes=FLASH_END-FLASH_START-span,
                ram_accounted_bytes=ram, ram_remaining_bytes=RAM_END-RAM_START-ram,
                binary_sha256=hashlib.sha256(binary).hexdigest() if binary is not None else None,
                sections=sections, objects=ranking(objects), modules=ranking(modules),
                runtime_families=ranking(families), contributions=contributions,
                symbols=sorted(ranked_symbols, key=lambda s: (-s["bytes"], s["address"], s["names"])),
                string_candidates=sorted(strings, key=lambda s: (-s["bytes"], s["address"])),
                duplicate_constant_candidates=[dict(sha256=k, symbols=v) for k, v in sorted(constant_groups.items()) if len(v) > 1],
                formatting_symbols=sorted({s["name"] for s in symbols if re.search(r"printf|scanf|dtoa|vfprintf", s["name"])}),
                archive_inclusions=inclusions, warnings=sorted(warnings))


def compare(baseline, candidate):
    if not isinstance(baseline, dict) or not isinstance(candidate, dict):
        raise ValueError("attribution report must be a JSON object")
    if baseline.get("schema_version") != 1 or candidate.get("schema_version") != 1:
        raise ValueError("unsupported attribution schema")
    result = {"baseline": baseline["profile"], "candidate": candidate["profile"]}
    for key in ("flash_bytes", "flash_load_span_bytes", "ram_accounted_bytes"):
        result[key + "_delta"] = candidate[key] - baseline[key]
    for view in ("objects", "modules"):
        old = {r["name"]: r["bytes"] for r in baseline[view]}
        new = {r["name"]: r["bytes"] for r in candidate[view]}
        result[view] = [dict(name=k, baseline_bytes=old.get(k, 0), candidate_bytes=new.get(k, 0),
                             delta_bytes=new.get(k, 0)-old.get(k, 0)) for k in sorted(old.keys() | new.keys())]
    return result


def csv_report(report):
    stream = io.StringIO(newline="")
    writer = csv.writer(stream, lineterminator="\n")
    writer.writerow(("view", "name", "bytes", "address", "section"))
    for row in report["sections"]:
        if is_flash(row):
            writer.writerow(("flash_sections", row["name"], row["size"], hex(row["lma"]), row["name"]))
    for view in ("objects", "modules", "runtime_families"):
        for row in report[view]:
            writer.writerow((view, row["name"], row["bytes"], "", ""))
    for row in report["symbols"]:
        writer.writerow(("symbols_nonadditive", " / ".join(row["names"]), row["bytes"], hex(row["address"]), row["section"]))
    for row in report["string_candidates"]:
        writer.writerow(("strings_heuristic_nonadditive", row["text"], row["bytes"], hex(row["address"]), ".rodata"))
    for view in ("objects", "modules"):
        for row in report.get("comparison", {}).get(view, []):
            writer.writerow((view + "_delta", row["name"], row["delta_bytes"], "", ""))
    return stream.getvalue()


def markdown_report(report):
    lines = ["# Flash attribution: " + report["profile"], "", "Source SHA: `" + report["git_sha"] + "`.", "",
             f"Flash sections: **{report['flash_bytes']} B**; load span: {report['flash_load_span_bytes']} B; "
             f"RAM accounted: **{report['ram_accounted_bytes']} B**.", "",
             "Views are alternatives, not additive. Module locations identify emitted function origins; "
             "LTO inlining can put other modules inside them. Symbol aliases are grouped; overlapping symbols "
             "are not additive. String candidates are heuristic printable NUL-terminated ranges, not guaranteed literals.", ""]
    for view in ("objects", "modules", "runtime_families"):
        lines += ["## " + view.replace("_", " ").title(), "", "| Name | Linked bytes |", "| --- | ---: |"]
        lines += [f"| `{r['name']}` | {r['bytes']} |" for r in report[view][:20]]
        lines.append("")
    for title, rows in (("Largest functions", [r for r in report["symbols"] if r["section"] == ".text"]),
                        ("Largest constant symbols", [r for r in report["symbols"] if r["section"].startswith(".rodata")])):
        lines += ["## " + title, "", "| Symbol aliases | Bytes (nonadditive) | Overlap |", "| --- | ---: | --- |"]
        lines += [f"| `{' / '.join(r['names'])}` | {r['bytes']} | {r['overlaps_other_symbols']} |" for r in rows[:20]]
        lines.append("")
    lines += ["## String candidates", "", "| Text | Bytes including NUL |", "| --- | ---: |"]
    for row in report["string_candidates"][:15]:
        safe = row["text"].replace("|", "\\|").replace("`", "'")
        lines.append(f"| `{safe}` | {row['bytes']} |")
    if "comparison" in report:
        lines += ["", "## Comparison (candidate minus baseline)", "", "```json",
                  json.dumps({k: v for k, v in report["comparison"].items() if k not in {"objects", "modules"}}, indent=2), "```"]
    lines += ["", f"Identical constant-symbol candidates (>=8 B): {len(report['duplicate_constant_candidates'])}.",
              "Formatting symbols: " + (", ".join(report["formatting_symbols"]) or "none") + ".",
              "", "## Parser observations", ""] + ["- " + w for w in report["warnings"]]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--map", required=True, type=Path)
    parser.add_argument("--nm", required=True, type=Path, help="GNU nm -S --size-sort --line-numbers output")
    parser.add_argument("--sections", required=True, type=Path, help="GNU objdump -h output")
    parser.add_argument("--bin", type=Path)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--git-sha", required=True)
    parser.add_argument("--baseline", type=Path, help="attribution JSON for candidate-minus-baseline diff")
    parser.add_argument("--json-out", required=True, type=Path)
    parser.add_argument("--csv-out", type=Path)
    parser.add_argument("--markdown-out", type=Path)
    args = parser.parse_args()
    try:
        report = analyze(args.map.read_text(encoding="utf-8-sig"), args.nm.read_text(encoding="utf-8-sig"),
                         args.sections.read_text(encoding="utf-8-sig"), args.profile, args.git_sha,
                         args.bin.read_bytes() if args.bin else None)
        if args.baseline:
            report["comparison"] = compare(json.loads(args.baseline.read_text(encoding="utf-8")), report)
        for path, content in ((args.json_out, json.dumps(report, indent=2, sort_keys=True) + "\n"),
                              (args.csv_out, csv_report(report)), (args.markdown_out, markdown_report(report))):
            if path:
                path.parent.mkdir(parents=True, exist_ok=True)
                with path.open("w", encoding="utf-8", newline="\n") as stream:
                    stream.write(content)
        print(f"{args.profile}: Flash={report['flash_bytes']} RAM={report['ram_accounted_bytes']} B")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("error: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
