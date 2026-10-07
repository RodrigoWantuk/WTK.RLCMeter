"""Synthetic passive BRINGUP COM capture tests; no serial device needed."""

from __future__ import annotations

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from pc_capture import receive_one, validate_dut_capture  # noqa: E402


def dump(mode: str = "DUT_MEASURE", end: str = "RAW_END st=OK") -> str:
    lines = ["RAW_BEGIN v=1", f"m={mode}", "f=1000", "a=100", "r=1K",
             "sr=64000", "n=256", "wps=3", "pi=1", "pv=2",
             "k1op=10", "k1rel=8", "i,v1,r1,v2,rh,vm1,vm2"]
    lines.extend(f"{index},2048,2048,2048,2048,2048,2048" for index in range(256))
    lines.append(end)
    return "\n".join(lines) + "\n"


class FakeLink:
    def __init__(self, text: str):
        self.lines = iter(text.encode("ascii").splitlines(keepends=True))

    def readline(self, limit: int) -> bytes:
        return next(self.lines, b"")[:limit]


class CaptureTests(unittest.TestCase):
    def test_complete_dump_after_unrelated_banner(self):
        text, capture = receive_one(FakeLink("SAFE_BOOT\n" + dump()), 0.1)
        self.assertTrue(text.startswith("RAW_BEGIN v=1"))
        self.assertEqual(len(capture.rows), 256)

    def test_reject_capture_mode_and_failure(self):
        with self.assertRaises(ValueError):
            validate_dut_capture(dump(mode="SAFE_CAPTURE"))
        with self.assertRaises(ValueError):
            validate_dut_capture(dump(end="RAW_END st=ERROR"))

    def test_reject_missing_row(self):
        text = dump().replace("42,2048,2048,2048,2048,2048,2048\n", "")
        with self.assertRaises(ValueError):
            validate_dut_capture(text)

    def test_reject_adc_out_of_range(self):
        text = dump().replace("42,2048", "42,4096", 1)
        with self.assertRaises(ValueError):
            validate_dut_capture(text)

    def test_incomplete_dump_times_out(self):
        with self.assertRaises(TimeoutError):
            receive_one(FakeLink("RAW_BEGIN v=1\n"), 0.001)


if __name__ == "__main__":
    unittest.main()
