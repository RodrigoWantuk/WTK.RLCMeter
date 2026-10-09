"""GNU map fixtures include LTO, archive paths, aliases and string relaxation."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[2] / "tools" / "flash_attribution.py"
SPEC = importlib.util.spec_from_file_location("flash_attribution", TOOL)
flash = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(flash)

MAP = """Archive member included to satisfy reference by file (symbol)
libproject.a(foo.c.obj)
                              main.o (foo)
Discarded input sections
 .text.dead 0x00000000 0x900 libproject.a(dead.c.obj)
Linker script and memory map
.text 0x08000000 0x10
 .text.foo
                0x08000000 0x8 libproject.a(foo.c.obj)
 .text 0x08000008 0x6 C:/Program Files/Arm/libgcc.a(_arm_addsubsf3.o)
 *fill* 0x0800000e 0x2
.rodata 0x08000010 0x8
 .rodata.str1.1 0x08000010 0x6 C:/Temp/random.ltrans0.ltrans.o
 .rodata.str1.1 0x08000016 0x90 C:/Temp/random.ltrans1.ltrans.o
 *fill* 0x08000016 0x2
.data 0x20000000 0x4 load address 0x08000018
 .data 0x20000000 0x4 CMakeFiles/fw.dir/src/app/context.c.obj
.bss 0x20000004 0x4
 .bss 0x20000004 0x4 libproject.a(foo.c.obj)
Cross Reference Table
Symbol File
foo libproject.a(foo.c.obj)
"""
SECTIONS = """Sections:
Idx Name Size VMA LMA File off Algn
 0 .text 00000010 08000000 08000000 00001000 2**2
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
 1 .rodata 00000008 08000010 08000010 00001010 2**2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
 2 .data 00000004 20000000 08000018 00002000 2**2
                  CONTENTS, ALLOC, LOAD, DATA
 3 .bss 00000004 20000004 0800001c 00002004 2**2
                  ALLOC
 4 .noinit 00000004 20000008 08000020 00002008 2**2
                  ALLOC
 5 ._user_heap_stack 00000804 2000000c 08000020 00002008 2**2
                  ALLOC
 6 .debug_info 00005000 00000000 00000000 00003000 2**0
                  CONTENTS, READONLY, DEBUGGING, OCTETS
"""
NM = """08000000 00000008 T foo\tC:/repo/Firmware/src/app/foo.c:10
08000000 00000008 T foo_alias\tC:/repo/Firmware/src/app/foo.c:10
08000008 00000006 T __aeabi_fadd
0800000a 00000004 T nested_alias
08000010 00000006 r table
20000000 00000004 d initialized
20000004 00000004 b workspace
20005000 A _estack
         U missing_optional
