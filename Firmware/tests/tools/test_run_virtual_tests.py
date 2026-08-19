import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


RUNNER_PATH = Path(__file__).resolve().parents[2] / "tools" / "run_virtual_tests.py"
SPEC = importlib.util.spec_from_file_location("run_virtual_tests", RUNNER_PATH)
runner = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(runner)


def write_vcd(text: str) -> Path:
    tmp = tempfile.NamedTemporaryFile("w", encoding="utf-8", suffix=".vcd", delete=False)
    with tmp:
        tmp.write(text)
    return Path(tmp.name)


def simple_vcd(definitions: str, changes: str) -> Path:
    return write_vcd(
        "$date\n"
        "test\n"
        "$end\n"
        "$version\n"
        "test\n"
        "$end\n"
        "$timescale 1 us $end\n"
        f"{definitions}"
        "$enddefinitions $end\n"
        f"{changes}"
    )


def quiet_mode_vcd(
    *,
    buzzer_during_quiet: bool = False,
    backlight_during_quiet: bool = True,
    flash_cs_during_quiet: bool = False,
    tft_cs_during_quiet: bool = False,
) -> Path:
    events: list[tuple[int, str]] = [
        (0, "0!"),
        (0, "0?"),
        (0, "1#"),
        (0, "1%"),
    ]

    for timestamp in range(90000, 870000, 1000):
        if not backlight_during_quiet and 260000 <= timestamp <= 690000:
            continue
        events.append((timestamp, "1?"))
        events.append((timestamp + 250, "0?"))

    for timestamp in range(100000, 250000, 500):
        events.append((timestamp, "1!" if ((timestamp // 500) % 2) == 0 else "0!"))

    if buzzer_during_quiet:
        for timestamp in range(350000, 450000, 500):
            events.append((timestamp, "1!" if ((timestamp // 500) % 2) == 0 else "0!"))

    for timestamp in range(700000, 850000, 500):
        events.append((timestamp, "1!" if ((timestamp // 500) % 2) == 0 else "0!"))

    if flash_cs_during_quiet:
        events.append((400000, "0#"))
        events.append((405000, "1#"))

    if tft_cs_during_quiet:
        events.append((420000, "0%"))
        events.append((425000, "1%"))

    changes = "".join(f"#{timestamp}\n{value}\n" for timestamp, value in sorted(events, key=lambda item: item[0]))
    return simple_vcd(
        "$var wire 1 ! PB1_IO_BUZZ $end\n"
        "$var wire 1 ? PB0_TFT_BL $end\n"
        "$var wire 1 # PA12_FLASH_CS $end\n"
        "$var wire 1 % PB12_TFT_CS $end\n",
        changes,
    )


class SerialMonitorWiringTest(unittest.TestCase):
    @staticmethod
    def _diagram(connections):
        return {"connections": connections}

    def test_correct_wiring_passes(self):
        diagram = self._diagram(
            [
                ["mcu:A9", "$serialMonitor:RX", "", []],
                ["$serialMonitor:TX", "mcu:A10", "", []],
            ]
        )
        self.assertEqual(runner.serial_monitor_wiring_errors(diagram), [])

    def test_missing_tx_connection_fails(self):
        diagram = self._diagram(
            [
                ["$serialMonitor:TX", "mcu:A10", "", []],
            ]
        )
        errors = runner.serial_monitor_wiring_errors(diagram)
        self.assertTrue(any("PA9 TX" in error for error in errors))
        self.assertFalse(any("PA10 RX" in error for error in errors))

    def test_missing_rx_connection_fails(self):
        diagram = self._diagram(
            [
                ["mcu:A9", "$serialMonitor:RX", "", []],
            ]
        )
        errors = runner.serial_monitor_wiring_errors(diagram)
        self.assertTrue(any("PA10 RX" in error for error in errors))
        self.assertFalse(any("PA9 TX" in error for error in errors))

    def test_swapped_monitor_direction_fails(self):
        diagram = self._diagram(
            [
                ["$serialMonitor:RX", "mcu:A9", "", []],
                ["mcu:A10", "$serialMonitor:TX", "", []],
            ]
        )
        errors = runner.serial_monitor_wiring_errors(diagram)
        self.assertEqual(len(errors), 2)
        self.assertTrue(any("PA9 TX" in error for error in errors))
        self.assertTrue(any("PA10 RX" in error for error in errors))

    def test_pa9_connected_to_monitor_tx_fails(self):
        diagram = self._diagram(
            [
                ["mcu:A9", "$serialMonitor:TX", "", []],
                ["$serialMonitor:TX", "mcu:A10", "", []],
            ]
        )
        errors = runner.serial_monitor_wiring_errors(diagram)
        self.assertTrue(any("PA9 TX" in error for error in errors))

    def test_pa10_connected_to_monitor_rx_fails(self):
        diagram = self._diagram(
            [
                ["mcu:A9", "$serialMonitor:RX", "", []],
                ["$serialMonitor:RX", "mcu:A10", "", []],
            ]
        )
        errors = runner.serial_monitor_wiring_errors(diagram)
        self.assertTrue(any("PA10 RX" in error for error in errors))

    def test_committed_diagram_has_correct_wiring(self):
        diagram_path = Path(__file__).resolve().parents[2] / "sim" / "wokwi" / "diagram.json"
        diagram = json.loads(diagram_path.read_text(encoding="utf-8"))
        self.assertEqual(runner.serial_monitor_wiring_errors(diagram), [])
        self.assertTrue(runner.has_directed_connection(diagram, "mcu:A9", "logic-io:D0"))
        self.assertTrue(runner.has_directed_connection(diagram, "mcu:B14", "flash:MISO"))
        self.assertFalse(runner.has_directed_connection(diagram, "mcu:B14", "tft:MISO"))


class UartTxActivityTest(unittest.TestCase):
    def test_pa9_edges_are_detected(self):
        changes = ["#0\n1!\n"]
        for index in range(1, 20):
            changes.append(f"#{index}\n{'0' if index % 2 else '1'}!\n")
        path = simple_vcd(
            "$scope module logic $end\n$var wire 1 ! D0 $end\n$upscope $end\n",
            "".join(changes),
        )
        self.assertTrue(runner.uart_tx_activity_present(path))

    def test_idle_pa9_is_not_activity(self):
        path = simple_vcd(
            "$scope module logic $end\n$var wire 1 ! D0 $end\n$upscope $end\n",
            "#0\n1!\n",
        )
        self.assertFalse(runner.uart_tx_activity_present(path))


class VcdSignalResolverTest(unittest.TestCase):
    def test_exact_canonical_match(self):
        path = simple_vcd("$var wire 1 ! PA12_FLASH_CS $end\n", "#0\n1!\n")
        signals, _ = runner.parse_vcd(path)
        self.assertEqual(runner.unique_signal_code(signals, "PA12_FLASH_CS"), "!")

    def test_analyzer_qualified_match(self):
        path = simple_vcd(
            "$scope module logic-safe $end\n"
            "$var wire 1 ? D2 $end\n"
            "$upscope $end\n",
            "#0\n1?\n",
        )
        signals, _ = runner.parse_vcd(path)
        self.assertEqual(runner.unique_signal_code(signals, "PA12_FLASH_CS"), "?")

    def test_duplicate_generic_d2_is_an_error(self):
        path = simple_vcd(
            "$scope module analyzer-a $end\n"
            "$var wire 1 ! D2 $end\n"
            "$upscope $end\n"
            "$scope module analyzer-b $end\n"
            "$var wire 1 ? D2 $end\n"
            "$upscope $end\n",
            "#0\n1!\n1?\n",
        )
        signals, _ = runner.parse_vcd(path)
        with self.assertRaisesRegex(RuntimeError, "ambiguous"):
            runner.unique_signal_code(signals, "PA12_FLASH_CS")

    def test_missing_signal_is_an_error(self):
        path = simple_vcd("$var wire 1 ! PB0_TFT_BL $end\n", "#0\n1!\n")
        signals, _ = runner.parse_vcd(path)
        with self.assertRaisesRegex(RuntimeError, "not found"):
            runner.unique_signal_code(signals, "PA12_FLASH_CS")


class VcdPostChecksTest(unittest.TestCase):
    def test_flash_tft_cs_non_overlap_passes(self):
        path = simple_vcd(
            "$var wire 1 ! PA12_FLASH_CS $end\n"
            "$var wire 1 ? PB12_TFT_CS $end\n",
            "#0\n1!\n1?\n"
            "#10\n0!\n"
            "#20\n1!\n"
            "#30\n0?\n"
            "#40\n1?\n",
        )
        runner.check_spi_cs(path)

    def test_flash_tft_cs_overlap_fails(self):
        path = simple_vcd(
            "$var wire 1 ! PA12_FLASH_CS $end\n"
            "$var wire 1 ? PB12_TFT_CS $end\n",
            "#0\n1!\n1?\n"
            "#10\n0!\n"
            "#20\n0?\n"
            "#30\n1!\n"
            "#40\n1?\n",
        )
        with self.assertRaisesRegex(RuntimeError, "same time"):
            runner.check_spi_cs(path)

    def test_pwm_1khz_passes(self):
        changes = ["#0\n0!\n"]
        for cycle in range(1, 7):
            start = cycle * 1000
            changes.append(f"#{start}\n1!\n")
            changes.append(f"#{start + 250}\n0!\n")
        path = simple_vcd("$var wire 1 ! PB0_TFT_BL $end\n", "".join(changes))
        runner.check_backlight_pwm(path)

    def test_pwm_outside_tolerance_fails(self):
        changes = ["#0\n0!\n"]
        for cycle in range(1, 7):
            start = cycle * 2000
            changes.append(f"#{start}\n1!\n")
            changes.append(f"#{start + 500}\n0!\n")
        path = simple_vcd("$var wire 1 ! PB0_TFT_BL $end\n", "".join(changes))
        with self.assertRaisesRegex(RuntimeError, "frequency out of range"):
            runner.check_backlight_pwm(path)

    def test_quiet_mode_temporal_and_spi_checks_pass(self):
        path = quiet_mode_vcd()
        runner.check_quiet_mode(path)

    def test_quiet_mode_buzzer_activity_during_quiet_fails(self):
        path = quiet_mode_vcd(buzzer_during_quiet=True)
        with self.assertRaisesRegex(RuntimeError, "buzzer toggled during quiet"):
            runner.check_quiet_mode(path)

    def test_quiet_mode_backlight_disappears_during_quiet_fails(self):
        path = quiet_mode_vcd(backlight_during_quiet=False)
        with self.assertRaisesRegex(RuntimeError, "during quiet"):
            runner.check_quiet_mode(path)

    def test_quiet_mode_flash_cs_assertion_during_quiet_fails(self):
        path = quiet_mode_vcd(flash_cs_during_quiet=True)
        with self.assertRaisesRegex(RuntimeError, "FLASH_CS asserted"):
            runner.check_quiet_mode(path)

    def test_quiet_mode_tft_cs_assertion_during_quiet_fails(self):
        path = quiet_mode_vcd(tft_cs_during_quiet=True)
        with self.assertRaisesRegex(RuntimeError, "TFT_CS asserted"):
            runner.check_quiet_mode(path)


    def test_display_activity_accepts_tft_cs_on_first_analyzer(self):
        path = simple_vcd(
            "$var wire 1 ! PB12_TFT_CS $end\n",
            "#0\n1!\n#10\n0!\n#20\n1!\n",
        )
        runner.check_display_activity(path)

    def test_display_activity_without_tft_cs_toggle_fails(self):
        path = simple_vcd("$var wire 1 ! PB12_TFT_CS $end\n", "#0\n1!\n")
        with self.assertRaisesRegex(RuntimeError, "PB12_TFT_CS"):
            runner.check_display_activity(path)


if __name__ == "__main__":
    unittest.main()
