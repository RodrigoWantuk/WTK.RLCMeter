import importlib.util
from pathlib import Path
import unittest


TOOL_PATH = Path(__file__).resolve().parents[2] / "tools" / "firmware_size.py"
SPEC = importlib.util.spec_from_file_location("firmware_size", TOOL_PATH)
firmware_size = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(firmware_size)


class FirmwareSizeToolTest(unittest.TestCase):
    def test_product_budget_has_preferred_and_hard_ram_gates(self):
        preferred, hard = firmware_size.ram_limits_for_budget("product")
        self.assertEqual(preferred, 16 * 1024)
        self.assertEqual(hard, 17 * 1024)

    def test_legacy_release_budget_aliases_product(self):
        self.assertEqual(firmware_size.normalized_budget_name("release"), "product")
        self.assertEqual(
            firmware_size.ram_limits_for_budget("release"),
            firmware_size.ram_limits_for_budget("product"),
        )

    def test_bringup_budget_uses_separate_hard_ram_gate(self):
        preferred, hard = firmware_size.ram_limits_for_budget("bringup")
        self.assertIsNone(preferred)
        self.assertEqual(hard, 18 * 1024)

    def test_unbudgeted_build_has_no_project_ram_gate(self):
        self.assertEqual(firmware_size.ram_limits_for_budget("none"), (None, None))


if __name__ == "__main__":
    unittest.main()
