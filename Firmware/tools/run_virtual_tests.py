#!/usr/bin/env python3
"""Run WTK.RLCMeter Wokwi virtual-hardware scenarios."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time


SCENARIOS = {
    "boot-safe": {
        "file": "boot-safe.yaml",
        "timeout_ms": 8000,
        "vcd": False,
        "checks": (),
    },
    "uart-boot": {
        "file": "uart-boot.yaml",
        "timeout_ms": 6000,
        "vcd": False,
        "checks": (),
    },
    "buttons": {
        "file": "buttons.yaml",
        "timeout_ms": 12000,
        "vcd": False,
        "checks": (),
    },
    "pwm-backlight": {
        "file": "pwm-backlight.yaml",
        "timeout_ms": 8000,
        "vcd": True,
        "checks": ("backlight_pwm",),
    },
    "spi-display": {
        "file": "spi-display.yaml",
        "timeout_ms": 8000,
        "vcd": True,
        "checks": ("display_activity",),
    },
    "spi-cs": {
        "file": "spi-cs.yaml",
        "timeout_ms": 7000,
        "vcd": True,
        "checks": ("spi_cs",),
    },
    "w25q-detect": {
        "file": "w25q-detect.yaml",
        "timeout_ms": 7000,
        "vcd": True,
        "checks": ("spi_cs", "display_activity"),
    },
    "w25q-selftest": {
        "file": "w25q-selftest.yaml",
        "timeout_ms": 12000,
        "vcd": True,
        "checks": ("spi_cs",),
    },
    "w25q-bad-jedec": {
        "file": "w25q-bad-jedec.yaml",
        "timeout_ms": 10000,
        "vcd": True,
        "checks": ("spi_cs",),
        "diagram_mode": "bad-jedec",
    },
    "w25q-absent": {
        "file": "w25q-absent.yaml",
        "timeout_ms": 10000,
        "vcd": True,
        "checks": ("spi_cs",),
        "diagram_mode": "absent",
    },
    "quiet-mode": {
        "file": "quiet-mode.yaml",
        "timeout_ms": 10000,
        "vcd": True,
        "checks": ("quiet_mode",),
    },
}

SMOKE_SCENARIOS = ("boot-safe", "uart-boot", "buttons", "spi-cs")

USART1_TX_PIN = "mcu:A9"
USART1_RX_PIN = "mcu:A10"
SERIAL_MONITOR_RX = "$serialMonitor:RX"
SERIAL_MONITOR_TX = "$serialMonitor:TX"

UART_PROBE = {
    "file": "uart-tx-probe.yaml",
    "timeout_ms": 8000,
    "vcd": True,
    "checks": (),
    "diagram_mode": "uart-probe",
}

VCD_SIGNAL_SPECS = {
    "PA12_FLASH_CS": {
        "aliases": ("PA12_FLASH_CS", "FLASH_CS", "logic-safe.PA12_FLASH_CS"),
        "qualified": ("logic-safe.D2",),
        "generic": ("D2",),
    },
    "PB12_TFT_CS": {
        "aliases": ("PB12_TFT_CS", "TFT_CS", "logic-safe.PB12_TFT_CS"),
        "qualified": ("logic-safe.D6",),
        "generic": ("D6",),
    },
    "PB0_TFT_BL": {
        "aliases": ("PB0_TFT_BL", "TFT_BL", "logic-safe.PB0_TFT_BL"),
        "qualified": ("logic-safe.D7",),
        "generic": ("D7",),
    },
    "PB1_IO_BUZZ": {
        "aliases": ("PB1_IO_BUZZ", "IO_BUZZ", "logic-safe.PB1_IO_BUZZ"),
        "qualified": ("logic-safe.D3",),
        "generic": ("D3",),
    },
    "PB11_TFT_DC": {
        "aliases": ("PB11_TFT_DC", "TFT_DC", "logic-spi.PB11_TFT_DC"),
        "qualified": ("logic-spi.D1",),
        "generic": (),
    },
    "PB13_TFT_SCK": {
        "aliases": ("PB13_TFT_SCK", "TFT_SCK", "logic-spi.PB13_TFT_SCK"),
        "qualified": ("logic-spi.D2",),
        "generic": (),
    },
    "PA9_USART1_TX": {
        "aliases": ("PA9_USART1_TX", "USART1_TX", "logic-io.PA9_USART1_TX"),
        "qualified": ("logic-io.D0", "logic.D0"),
        "generic": (),
    },
}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def firmware_root() -> Path:
    return Path(__file__).resolve().parents[1]


def run_command(command: list[str], cwd: Path, timeout_s: int | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=str(cwd),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout_s,
        check=False,
    )


def build_lab_firmware(fw_root: Path) -> int:
    for command in (
        ["cmake", "--preset", "stm32-lab"],
        ["cmake", "--build", "--preset", "stm32-lab"],
    ):
        print(f"+ {' '.join(command)}")
        result = run_command(command, fw_root, timeout_s=180)
        if result.stdout:
            print(result.stdout, end="" if result.stdout.endswith("\n") else "\n")
        if result.returncode != 0:
            return result.returncode
    return 0


def connection_pair(connection: object) -> tuple[str, str] | None:
    if not isinstance(connection, list) or len(connection) < 2:
        return None
    source = connection[0]
    destination = connection[1]
    if not isinstance(source, str) or not isinstance(destination, str):
        return None
    return (source, destination)


def has_directed_connection(diagram: dict[str, object], source: str, destination: str) -> bool:
    connections = diagram.get("connections", [])
    if not isinstance(connections, list):
        return False
    for connection in connections:
        pair = connection_pair(connection)
        if pair == (source, destination):
            return True
    return False


def serial_monitor_wiring_errors(diagram: dict[str, object]) -> list[str]:
    errors: list[str] = []
    if not has_directed_connection(diagram, USART1_TX_PIN, SERIAL_MONITOR_RX):
        errors.append(
            "Wokwi USART1 serial monitor wiring missing: expected PA9 TX -> $serialMonitor:RX"
        )
    if not has_directed_connection(diagram, SERIAL_MONITOR_TX, USART1_RX_PIN):
        errors.append(
            "Wokwi USART1 serial monitor wiring missing: expected $serialMonitor:TX -> PA10 RX"
        )
    return errors


def verify_serial_monitor_wiring(diagram: dict[str, object]) -> bool:
    errors = serial_monitor_wiring_errors(diagram)
    for error in errors:
        print(error)
    return not errors


def verify_static_files(fw_root: Path, project_dir: Path) -> bool:
    ok = True
    required = [
        project_dir / "wokwi.toml",
        project_dir / "diagram.json",
        project_dir / "README.md",
        project_dir / "chips" / "w25q64" / "w25q64.chip.c",
        project_dir / "chips" / "w25q64" / "w25q64.chip.json",
    ]
    required.extend(project_dir / "scenarios" / data["file"] for data in SCENARIOS.values())

    for path in required:
        if not path.exists():
            print(f"missing: {path}")
            ok = False

    try:
        with (project_dir / "diagram.json").open("r", encoding="utf-8") as handle:
            diagram = json.load(handle)
        part_ids = {part.get("id") for part in diagram.get("parts", [])}
        for part_id in ("mcu", "tft", "flash", "btn-up", "btn-down", "btn-ok", "logic-safe", "logic-spi", "logic-io"):
            if part_id not in part_ids:
                print(f"diagram is missing part id: {part_id}")
                ok = False
        if not verify_serial_monitor_wiring(diagram):
            ok = False
        if not has_directed_connection(diagram, USART1_TX_PIN, "logic-io:D0"):
            print("Wokwi USART1 TX logic-analyzer observability missing: expected PA9 -> logic-io:D0")
            ok = False
        if not has_directed_connection(diagram, "mcu:B14", "flash:MISO"):
            print("Wokwi W25Q MISO wiring missing: expected PB14 -> flash:MISO")
            ok = False
        if has_directed_connection(diagram, "mcu:B14", "tft:MISO"):
            print("Wokwi ILI9341 MISO must stay disconnected; firmware never reads the TFT and the simulator model can hold MISO")
            ok = False
    except (OSError, json.JSONDecodeError) as exc:
        print(f"diagram.json validation failed: {exc}")
        ok = False

    probe_scenario = project_dir / "scenarios" / str(UART_PROBE["file"])
    if not probe_scenario.exists():
        print(f"missing: {probe_scenario}")
        ok = False

    elf = fw_root / "build" / "stm32-lab" / "WTK.RLCMeter.elf"
    if not elf.exists():
        print(f"ELF not found: {elf}")
        ok = False

    return ok


def normalized_name(name: str) -> str:
    return re.sub(r"[^a-z0-9]", "", name.lower())


def parse_timescale(line: str) -> float:
    parts = line.strip().split()
    if len(parts) < 3:
        return 1e-9
    try:
        quantity = float(parts[1])
    except ValueError:
        quantity = 1.0
    unit = parts[2]
    multipliers = {
        "fs": 1e-15,
        "ps": 1e-12,
        "ns": 1e-9,
        "us": 1e-6,
        "ms": 1e-3,
        "s": 1.0,
    }
    return quantity * multipliers.get(unit, 1e-9)


def parse_vcd(path: Path) -> tuple[dict[str, str], dict[str, list[tuple[float, str]]]]:
    signals: dict[str, str] = {}
    events: dict[str, list[tuple[float, str]]] = {}
    timescale_s = 1e-9
    current_time_s = 0.0
    in_definitions = True
    scopes: list[str] = []

    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line:
                continue
            if line.startswith("$timescale"):
                timescale_s = parse_timescale(line)
                continue
            if line.startswith("$scope"):
                parts = line.split()
                if len(parts) >= 3:
                    scopes.append(parts[2])
                continue
            if line.startswith("$upscope"):
                if scopes:
                    scopes.pop()
                continue
            if line.startswith("$var"):
                parts = line.split()
                if len(parts) >= 5:
                    code = parts[3]
                    reference = " ".join(parts[4:-1])
                    full_reference = ".".join(scopes + [reference]) if scopes else reference
                    signals[full_reference] = code
                    events.setdefault(code, [])
                continue
            if line.startswith("$enddefinitions"):
                in_definitions = False
                continue
            if in_definitions:
                continue
            if line.startswith("#"):
                try:
                    current_time_s = float(int(line[1:])) * timescale_s
                except ValueError:
                    current_time_s = current_time_s
                continue
            if len(line) >= 2 and line[0] in "01xXzZ":
                value = line[0].lower()
                code = line[1:]
                if code in events:
                    events[code].append((current_time_s, value))

    return signals, events


def unique_signal_code(signals: dict[str, str], canonical: str) -> str:
    spec = VCD_SIGNAL_SPECS.get(canonical, {"aliases": (canonical,), "qualified": (), "generic": ()})
    alias_norms = {normalized_name(alias) for alias in spec["aliases"]}
    qualified_norms = {normalized_name(alias) for alias in spec["qualified"]}
    generic_norms = {normalized_name(alias) for alias in spec["generic"]}

    exact: list[tuple[str, str]] = []
    qualified: list[tuple[str, str]] = []
    generic: list[tuple[str, str]] = []

    for reference, code in signals.items():
        norm = normalized_name(reference)
        components = {normalized_name(component) for component in re.split(r"[.\s/]+", reference)}
        if norm in alias_norms:
            exact.append((reference, code))
        elif any(norm.endswith(alias) for alias in alias_norms):
            qualified.append((reference, code))
        elif norm in qualified_norms or any(norm.endswith(alias) for alias in qualified_norms):
            qualified.append((reference, code))
        elif norm in generic_norms or (components & generic_norms):
            generic.append((reference, code))

    for label, matches in (("exact", exact), ("qualified", qualified), ("generic", generic)):
        unique_codes = {code for _, code in matches}
        if len(unique_codes) == 1:
            return next(iter(unique_codes))
        if len(unique_codes) > 1:
            names = ", ".join(reference for reference, _ in matches)
            raise RuntimeError(f"ambiguous {label} VCD signal for {canonical}: {names}")

    raise RuntimeError(f"VCD signal not found: {canonical}")


def value_events(signals: dict[str, str], events: dict[str, list[tuple[float, str]]], name: str) -> list[tuple[float, str]]:
    code = unique_signal_code(signals, name)
    return events.get(code, [])


def binary_events(signals: dict[str, str], events: dict[str, list[tuple[float, str]]], name: str) -> list[tuple[float, str]]:
    return [(timestamp, value) for timestamp, value in value_events(signals, events, name) if value in ("0", "1")]


def value_at(events: list[tuple[float, str]], timestamp_s: float, default: str = "1") -> str:
    value = default
    for event_time_s, event_value in events:
        if event_time_s > timestamp_s:
            break
        value = event_value
    return value


def windowed_events(events: list[tuple[float, str]], start_s: float, end_s: float, default: str = "0") -> list[tuple[float, str]]:
    if end_s <= start_s:
        return []

    result = [(start_s, value_at(events, start_s, default))]
    result.extend((timestamp, value) for timestamp, value in events if start_s < timestamp < end_s)
    result.append((end_s, value_at(events, end_s, default)))
    return result


def signal_has_low_assertion(events: list[tuple[float, str]], start_s: float, end_s: float) -> bool:
    if value_at(events, start_s, "1") == "0":
        return True
    return any((start_s < timestamp < end_s) and (value == "0") for timestamp, value in events)


def signal_bursts(events: list[tuple[float, str]], gap_s: float) -> list[tuple[float, float, int]]:
    if not events:
        return []

    bursts: list[tuple[float, float, int]] = []
    burst_start = events[0][0]
    burst_end = events[0][0]
    edge_count = 1
    for timestamp, _ in events[1:]:
        if timestamp - burst_end > gap_s:
            if edge_count >= 2:
                bursts.append((burst_start, burst_end, edge_count))
            burst_start = timestamp
            edge_count = 1
        else:
            edge_count += 1
        burst_end = timestamp

    if edge_count >= 2:
        bursts.append((burst_start, burst_end, edge_count))
    return bursts


def backlight_pwm_metrics(events: list[tuple[float, str]], start_s: float | None = None, end_s: float | None = None) -> tuple[float, float]:
    pwm_events = events
    if start_s is not None and end_s is not None:
        pwm_events = windowed_events(events, start_s, end_s, "0")
    if len(pwm_events) < 8:
        raise RuntimeError("not enough backlight PWM edges captured")

    rising_edges: list[float] = []
    high_time = 0.0
    total_time = 0.0
    last_time = pwm_events[0][0]
    last_value = pwm_events[0][1]
    for timestamp, value in pwm_events[1:]:
        duration = timestamp - last_time
        if duration > 0.0:
            total_time += duration
            if last_value == "1":
                high_time += duration
        if last_value == "0" and value == "1":
            rising_edges.append(timestamp)
        last_time = timestamp
        last_value = value

    if len(rising_edges) < 3 or total_time <= 0.0:
        raise RuntimeError("not enough backlight PWM cycles captured")

    periods = [rising_edges[i + 1] - rising_edges[i] for i in range(len(rising_edges) - 1)]
    average_period_s = sum(periods) / len(periods)
    frequency_hz = 1.0 / average_period_s if average_period_s > 0.0 else 0.0
    duty_percent = (high_time / total_time) * 100.0
    return frequency_hz, duty_percent


def require_backlight_pwm(events: list[tuple[float, str]], label: str, start_s: float | None = None, end_s: float | None = None) -> tuple[float, float]:
    try:
        frequency_hz, duty_percent = backlight_pwm_metrics(events, start_s, end_s)
    except RuntimeError as exc:
        raise RuntimeError(f"{label}: {exc}") from exc

    if not (900.0 <= frequency_hz <= 1100.0):
        raise RuntimeError(f"{label}: backlight PWM frequency out of range: {frequency_hz:.1f} Hz")
    if not (15.0 <= duty_percent <= 35.0):
        raise RuntimeError(f"{label}: backlight PWM duty out of range: {duty_percent:.1f}%")
    return frequency_hz, duty_percent


def check_spi_cs(path: Path) -> None:
    signals, events = parse_vcd(path)
    flash_events = binary_events(signals, events, "PA12_FLASH_CS")
    tft_events = binary_events(signals, events, "PB12_TFT_CS")
    combined = [(t, "flash", v) for t, v in flash_events] + [(t, "tft", v) for t, v in tft_events]
    combined.sort(key=lambda item: item[0])

    state = {"flash": "1", "tft": "1"}
    saw_flash = False
    saw_tft = False
    saw_tft_asserted = False
    for _, channel, value in combined:
        state[channel] = value
        if channel == "flash":
            saw_flash = True
        if channel == "tft":
            saw_tft = True
            saw_tft_asserted = saw_tft_asserted or (value == "0")
        if state["flash"] == "0" and state["tft"] == "0":
            raise RuntimeError("FLASH_CS and TFT_CS were low at the same time")

    if not (saw_flash and saw_tft):
        raise RuntimeError("chip-select signals were not captured")
    if not saw_tft_asserted:
        raise RuntimeError("TFT_CS activity was not captured")
    if state["flash"] != "1" or state["tft"] != "1":
        raise RuntimeError("chip-select signals did not finish idle-high")


def transition_count(events: list[tuple[float, str]]) -> int:
    if len(events) < 2:
        return 0
    count = 0
    previous = events[0][1]
    for _, value in events[1:]:
        if value != previous:
            count += 1
            previous = value
    return count


def uart_tx_activity_present(path: Path, min_edges: int = 16) -> bool:
    signals, events = parse_vcd(path)
    tx_events = binary_events(signals, events, "PA9_USART1_TX")
    return transition_count(tx_events) >= min_edges


def serial_capture_contains(path: Path, token: str) -> bool:
    if not path.exists():
        return False
    return token in path.read_text(encoding="utf-8", errors="replace")


def check_display_activity(path: Path) -> None:
    signals, events = parse_vcd(path)
    cs_events = binary_events(signals, events, "PB12_TFT_CS")
    cs_values = {value for _, value in cs_events}
    if not ({"0", "1"} <= cs_values):
        raise RuntimeError("display signal did not toggle: PB12_TFT_CS")

    # wokwi-cli --vcd-file records only the first logic analyzer as scope "logic".
    # TFT_DC/SCK live on logic-spi, so they are optional when that analyzer is not dumped.
    for name in ("PB11_TFT_DC", "PB13_TFT_SCK"):
        try:
            captured = binary_events(signals, events, name)
        except RuntimeError:
            continue
        values = {value for _, value in captured}
        if captured and not ({"0", "1"} <= values):
            raise RuntimeError(f"display signal did not toggle: {name}")


def first_rising_edge_s(events: list[tuple[float, str]]) -> float | None:
    previous: str | None = None
    for timestamp, value in events:
        if previous == "0" and value == "1":
            return timestamp
        previous = value
    return None


def check_backlight_pwm(path: Path) -> None:
    signals, events = parse_vcd(path)
    pwm_events = binary_events(signals, events, "PB0_TFT_BL")
    start_s = first_rising_edge_s(pwm_events)
    if start_s is None:
        raise RuntimeError("backlight PWM never started")
    end_s = pwm_events[-1][0]
    frequency_hz, duty_percent = require_backlight_pwm(pwm_events, "overall", start_s, end_s)
    print(f"  VCD: backlight PWM {frequency_hz:.1f} Hz, duty {duty_percent:.1f}%")


def check_quiet_mode(path: Path) -> None:
    signals, events = parse_vcd(path)
    buzzer_events = binary_events(signals, events, "PB1_IO_BUZZ")
    backlight_events = binary_events(signals, events, "PB0_TFT_BL")
    flash_cs_events = binary_events(signals, events, "PA12_FLASH_CS")
    tft_cs_events = binary_events(signals, events, "PB12_TFT_CS")
    if len(buzzer_events) < 8:
        raise RuntimeError("buzzer did not toggle before quiet-mode request")
    if len(backlight_events) < 8:
        raise RuntimeError("backlight PWM was not captured during quiet-mode scenario")

    buzzer_bursts = signal_bursts(buzzer_events, 0.050)
    if len(buzzer_bursts) < 2:
        raise RuntimeError("quiet-mode did not create separated buzzer bursts")

    pre_quiet_burst = buzzer_bursts[0]
    post_quiet_burst = buzzer_bursts[-1]
    pre_duration_s = pre_quiet_burst[1] - pre_quiet_burst[0]
    quiet_start_s = pre_quiet_burst[1]
    quiet_end_s = post_quiet_burst[0]
    quiet_duration_s = quiet_end_s - quiet_start_s

    if pre_duration_s >= 1.5:
        raise RuntimeError("pre-quiet buzzer tone was not stopped early")
    if quiet_duration_s < 0.2:
        raise RuntimeError("quiet interval was too short to prove inactivity")
    if any(quiet_start_s < timestamp < quiet_end_s for timestamp, _ in buzzer_events):
        raise RuntimeError("buzzer toggled during quiet interval")
    if signal_has_low_assertion(flash_cs_events, quiet_start_s, quiet_end_s):
        raise RuntimeError("FLASH_CS asserted during quiet interval")
    if signal_has_low_assertion(tft_cs_events, quiet_start_s, quiet_end_s):
        raise RuntimeError("TFT_CS asserted during quiet interval")

    before_frequency_hz, before_duty_percent = require_backlight_pwm(
        backlight_events, "before quiet", pre_quiet_burst[0], pre_quiet_burst[1]
    )
    during_frequency_hz, during_duty_percent = require_backlight_pwm(
        backlight_events, "during quiet", quiet_start_s, quiet_end_s
    )
    after_frequency_hz, after_duty_percent = require_backlight_pwm(
        backlight_events, "after quiet", post_quiet_burst[0], post_quiet_burst[1]
    )

    print(
        "  VCD: quiet-mode buzzer stopped, SPI idle; "
        f"backlight {before_frequency_hz:.1f}/{during_frequency_hz:.1f}/{after_frequency_hz:.1f} Hz, "
        f"duty {before_duty_percent:.1f}/{during_duty_percent:.1f}/{after_duty_percent:.1f}%"
    )


def run_post_checks(checks: tuple[str, ...], vcd_file: Path) -> None:
    for check in checks:
        if check == "spi_cs":
            check_spi_cs(vcd_file)
            print("  VCD: SPI chip-select invariant OK")
        elif check == "display_activity":
            check_display_activity(vcd_file)
            print("  VCD: display CS/DC/SCK activity OK")
        elif check == "backlight_pwm":
            check_backlight_pwm(vcd_file)
        elif check == "quiet_mode":
            check_quiet_mode(vcd_file)
        else:
            raise RuntimeError(f"unknown post-check: {check}")


def copy_chip_json(project_dir: Path, chip_out_dir: Path) -> None:
    source = project_dir / "chips" / "w25q64" / "w25q64.chip.json"
    destination = chip_out_dir / "w25q64.chip.json"
    chip_out_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)


def install_chip_wasm(wasm: Path, project_dir: Path) -> None:
    destination = project_dir / "chips" / "w25q64" / "w25q64.chip.wasm"
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(wasm, destination)


def build_custom_chips(wokwi_cli: str, project_dir: Path, artifact_dir: Path) -> int:
    source = project_dir / "chips" / "w25q64" / "w25q64.chip.c"
    chip_out_dir = artifact_dir / "chips" / "w25q64"
    wasm = chip_out_dir / "w25q64.chip.wasm"
    json_out = chip_out_dir / "w25q64.chip.json"
    copy_chip_json(project_dir, chip_out_dir)

    if wasm.exists() and wasm.stat().st_mtime >= source.stat().st_mtime and json_out.stat().st_mtime >= source.stat().st_mtime:
        print(f"custom chip current: {wasm}")
        install_chip_wasm(wasm, project_dir)
        return 0

    command = [
        wokwi_cli,
        "chip",
        "compile",
        "chips/w25q64/w25q64.chip.c",
        "-o",
        str(wasm),
    ]
    print(f"+ {' '.join(command)}")
    result = run_command(command, project_dir, timeout_s=180)
    if result.stdout:
        print(result.stdout, end="" if result.stdout.endswith("\n") else "\n")
    if result.returncode != 0:
        return result.returncode
    install_chip_wasm(wasm, project_dir)
    return 0


def run_wokwi_lint(wokwi_cli: str, project_dir: Path) -> int:
    command = [wokwi_cli, "lint"]
    print(f"+ {' '.join(command)}")
    result = run_command(command, project_dir, timeout_s=60)
    if result.stdout:
        print(result.stdout, end="" if result.stdout.endswith("\n") else "\n")
    if result.returncode != 0:
        print("wokwi-cli lint failed; virtual scenarios were not executed")
    return result.returncode


def diagram_for_mode(project_dir: Path, artifact_dir: Path, mode: str | None) -> Path:
    if mode is None:
        return project_dir / "diagram.json"

    with (project_dir / "diagram.json").open("r", encoding="utf-8") as handle:
        diagram = json.load(handle)

    if mode in ("bad-jedec", "absent"):
        for part in diagram.get("parts", []):
            if part.get("id") == "flash":
                attrs = part.setdefault("attrs", {})
                if mode == "bad-jedec":
                    attrs["jedecMode"] = "1"
                    attrs["noResponse"] = "0"
                else:
                    attrs["jedecMode"] = "0"
                    attrs["noResponse"] = "1"
    elif mode == "uart-probe":
        # wokwi-cli --vcd-file records the first logic analyzer as scope "logic".
        # Keep the committed diagram order for acceptance scenarios; only the
        # diagnostic probe promotes logic-io so D0 is PA9 / USART1_TX.
        parts = list(diagram.get("parts", []))
        diagram["parts"] = [part for part in parts if part.get("id") == "logic-io"] + [
            part for part in parts if part.get("id") != "logic-io"
        ]
    else:
        raise RuntimeError(f"unknown diagram mode: {mode}")

    out_dir = artifact_dir / "diagrams"
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / f"diagram-{mode}.json"
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(diagram, handle, indent=2)
        handle.write("\n")
    return path


def project_relative(path: Path, project_dir: Path) -> str:
    return os.path.relpath(path, project_dir).replace("\\", "/")


def run_scenario(
    name: str,
    data: dict[str, object],
    project_dir: Path,
    artifact_dir: Path,
    elf: Path,
    wokwi_cli: str,
) -> bool:
    serial_log = artifact_dir / f"{name}.serial.log"
    vcd_file = artifact_dir / f"{name}.vcd"
    scenario_file = Path("scenarios") / str(data["file"])
    diagram_file = diagram_for_mode(project_dir, artifact_dir, data.get("diagram_mode"))
    timeout_ms = int(data["timeout_ms"])
    command = [
        wokwi_cli,
        ".",
        "--scenario",
        str(scenario_file).replace("\\", "/"),
        "--diagram-file",
        project_relative(diagram_file, project_dir),
        "--elf",
        str(elf),
        "--serial-log-file",
        str(serial_log),
        "--timeout",
        str(timeout_ms),
        "--timeout-exit-code",
        "124",
    ]
    if data["vcd"]:
        command.extend(["--vcd-file", str(vcd_file)])

    print(f"[RUN] {name}")
    start = time.monotonic()
    try:
        result = run_command(command, project_dir, timeout_s=max(20, (timeout_ms // 1000) + 30))
    except subprocess.TimeoutExpired:
        print(f"[FAIL] {name}: host timeout")
        return False

    elapsed = time.monotonic() - start
    if result.stdout:
        print(result.stdout, end="" if result.stdout.endswith("\n") else "\n")
    if result.returncode != 0:
        print(f"[FAIL] {name}: wokwi-cli exited {result.returncode} after {elapsed:.1f}s")
        return False

    try:
        run_post_checks(tuple(data["checks"]), vcd_file)
    except RuntimeError as exc:
        print(f"[FAIL] {name}: {exc}")
        return False

    print(f"[PASS] {name}: simulated {timeout_ms} ms budget, wall {elapsed:.1f}s")
    return True


def selected_scenarios(args: argparse.Namespace) -> list[str]:
    if args.scenario:
        unknown = [name for name in args.scenario if name not in SCENARIOS]
        if unknown:
            raise SystemExit(f"unknown scenario(s): {', '.join(unknown)}")
        return list(args.scenario)
    if args.smoke:
        return list(SMOKE_SCENARIOS)
    return list(SCENARIOS.keys())


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", action="store_true", help="configure and build the stm32-lab preset before running")
    parser.add_argument("--smoke", action="store_true", help="run the short smoke suite")
    parser.add_argument("--scenario", action="append", help="run one named scenario; can be repeated")
    parser.add_argument("--keep-artifacts", action="store_true", help="keep previous logs/VCD files")
    parser.add_argument("--check-only", action="store_true", help="validate local files without invoking Wokwi")
    parser.add_argument("--lint-only", action="store_true", help="build custom chips and run wokwi-cli lint without simulation")
    parser.add_argument("--uart-probe", action="store_true", help="record PA9 VCD activity and Serial Monitor capture without changing the acceptance suite")
    args = parser.parse_args(argv)

    fw_root = firmware_root()
    project_dir = fw_root / "sim" / "wokwi"
    artifact_dir = fw_root / "build" / "virtual" / "wokwi"
    elf = fw_root / "build" / "stm32-lab" / "WTK.RLCMeter.elf"

    if args.build:
        build_status = build_lab_firmware(fw_root)
        if build_status != 0:
            return build_status

    if not args.keep_artifacts and artifact_dir.exists():
        shutil.rmtree(artifact_dir)
    artifact_dir.mkdir(parents=True, exist_ok=True)

    static_ok = verify_static_files(fw_root, project_dir)
    if args.check_only:
        print("available scenarios:")
        for name in SCENARIOS:
            print(f"  {name}")
        return 0 if static_ok else 1
    if not static_ok:
        return 1

    wokwi_cli = shutil.which("wokwi-cli")
    if wokwi_cli is None:
        print("wokwi-cli not found in PATH; lint/simulation were not executed")
        return 2

    version = run_command([wokwi_cli, "--version"], fw_root, timeout_s=10)
    if version.stdout:
        print(version.stdout.strip())

    chip_status = build_custom_chips(wokwi_cli, project_dir, artifact_dir)
    if chip_status != 0:
        return chip_status

    lint_status = run_wokwi_lint(wokwi_cli, project_dir)
    if lint_status != 0:
        return lint_status
    if args.lint_only:
        return 0

    token = os.environ.get("WOKWI_CLI_TOKEN")
    if not token:
        print("WOKWI_CLI_TOKEN is not set; virtual scenarios were not executed")
        return 2
    print("WOKWI_CLI_TOKEN: present")

    if args.uart_probe:
        scenario_ok = run_scenario("uart-tx-probe", UART_PROBE, project_dir, artifact_dir, elf, wokwi_cli)
        serial_log = artifact_dir / "uart-tx-probe.serial.log"
        vcd_file = artifact_dir / "uart-tx-probe.vcd"
        pa9_toggled = False
        if vcd_file.exists():
            try:
                pa9_toggled = uart_tx_activity_present(vcd_file)
            except RuntimeError as exc:
                print(f"PA9 VCD parse: {exc}")
        serial_captured = serial_capture_contains(serial_log, "WTK.RLCMeter")
        print(f"PA9 toggled: {'yes' if pa9_toggled else 'no'}")
        print(f"Serial Monitor captured text: {'yes' if serial_captured else 'no'}")
        if not scenario_ok and not serial_captured:
            return 1
        return 0 if serial_captured else 1

    scenario_names = selected_scenarios(args)
    failures = 0
    simulated_ms = 0
    for name in scenario_names:
        data = SCENARIOS[name]
        simulated_ms += int(data["timeout_ms"])
        if not run_scenario(name, data, project_dir, artifact_dir, elf, wokwi_cli):
            failures += 1

    print(f"simulated time budget: {simulated_ms / 1000.0:.1f}s across {len(scenario_names)} scenario(s)")
    if failures:
        print(f"virtual suite failed: {failures} scenario(s)")
        return 1

    print("virtual suite passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
