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
        "timeout_ms": 6000,
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
        "timeout_ms": 9000,
        "vcd": False,
        "checks": (),
    },
    "pwm-backlight": {
        "file": "pwm-backlight.yaml",
        "timeout_ms": 5000,
        "vcd": True,
        "checks": ("backlight_pwm",),
    },
    "spi-display": {
        "file": "spi-display.yaml",
        "timeout_ms": 7000,
        "vcd": True,
        "checks": ("display_activity",),
    },
    "spi-cs": {
        "file": "spi-cs.yaml",
        "timeout_ms": 7000,
        "vcd": True,
        "checks": ("spi_cs",),
    },
}

SMOKE_SCENARIOS = ("boot-safe", "uart-boot", "buttons", "spi-cs")

VCD_NAME_ALIASES = {
    "PA12_FLASH_CS": ("PA12_FLASH_CS", "FLASH_CS", "D2"),
    "PB12_TFT_CS": ("PB12_TFT_CS", "TFT_CS", "D6"),
    "PB0_TFT_BL": ("PB0_TFT_BL", "TFT_BL", "D7"),
    "PB11_TFT_DC": ("PB11_TFT_DC", "TFT_DC", "D1"),
    "PB13_TFT_SCK": ("PB13_TFT_SCK", "TFT_SCK", "D2"),
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


def verify_static_files(fw_root: Path, project_dir: Path) -> bool:
    ok = True
    required = [
        project_dir / "wokwi.toml",
        project_dir / "diagram.json",
        project_dir / "README.md",
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
        for part_id in ("mcu", "tft", "btn-up", "btn-down", "btn-ok", "logic-safe", "logic-spi", "logic-io"):
            if part_id not in part_ids:
                print(f"diagram is missing part id: {part_id}")
                ok = False
    except (OSError, json.JSONDecodeError) as exc:
        print(f"diagram.json validation failed: {exc}")
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

    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line:
                continue
            if line.startswith("$timescale"):
                timescale_s = parse_timescale(line)
                continue
            if line.startswith("$var"):
                parts = line.split()
                if len(parts) >= 5:
                    code = parts[3]
                    reference = " ".join(parts[4:-1])
                    signals[reference] = code
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


def signal_code(signals: dict[str, str], canonical: str) -> str | None:
    aliases = VCD_NAME_ALIASES.get(canonical, (canonical,))
    normalized_aliases = {normalized_name(alias) for alias in aliases}
    for reference, code in signals.items():
        norm = normalized_name(reference)
        if norm in normalized_aliases or any(alias in norm for alias in normalized_aliases):
            return code
    return None


def value_events(signals: dict[str, str], events: dict[str, list[tuple[float, str]]], name: str) -> list[tuple[float, str]]:
    code = signal_code(signals, name)
    if code is None:
        raise RuntimeError(f"VCD signal not found: {name}")
    return events.get(code, [])


def check_spi_cs(path: Path) -> None:
    signals, events = parse_vcd(path)
    flash_events = value_events(signals, events, "PA12_FLASH_CS")
    tft_events = value_events(signals, events, "PB12_TFT_CS")
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


def check_display_activity(path: Path) -> None:
    signals, events = parse_vcd(path)
    for name in ("PB12_TFT_CS", "PB11_TFT_DC", "PB13_TFT_SCK"):
        captured = value_events(signals, events, name)
        values = {value for _, value in captured}
        if not ({"0", "1"} <= values):
            raise RuntimeError(f"display signal did not toggle: {name}")


def check_backlight_pwm(path: Path) -> None:
    signals, events = parse_vcd(path)
    pwm_events = [(t, v) for t, v in value_events(signals, events, "PB0_TFT_BL") if v in ("0", "1")]
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

    if not (900.0 <= frequency_hz <= 1100.0):
        raise RuntimeError(f"backlight PWM frequency out of range: {frequency_hz:.1f} Hz")
    if not (15.0 <= duty_percent <= 35.0):
        raise RuntimeError(f"backlight PWM duty out of range: {duty_percent:.1f}%")

    print(f"  VCD: backlight PWM {frequency_hz:.1f} Hz, duty {duty_percent:.1f}%")


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
        else:
            raise RuntimeError(f"unknown post-check: {check}")


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
    timeout_ms = int(data["timeout_ms"])
    command = [
        wokwi_cli,
        ".",
        "--scenario",
        str(scenario_file).replace("\\", "/"),
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
        print("wokwi-cli not found in PATH; virtual scenarios were not executed")
        return 2

    token = os.environ.get("WOKWI_CLI_TOKEN")
    if not token:
        print("WOKWI_CLI_TOKEN is not set; virtual scenarios were not executed")
        return 2
    print("WOKWI_CLI_TOKEN: present")

    version = run_command([wokwi_cli, "--version"], fw_root, timeout_s=10)
    if version.stdout:
        print(version.stdout.strip())

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