"""
BIN = bytes(16) + b"hello\0\0\0" + bytes(4)


class FlashAttributionTest(unittest.TestCase):
    def report(self, **kwargs):
        inputs = dict(map_text=MAP, nm_text=NM, section_text=SECTIONS, binary=BIN)
        inputs.update(kwargs)
        return flash.analyze(**inputs)

    def test_flash_uses_load_addresses_not_elf_or_ram_size(self):
        report = self.report()
        self.assertEqual(report["flash_bytes"], 28)
        self.assertEqual(report["ram_accounted_bytes"], 2064)
        self.assertEqual(report["flash_remaining_bytes"], 65536 - 28)

    def test_views_reconcile_without_counting_aliases_twice(self):
        report = self.report()
        for view in ("objects", "modules", "runtime_families"):
            self.assertEqual(sum(r["bytes"] for r in report[view]), 28)
        self.assertEqual(sum(r["bytes"] for r in report["contributions"]), 28)
        foo = next(r for r in report["symbols"] if "foo" in r["names"])
        self.assertEqual(foo["names"], ["foo", "foo_alias"])
        self.assertFalse(foo["overlaps_other_symbols"])
        self.assertTrue(next(r for r in report["symbols"] if "nested_alias" in r["names"])["overlaps_other_symbols"])

    def test_discarded_objects_not_linked_bytes(self):
        report = self.report()
        self.assertNotIn("dead.c", json.dumps(report))
        self.assertEqual(report["archive_inclusions"], ["libproject.a(foo.c.obj)"])

    def test_archive_spaces_and_lto_random_paths(self):
        report = self.report()
        names = {r["name"] for r in report["objects"]}
        self.assertIn("libgcc.a(_arm_addsubsf3.o)", names)
        self.assertIn("LTO/ltrans0.ltrans.o", names)
        self.assertNotIn("C:/", json.dumps(report))

    def test_relaxation_is_unattributed_and_warned(self):
        report = self.report()
        self.assertTrue(report["warnings"])
        self.assertEqual(next(r["bytes"] for r in report["objects"] if r["name"] == "UNATTRIBUTED"), 2)

    def test_overlapping_owners_unattributed(self):
        report = self.report(map_text=MAP.replace(".text 0x08000008 0x6", ".text 0x08000004 0xa"))
        row = next(r for r in report["contributions"] if r["address"] == 0x08000004)
        self.assertEqual(row["object"], "UNATTRIBUTED")

    def test_missing_debug_info_is_not_guessed_from_names(self):
        report = self.report(nm_text=NM.replace("\tC:/repo/Firmware/src/app/foo.c:10", ""))
        self.assertNotIn("src/app/foo.c", json.dumps(report))

    def test_ambiguous_alias_sources_unattributed(self):
        report = self.report(nm_text=NM.replace("foo_alias\tC:/repo/Firmware/src/app/foo.c:10", "foo_alias\tC:/repo/Firmware/src/app/other.c:10"))
        self.assertEqual(report["contributions"][0]["module"], "UNATTRIBUTED")

    def test_strings_are_nonadditive_candidates(self):
        self.assertEqual(self.report()["string_candidates"], [dict(address=0x08000010, bytes=6, text="hello")])

    def test_bad_map_rejected(self):
        for text in ("", "corrupt", "Linker script and memory map\n.text bad 0x10"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.report(map_text=text)

    def test_bad_nm_rejected(self):
        for text in ("", "08000000 nope T foo", NM + "bad data\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.report(nm_text=text)

    def test_map_elf_mismatch_rejected(self):
        with self.assertRaises(ValueError):
            self.report(map_text=MAP.replace(".text 0x08000000 0x10", ".text 0x08000000 0x20"))

    def test_overlapping_flash_sections_rejected(self):
        with self.assertRaises(ValueError):
            self.report(section_text=SECTIONS.replace("08000010 08000010", "08000008 08000008"))

    def test_no_load_sections_rejected(self):
        with self.assertRaises(ValueError):
            self.report(section_text=SECTIONS.replace("ALLOC, LOAD", "ALLOC"))

    def test_binary_size_mismatch_rejected(self):
        with self.assertRaises(ValueError):
            self.report(binary=BIN[:-1])

    def test_deterministic_outputs(self):
        first, second = self.report(), self.report(map_text=MAP.replace("random.ltrans", "different.ltrans"))
        self.assertEqual(json.dumps(first, sort_keys=True), json.dumps(second, sort_keys=True))
        self.assertEqual(flash.csv_report(first), flash.csv_report(second))
        self.assertEqual(flash.markdown_report(first), flash.markdown_report(second))

    def test_diff_reconciles_and_preserves_zero(self):
        baseline, candidate = self.report(), self.report()
        candidate["flash_bytes"] += 2
        diff = flash.compare(baseline, candidate)
        self.assertEqual(diff["flash_bytes_delta"], 2)
        self.assertEqual(diff["ram_accounted_bytes_delta"], 0)
        self.assertTrue(all(r["delta_bytes"] == 0 for r in diff["modules"]))

    def test_non_object_baseline_refused(self):
        with self.assertRaises(ValueError):
            flash.compare([], self.report())

    def test_load_span_accounts_for_inter_section_gap(self):
        sections = SECTIONS.replace("20000000 08000018", "20000000 0800001c")
        report = self.report(section_text=sections, binary=BIN + bytes(4))
        self.assertEqual(report["flash_bytes"], 28)
        self.assertEqual(report["flash_load_span_bytes"], 32)
        self.assertEqual(report["flash_gap_bytes"], 4)

    def test_formatting_symbol_detection(self):
        self.assertEqual(self.report(nm_text=NM.replace("nested_alias", "snprintf"))["formatting_symbols"], ["snprintf"])

    def test_missing_input_cli_error_without_traceback(self):
        with tempfile.TemporaryDirectory() as temp:
            result = subprocess.run([sys.executable, str(TOOL), "--map", temp + "/absent", "--nm", temp + "/absent",
                                     "--sections", temp + "/absent", "--profile", "test", "--git-sha", "test",
                                     "--json-out", temp + "/out.json"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertNotIn("Traceback", result.stderr)
            self.assertFalse(Path(temp, "out.json").exists())

    def test_cli_deterministic_json_csv(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name, content in (("map", MAP), ("nm", NM), ("sections", SECTIONS)):
                (root / name).write_text(content, encoding="utf-8")
            command = [sys.executable, str(TOOL), "--profile", "test", "--git-sha", "test"]
            for name in ("map", "nm", "sections"):
                command += ["--" + name, str(root / name)]
            command += ["--json-out", str(root / "out.json"), "--csv-out", str(root / "out.csv")]
            subprocess.run(command, check=True, capture_output=True)
            first = [(root / name).read_bytes() for name in ("out.json", "out.csv")]
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual(first, [(root / name).read_bytes() for name in ("out.json", "out.csv")])


if __name__ == "__main__":
    unittest.main()
