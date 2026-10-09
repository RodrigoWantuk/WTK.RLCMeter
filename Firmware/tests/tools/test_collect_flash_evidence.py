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
    def test_command_capture_never_invokes_build_or_regeneration(self):
        commands = collector.build_graph_commands(Path("saved-build"), "CMAKE_GENERATOR:INTERNAL=Ninja")
        self.assertEqual(commands, {
            "build-targets.txt": ["ninja", "-C", "saved-build", "-t", "targets", "all"],
            "commands.txt": ["ninja", "-C", "saved-build", "-t", "commands"],
        })

    def test_other_generators_do_not_fall_back_to_cmake_build(self):
        self.assertEqual(collector.build_graph_commands(Path("saved-build"), "CMAKE_GENERATOR:INTERNAL=Other"), {})

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
