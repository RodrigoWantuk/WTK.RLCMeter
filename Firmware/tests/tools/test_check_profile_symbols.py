import importlib.util
from pathlib import Path
import unittest


SPEC = importlib.util.spec_from_file_location(
    "check_profile_symbols", Path(__file__).resolve().parents[2] / "tools" / "check_profile_symbols.py")
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)


class CurveCompositionTest(unittest.TestCase):
    def test_peripheral_diagnostic_rejects_active_operations(self):
        for name in ("app_bringup_console_step", "run_command.constprop.0", "hw_metrology_measure_start",
                     "hw_metrology_measure_step", "bsp_metrology_adc_start_capture",
                     "app_calibration_service_candidate_commit", "w25q_device_program", "w25q_device_erase"):
            self.assertTrue(tool.peripheral_composition_errors("08001234 T " + name, True))
        self.assertEqual(tool.peripheral_composition_errors("08001234 T ui_peripheral_test_step", True), [])

    def test_normal_profiles_exclude_peripheral_panel(self):
        for name in ("ui_peripheral_test_step", "g_peripheral_ui"):
            self.assertTrue(tool.peripheral_composition_errors("08001234 T " + name, False))
        self.assertEqual(tool.peripheral_composition_errors("08001234 T hw_safety_evaluate", False), [])

    def test_relay_bench_symbols_forbidden_outside_bringup(self):
        for profile in ("PRODUCT", "BRINGUP_CAL"):
            for name in ("hw_k1_bench_start", "hw_k1_bench_step", "lab_k1_test_input"):
                self.assertTrue(any(pattern.search(name) for pattern in tool.FORBIDDEN[profile]))
        self.assertFalse(any(pattern.search("hw_k1_bench_start") for pattern in tool.FORBIDDEN["BRINGUP"]))

    def test_factory_requires_runtime_safety_resources_and_excludes_fit(self):
        retained = [{"file": name} for name in ("app_product.c", "app_calibration_runtime.c",
                    "measurement_calibration.c", "measurement_calibration_store.c",
                    "app_resource_update.c", "hw_safety.c")]
        self.assertEqual(tool.factory_composition_errors("", retained, True), [])
        self.assertTrue(tool.factory_composition_errors("", retained[:-1], True))
        for source in ("app_calibration_wizard.c", "app_calibration_session.c", "measurement_calibration_solver.c"):
            self.assertTrue(tool.factory_composition_errors("", retained + [{"file": source}], True))
        for symbol in ("app_calibration_wizard_start", "measurement_cal_solver_solve",
                       "measurement_cal_store_step", "prepare_wizard_line", "cal_load_preset_token"):
            self.assertTrue(tool.factory_composition_errors(symbol, retained, True))
        self.assertEqual(tool.factory_composition_errors("measurement_cal_solver_solve", [], False), [])

    def test_capture_service_excluded_from_normal_profiles(self):
        for profile in ("PRODUCT","BRINGUP"):
            self.assertTrue(tool.capture_composition_errors([{"file":"app_cal_capture_service.c"}],profile))
            self.assertEqual(tool.capture_composition_errors([{"file":"app_shell.c"}],profile),[])

    def test_calibration_profile_requires_service_and_excludes_laboratory(self):
        sources=[{"file":name} for name in ("app_cal_capture_service.c","app_cal_capture_shell.c")]
        self.assertEqual(tool.capture_composition_errors(sources,"BRINGUP_CAL"),[])
        self.assertEqual(tool.capture_composition_errors(sources + [{"file":name} for name in
            ("ui_boot_splash.c", "ui_image.c", "ili9341.c", "resource_store.c")],"BRINGUP_CAL"),[])
        self.assertTrue(tool.capture_composition_errors(sources+[{"file":"app_bringup_console.c"}],"BRINGUP_CAL"))
        self.assertTrue(tool.capture_composition_errors([],"BRINGUP_CAL"))

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
