import importlib.util
from pathlib import Path
import unittest


SPEC = importlib.util.spec_from_file_location(
    "check_profile_symbols", Path(__file__).resolve().parents[2] / "tools" / "check_profile_symbols.py")
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)


class CurveCompositionTest(unittest.TestCase):
    def test_off_rejects_compiled_sources_even_if_lto_hides_symbols(self):
        for source in tool.CURVE_SOURCES:
            self.assertTrue(tool.curve_composition_errors("", [{"file": "C:\\src\\" + source}], False))

    def test_off_rejects_surviving_curve_runtime(self):
        for name in ("measurement_cal_curve_apply", "g_curve_store", "product_load_curve_store"):
            self.assertTrue(tool.curve_composition_errors("08001234 t " + name, [], False))

    def test_off_accepts_osl_and_unsupported_entry_point(self):
        self.assertEqual(tool.curve_composition_errors(
            "08001234 t measurement_cal_apply_curve", [{"file": "src/measurement_calibration.c"}], False), [])

    def test_on_requires_sources_without_requiring_lto_symbol_names(self):
        commands = [{"file": "src/" + name} for name in tool.CURVE_SOURCES]
        self.assertEqual(tool.curve_composition_errors("", commands, True), [])
        self.assertTrue(tool.curve_composition_errors("", commands[:-1], True))


if __name__ == "__main__":
    unittest.main()
