#!/usr/bin/env python3
"""Read-only reconciliation of the Rev.1 EasyEDA archive and fabrication exports.

Standard library only. Export connectivity is not routed-copper DRC or an
as-built continuity measurement. The accepted population decisions live in the
review CSV, not in this extractor.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import re
from urllib.parse import unquote
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path("PCB/source/ProPrj_WTK RLC Meter_2026-08-19.epro2")
FAB = Path("PCB/fabrication/Rev1")
BOM = FAB / "BOM_RLC-PCB_2026-08-23.csv"
GERBER = FAB / "Gerber_RLC-1_2026-08-23.zip"


class AuditError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise AuditError(message)


def parse_epru(content):
    """Resolve record IDs within each document, including null tombstones."""
    documents = []
    current = None
    for line, raw in enumerate(content.splitlines(), 1):
        if not raw.strip():
            continue
        require("||" in raw,
                f"unsupported record envelope at line {line}")
        header_text, body_text = raw.split("||", 1)
        header = json.loads(header_text)
        body_text = body_text.strip()
        if body_text.endswith("|"):
            body_text = body_text[:-1].strip()
        body = json.loads(body_text) if body_text else None
        if header["type"] == "DOCHEAD":
            require(isinstance(body, dict), f"invalid DOCHEAD at {line}")
            current = {"head": body, "records": {}}
            documents.append(current)
        else:
            require(current is not None, f"record before DOCHEAD at {line}")
            key = (header["type"], header.get("id", f"line:{line}"))
            current["records"][key] = (line, header, body)
    require(documents, "empty project")
    return documents


def live(document, kind):
    return [(line, header, body) for line, header, body in
            document["records"].values() if header["type"] == kind and body is not None]


def attributes(document):
    result = {}
    for _, _, body in live(document, "ATTR"):
        result.setdefault(body["parentId"], {})[body["key"]] = body["value"]
    return result


def extract_source(documents):
    by_uuid = {d["head"]["uuid"]: d for d in documents}
    pcbs = [d for d in documents if d["head"]["docType"] == "PCB"]
    require(len(pcbs) == 1, "expected exactly one PCB document")
    pcb = pcbs[0]
    attrs = attributes(pcb)
    components = {}
    ids = {}
    for line, header, body in live(pcb, "COMPONENT"):
        component_id = header["id"]
        attr = attrs.get(component_id, {})
        ref = attr.get("Designator")
        require(ref and ref not in components, f"duplicate/missing reference: {ref}")
        device = by_uuid[attr["Device"]]
        meta = live(device, "META")[0][2]["attributes"]
        footprint = by_uuid[attr["Footprint"]]
        symbol = by_uuid[meta["Symbol"]]
        pin_attrs = attributes(symbol)
        pin_names = {a["Pin Number"]: a["Pin Name"] for a in pin_attrs.values()
                     if "Pin Number" in a and "Pin Name" in a}
        pads = {b["num"]: b for _, _, b in live(footprint, "PAD")}
        require(len(pads) == len(live(footprint, "PAD")), f"duplicate library pad: {ref}")
        components[ref] = {"line": line, "placement": body,
                           "part": meta.get("Manufacturer Part", ""),
                           "value": attrs.get(component_id, {}).get("Value", meta.get("Value", "")),
                           "footprint": live(footprint, "META")[0][2]["title"],
                           "pads": pads, "pin_names": pin_names}
        ids[component_id] = ref
    pins = {}
    for line, header, body in live(pcb, "PAD_NET"):
        key = json.loads(header["id"])
        require(len(key) == 4 and key[0] == "PAD_NET", f"unknown PAD_NET at {line}")
        ref, number = ids[key[1]], str(key[2])
        require((ref, number) not in pins, f"duplicate net pad {ref}.{number}")
        component = components[ref]
        pad = component["pads"][number]
        placement = component["placement"]
        radians = math.radians(placement["angle"])
        x, y = pad["centerX"], pad["centerY"]
        pins[(ref, number)] = {
            "net": body["padNet"], "line": line,
            "x": placement["x"] + x * math.cos(radians) - y * math.sin(radians),
            "y": placement["y"] + x * math.sin(radians) + y * math.cos(radians),
            "hole": (pad.get("hole") or {}).get("width", 0),
        }
    expected = {(ref, number) for ref, component in components.items()
                for number in component["pads"]}
    require(set(pins) == expected, "source physical pads and PAD_NET records differ")
    return components, pins


def read_bom(content):
    rows = list(csv.DictReader(io.StringIO(content.decode("utf-16")), delimiter="\t"))
    refs = {}
    for row in rows:
        names = row["Designator"].split(",")
        require(len(names) == int(row["Quantity"]), f"BOM quantity mismatch {row['No.']}")
        for ref in names:
            require(ref and ref not in refs, f"duplicate/blank BOM reference: {ref}")
            refs[ref] = row
    return rows, refs


def fabrication_pins(flying, refs):
    require(flying["lengthUnit"] == "mil", "unknown fabrication coordinate units")
    pins = {}
    for values in flying["pins"]["rows"]:
        pin = dict(zip(flying["pins"]["fields"], values))
        ref, number = pin["PIN_NAME"].rsplit("_", 1)
        if ref not in refs:
            require(re.fullmatch(r"PAD\d+", ref) is not None,
                    f"unexpected fabrication-only component {ref}")
            continue
        key = (ref, number)
        value = {"net": pin["NET_NAME"], "x": pin["PIN_X"], "y": pin["PIN_Y"],
                 "hole": pin["HOLE_SIZE"]}
        require(key not in pins or pins[key] == value,
                f"conflicting repeated fabrication pad {key}")
        pins[key] = value
    return pins


def reconcile_pins(source, fabrication, position_exceptions=None):
    require(set(source) == set(fabrication), "source/fabrication pad set mismatch")
    groups = {}
    reverse = {}
    aliases = {}
    for key, pin in source.items():
        other = fabrication[key]
        delta = tuple(round(other[n] - pin[n], 3) for n in ("x", "y", "hole"))
        allowed = (position_exceptions or {}).get(key, (0, 0, 0))
        require(max(abs(delta[n] - allowed[n]) for n in range(3)) < 0.02,
                f"unreviewed pad position/drill mismatch: {key}, delta={delta}")
        # Unnamed source pads are individually unconnected, NOT one shared net.
        source_net = pin["net"] or ("unconnected", key)
        net = other["net"]
        require(source_net not in groups or groups[source_net] == net,
                f"fabrication splits source net {source_net}")
        require(net not in reverse or reverse[net] == source_net,
                f"fabrication merges source nets into {net}")
        groups[source_net] = net
        reverse[net] = source_net
        if pin["net"] and pin["net"] != net:
            aliases[pin["net"]] = net
    return aliases


def check_top_copper(text, pins):
    """Check explicit top-layer pad flashes; deliberately not a Gerber DRC."""
    require("%FSLAX45Y45*%" in text and "%MOMM*%" in text,
            "unsupported Gerber units/precision")
    flashes = {(int(x) / 100000, int(y) / 100000) for x, y in
               re.findall(r"X(-?\d+)Y(-?\d+)D03", text)}
    for key, pin in pins.items():
        x, y = pin["x"] * 0.0254, pin["y"] * 0.0254
        require(any(abs(x - fx) < 0.001 and abs(y - fy) < 0.001 for fx, fy in flashes),
                f"missing top copper pad flash: {key}")
    return len(pins)


def check_drills(text, pins):
    require("METRIC,LZ,0000.00000" in text, "unsupported Excellon format")
    tools = {int(n): float(size) for n, size in re.findall(r"T(\d+)C([\d.]+)", text)}
    current = None
    holes = []
    for line in text.splitlines():
        select = re.fullmatch(r"T(\d+)", line)
        point = re.fullmatch(r"X(-?[\d.]+)Y(-?[\d.]+)", line)
        if select:
            current = tools[int(select[1])]
        elif point:
            require(current is not None, "drill hit before tool selection")
            holes.append((float(point[1]), float(point[2]), current))
    count = 0
    for key, pin in pins.items():
        if pin["hole"]:
            expected = tuple(pin[n] * 0.0254 for n in ("x", "y", "hole"))
            require(any(max(abs(a - b) for a, b in zip(expected, hole)) < 0.002
                        for hole in holes), f"missing component drill: {key}")
            count += 1
    return count


def gpio_outputs(text):
    pattern = (r"case BSP_GPIO_OUTPUT_(\w+):\s*port = GPIO([ABC]);"
               r"\s*pin = (\d+)u;\s*break;")
    result = {f"P{port}{pin}": name for name, port, pin in re.findall(pattern, text)}
    require(len(result) == len(re.findall(r"case BSP_GPIO_OUTPUT_", text)),
            "unsupported or duplicate GPIO output mapping")
    return result


def check_firmware(root, components, pins):
    folder = root / "Firmware/src/bsp"
    gpio = (folder / "bsp_gpio.c").read_text()
    module = {name: number for number, name in components["USTM32"]["pin_names"].items()}
    outputs = gpio_outputs(gpio)
    require(set(outputs.values()) == {"FLASH_CS", "TFT_CS", "TFT_DC", "TFT_RST", "TFT_BL", "BUZZER",
                                      "K1_CMD", "K2_CMD", "RANGE_A0", "RANGE_A1", "RANGE_A2", "RANGE_EN"},
            "GPIO output capability drift")
    for pin, role in outputs.items():
        net = pins[("USTM32", module[pin])]["net"]
        require(net == role or (role == "BUZZER" and net == "IO_BUZZ"), f"GPIO/source drift {pin}: {role}/{net}")
    inputs = re.findall(r"case BSP_GPIO_INPUT_(\w+):\s*\*active = (!?)gpio_read\(GPIO([ABC]), (\d+)u\);", gpio)
    expected_inputs = {"BUTTON_UP": ("PB3", True), "BUTTON_DOWN": ("PB4", True),
                       "BUTTON_OK": ("PC13", True), "CHARGER_DETECT": ("PA15", False)}
    require({name: (f"P{port}{pin}", bool(inv)) for name, inv, port, pin in inputs} == expected_inputs,
            "input pin/polarity drift")
    for pin in ("PA8", "PA11", "PB8", "PB9"):
        require(f"gpio_clear(GPIO{pin[1]}, {pin[2:]}u)" in gpio, f"unsafe boot level {pin}")
    require("AFIO_MAPR_SWJ_CFG_JTAGDISABLE" in gpio, "SWD/JTAG contract drift")
    checks = {
        "bsp_uart.c": ["gpio_config_pin(GPIOA, 9u", "gpio_config_pin(GPIOA, 10u", "USART1"],
        "bsp_spi.c": [f"gpio_config_pin(GPIOB, {n}u" for n in (13, 14, 15)] + ["SPI2"],
        "bsp_metrology_adc.c": [f"gpio_config_analog(GPIOA, {n}u)" for n in range(4)],
        "bsp_adc.c": [f"gpio_config_analog(GPIOA, {n}u)" for n in (1, 4, 5, 6, 7)],
        "bsp_excitation.c": ["gpio_config_pin(GPIOA, 8u", "TIM1->CCR1"],
        "bsp_timers.c": ["gpio_config_pin(GPIOB, 0u", "TIM3->CCR3", "BSP_GPIO_OUTPUT_BUZZER"],
    }
    for filename, needles in checks.items():
        text = (folder / filename).read_text()
        require(all(needle in text for needle in needles), f"peripheral contract drift: {filename}")
    return {"gpio_outputs": len(gpio_outputs(gpio)), "gpio_inputs": len(inputs), "peripheral_files": len(checks)}


def check_review(root, components, pins, bom):
    review = root / "docs/review/a05"
    with (review / "assembly-population.csv").open(encoding="utf-8-sig", newline="") as f:
        population = list(csv.DictReader(f))
    require(len(population) == len(components), "population row count mismatch")
    require({r["reference"] for r in population} == set(components), "population references mismatch")
    for row in population:
        ref = row["reference"]
        require(row["quantity"] == "1", f"population quantity {ref}")
        for field, bom_field in (("part_number", "Manufacturer Part"), ("footprint", "Footprint"), ("value", "Value")):
            require(row[field] == bom[ref][bom_field], f"population {field} drift {ref}")
        require(row["decision"] in {"POPULATE", "DNP", "HOLD FOR REVIEW"}, f"decision {ref}")
        require(row["reason"] and row["evidence"], f"missing population evidence {ref}")
    decisions = {r["reference"]: r["decision"] for r in population}
    require(not (decisions["K2"] == decisions["R0_BANK"] == "POPULATE"), "mutually exclusive K2 and hard link")
    require(decisions["D_TVS"] == decisions["R_TVS_LINK"], "incomplete optional TVS path")
    with (review / "pin-net-matrix.csv").open(encoding="utf-8-sig", newline="") as f:
        matrix = list(csv.DictReader(f))
    names = [row["mcu_pin"] for row in matrix]
    required = {f"P{port}{n}" for port in "AB" for n in range(16)} | {"PC13"}
    require(required <= set(names) and len(names) == len(set(names)), "missing/duplicate MCU matrix row")
    module_pins = components["USTM32"]["pin_names"]
    for row in matrix:
        numbers = row["module_pads"].split(";") if row["module_pads"] else []
        for number in numbers:
            require(number in module_pins and module_pins[number] == row["mcu_pin"], f"module pin identity {row['mcu_pin']}")
            require(pins[("USTM32", number)]["net"] == row["pcb_net"], f"matrix net drift {row['mcu_pin']}")
        if numbers or row["mcu_pin"].startswith("circuit:"):
            net = row["pcb_net"]
            expected = {f"{ref}.{pad}" for (ref, pad), pin in pins.items() if pin["net"] == net} if net else {f"USTM32.{n}" for n in numbers}
            require(set(row["connected_pads"].split(";")) == expected, f"matrix endpoint drift {row['mcu_pin']}")
        require(row["evidence"] and row["verification"], "missing matrix evidence")
    with (review / "findings.csv").open(encoding="utf-8-sig", newline="") as f:
        findings = list(csv.DictReader(f))
    require(len({r["id"] for r in findings}) == len(findings), "duplicate finding ID")
    for row in findings:
        require(row["severity"] in {"RED", "YELLOW", "GREEN", "UNKNOWN"}, "unknown severity")
        require(row["evidence"] and row["verification_method"], "finding lacks evidence/method")
        if row["severity"] == "RED":
            require(row["correction"] and row["change_type"], "RED lacks resolution")
    counts = {s: sum(r["severity"] == s for r in findings) for s in ("RED", "YELLOW", "GREEN", "UNKNOWN")}
    summary = " / ".join(f"{counts[s]} {s}" for s in counts)
    require(summary in (review / "README.md").read_text(encoding="utf-8"), "README finding counts drift")
    links = 0
    documents = sorted(review.glob("*.md")) + [root / name for name in (
        "PCB/README.md", "docs/05-Pinout-and-Interfaces.md", "docs/09-Rev1-Bringup.md",
        "docs/10-Consolidated-Design-Decisions.md", "docs/11-Rev1-BOM-and-Assembly.md",
        "plans/12-Hardware-Metrology-and-DC-Qualification.md", "plans/15-Execution-Workstreams-and-Release-Gates.md")]
    for path in documents:
        for target in re.findall(r"\]\(([^)]+)\)", path.read_text(encoding="utf-8")):
            if "://" in target or target.startswith("#"):
                continue
            resolved = path.parent / unquote(target.split("#", 1)[0])
            require(resolved.exists(), f"broken local link {path.name}: {target}")
            links += 1
    return {"matrix_rows": len(matrix), "carrier_mcu_pins_verified": len(required & set(module_pins.values())),
            "population": {d: sum(r["decision"] == d for r in population) for d in sorted(set(decisions.values()))},
            "findings": counts, "local_document_links_verified": links}


def audit(root=ROOT, check=False):
    with zipfile.ZipFile(root / SOURCE) as archive:
        members = [n for n in archive.namelist() if n.endswith(".epru")]
        require(len(members) == 1, "expected one epru project stream")
        documents = parse_epru(archive.read(members[0]).decode("utf-8-sig"))
    components, pins = extract_source(documents)
    rows, bom = read_bom((root / BOM).read_bytes())
    require(set(bom) == set(components), "BOM/source component set mismatch")
    for ref, component in components.items():
        require(component["part"] == bom[ref]["Manufacturer Part"], f"BOM/source part mismatch: {ref}")
        require(component["footprint"] == bom[ref]["Footprint"], f"BOM/source footprint mismatch: {ref}")
        require(component["value"] == bom[ref]["Value"], f"BOM/source value mismatch: {ref}")
    with zipfile.ZipFile(root / GERBER) as archive:
        flying = json.loads(archive.read("FlyingProbeTesting.json"))
        # Keep names in output; only their actual suffixes are format-stable.
        require(any(n.endswith(".GTL") for n in archive.namelist()) and
                any(n.endswith(".GBL") for n in archive.namelist()), "missing copper layer")
        fabrication = fabrication_pins(flying, components)
        copper_pads = check_top_copper(archive.read("Gerber_TopLayer.GTL").decode("utf-8-sig"), fabrication)
        drill_pads = check_drills(archive.read("Drill_PTH_Through.DRL").decode("utf-8-sig"), fabrication)
        members = archive.namelist()
    # Both RSCK pads moved +5 mil in the later fabrication export. Connectivity
    # is identical; explicitly record this revision delta, never hide it with
    # a widened tolerance for all 434 pads (finding A05-Y01).
    position_exceptions = {("RSCK", n): (0, 5, 0) for n in ("1", "2")}
    aliases = reconcile_pins(pins, fabrication, position_exceptions)
    report = {"source_documents": len(documents), "schematic_pages": sum(d["head"]["docType"] == "SCH_PAGE" for d in documents),
              "bom_groups": len(rows), "components": len(components), "pads": len(pins),
              "connected_source_nets": len({p["net"] for p in pins.values() if p["net"]}),
              "renamed_connected_nets": aliases, "unconnected_pads": sum(not p["net"] for p in pins.values()),
              "reviewed_position_deltas_mil": {f"{r}.{n}": delta for (r, n), delta in position_exceptions.items()},
              "top_copper_pad_flashes_verified": copper_pads,
              "component_drills_verified": drill_pads,
              "firmware": check_firmware(root, components, pins),
              "gerber_members": members,
              "sha256": {p.as_posix(): hashlib.sha256((root / p).read_bytes()).hexdigest() for p in
                         (SOURCE, BOM, GERBER, FAB / "SCH_WTK-RLC-Meter_2026-08-19.pdf", FAB / "PCB_RLC_2026-08-19.pdf")}}
    if check:
        report["review"] = check_review(root, components, pins, bom)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--check", action="store_true", help="also reconcile committed review CSVs")
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.root, args.check), indent=2, ensure_ascii=True))
    except (AuditError, KeyError, json.JSONDecodeError, zipfile.BadZipFile) as error:
        parser.exit(1, f"audit failed: {error}\n")


if __name__ == "__main__":
    main()
