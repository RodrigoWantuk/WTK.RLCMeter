import importlib.util
from pathlib import Path
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[2] / "tools"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location("collect_flash_evidence", TOOLS / "collect_flash_evidence.py")
collector = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(collector)


class DiagnosticTwinTest(unittest.TestCase):
    def test_different_program_bytes_refused(self):
        with self.assertRaisesRegex(ValueError, "BIN differs"):
            collector.enrich_nm("", "", b"a", b"b")

    def test_only_matching_symbols_enriched(self):
        base = "08000000 00000008 T foo\n08000008 00000008 T bar\n"
        twin = "08000000 00000008 T foo\tC:/repo/Firmware/src/foo.c:12\n08000008 00000004 T bar\tC:/repo/Firmware/src/bar.c:9\n"
        actual = collector.enrich_nm(base, twin, b"same", b"same")
        self.assertIn("foo\tsrc/foo.c", actual)
        self.assertNotIn("bar.c", actual)


if __name__ == "__main__":
    unittest.main()
