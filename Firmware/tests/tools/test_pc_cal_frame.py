"""Synthetic BRINGUP OSL frame transfer; no serial device required."""

from __future__ import annotations

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from pc_cal_frame import BEGIN, END, receive_one  # noqa: E402
from Firmware.tests.tools.test_inspect_calibration_record import valid_frame  # noqa: E402


class FakeLink:
    def __init__(self, lines: list[str]):
        self.lines = iter((line + "\r\n").encode("ascii") for line in lines)

    def readline(self, limit: int) -> bytes:
        return next(self.lines, b"")[:limit]


def transfer(frame: bytes) -> list[str]:
    return [BEGIN] + ["D," + frame[offset:offset + 16].hex().upper()
                      for offset in range(0, len(frame), 16)] + [END]


class CalibrationFrameTransferTests(unittest.TestCase):
    def test_full_frame_with_banner(self):
        frame = valid_frame(full=True)
        received, summary = receive_one(FakeLink(["SAFE_BOOT"] + transfer(frame)), 0.5)
        self.assertEqual(received, frame)
        self.assertEqual(summary.sequence, 7)
        self.assertEqual(len(summary.conditions), 33)

    def test_rejects_truncated_duplicate_and_corrupt_frames(self):
        lines = transfer(valid_frame(full=True))
        for bad in (lines[:-2] + [END], lines[:3] + [BEGIN] + lines[3:],
                    lines[:2] + ["D," + "FF" * 16] + lines[3:]):
            with self.subTest(kind=bad[2]), self.assertRaises(ValueError):
                receive_one(FakeLink(bad), 0.1)
        with self.assertRaises(TimeoutError):
            receive_one(FakeLink(lines[:-1]), 0.01)

    def test_rejects_malformed_and_unsuccessful_transfer(self):
        frame = valid_frame(full=True)
        lines = transfer(frame)
        for bad in (["CAL_FRAME_BUSY"], ["CAL_FRAME_ERROR"], ["CAL_FRAME_UNAVAILABLE"],
                    lines[:2] + ["D,AA Z0"] + lines[2:],
                    lines[:2] + ["D,AA"] + lines[2:],
                    lines[:-1] + ["CAL_FRAME_END st=ERROR"]):
            with self.subTest(kind=bad[-1]), self.assertRaises(ValueError):
                receive_one(FakeLink(bad), 0.1)


if __name__ == "__main__":
    unittest.main()
