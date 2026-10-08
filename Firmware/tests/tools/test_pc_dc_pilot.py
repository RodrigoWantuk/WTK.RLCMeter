"""Synthetic DC pilot COM frames; no serial device or DUT needed."""

from __future__ import annotations

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from pc_dc_pilot import receive_pilot, validate_pilot  # noqa: E402


FRAME = ("DC_PILOT_BEGIN range=1m ac=1khz,100mv ccr=81 adc_cal=IDEAL_UNQUALIFIED\n"
         "DC_PILOT_AC_OK residual=REQUALIFIED\n"
         "DC_PILOT_RESULT status=EXPLORATORY resistance_ohm=923076 "
         "source_uv=20146 dut_uv=9670 current_na=10\n"
         "DC_PILOT_END status=SAFE\n")


class FakeLink:
    def __init__(self, text: str):
        self.lines = iter(text.encode("ascii").splitlines(keepends=True))

    def readline(self, limit: int) -> bytes:
        return next(self.lines, b"")[:limit]


class PilotTests(unittest.TestCase):
    def test_complete_safe_pilot(self):
        text, result = receive_pilot(FakeLink("SAFE_BOOT\n" + FRAME), 0.1)
        self.assertEqual(text, FRAME)
        self.assertEqual(result["adc_cal"], "IDEAL_UNQUALIFIED")
        self.assertEqual(result["resistance_ohm"], "923076")

    def test_unresolved_has_no_resistance(self):
        frame = FRAME.replace("status=EXPLORATORY resistance_ohm=923076",
                              "status=CURRENT_UNRESOLVED")
        self.assertEqual(validate_pilot(frame)["status"], "CURRENT_UNRESOLVED")
        with self.assertRaises(ValueError):
            validate_pilot(frame.replace("status=CURRENT_UNRESOLVED",
                                         "status=CURRENT_UNRESOLVED resistance_ohm=10"))

    def test_failed_teardown_and_missing_ac_requalification_rejected(self):
        with self.assertRaises(ValueError):
            validate_pilot(FRAME.replace("status=SAFE", "status=TRANSPORT_FAILED"))
        with self.assertRaises(ValueError):
            validate_pilot(FRAME.replace("DC_PILOT_AC_OK residual=REQUALIFIED\n", ""))

    def test_missing_or_duplicate_result_rejected(self):
        with self.assertRaises(ValueError):
            validate_pilot(FRAME.replace("DC_PILOT_RESULT status=EXPLORATORY", "DC_PILOT_RESULT status=INVALID"))
        with self.assertRaises(ValueError):
            validate_pilot(FRAME.replace("DC_PILOT_END", "DC_PILOT_RESULT status=INVALID\nDC_PILOT_END"))

    def test_timeout(self):
        with self.assertRaises(TimeoutError):
            receive_pilot(FakeLink("DC_PILOT_BEGIN range=1m\n"), 0.001)


if __name__ == "__main__":
    unittest.main()
