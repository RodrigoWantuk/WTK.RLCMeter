import importlib.util
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


if __name__ == "__main__":
    unittest.main()
