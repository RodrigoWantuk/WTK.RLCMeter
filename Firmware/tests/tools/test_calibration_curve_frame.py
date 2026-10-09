"""Non-qualified supplementary curve candidate wire-format checks."""

import copy
import struct
import sys
import unittest
import zlib
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from calibration_curve_frame import (  # noqa: E402
    CurveFrameError, HEADER, RECORD, TRAILER, build_candidate, inspect_candidate,
)


def report():
    return {
        "status": "HOST_PROVISIONAL_NOT_FLASH_READY",
        "qualification": "UNQUALIFIED",
        "capture_binding": "RAW_SHA256_BOUND",
        "supplied_osl_sequence": 42,
        "supplied_osl_frame_crc32": 0x12345678,
        "supplied_osl_frame_sha256": "a" * 64,
        "conditions": [{
            "condition": {"range_ohms": 1000, "frequency_hz": 1000,
                          "amplitude_mv_rms": 100},
            "curve": [1.0, 0.0, 0.0, 1.0] * 3,
            "fit_count": 2,
            "validation_count": 1,
            "validation": "PASS",
        }],
    }


class CurveFrameTests(unittest.TestCase):
    def test_exact_wire_format_and_identity(self):
        frame = build_candidate(report(), 7)
        self.assertEqual(len(frame), HEADER.size + RECORD.size + TRAILER.size)
        self.assertEqual(HEADER.size, 28)
        self.assertEqual(RECORD.size, 56)
        inspected = inspect_candidate(frame)
        self.assertEqual(inspected["record_count"], 1)
        self.assertEqual(inspected["osl_sequence"], 42)
        self.assertEqual(inspected["osl_frame_crc32"], 0x12345678)
        self.assertFalse(inspected["qualified"])
        self.assertEqual(inspected["conditions"][0]["curve"], [1.0, 0.0, 0.0, 1.0] * 3)

    def test_no_unbound_or_unvalidated_export(self):
        for mutate in (
                lambda value: value.update(capture_binding="SYNTHETIC_UNBOUND"),
                lambda value: value.update(status="HOST_VALIDATION_FAILED"),
                lambda value: value["conditions"][0].update(validation="FAIL"),
                lambda value: value["conditions"][0].update(validation_count=0),
                lambda value: value["conditions"][0].update(fit_count=0),
                lambda value: value.update(supplied_osl_frame_crc32=None)):
            value = report()
            mutate(value)
            with self.subTest(value=value), self.assertRaises(CurveFrameError):
                build_candidate(value, 1)

    def test_forbidden_duplicate_and_nonfinite_rejected(self):
        value = report()
        value["conditions"][0]["condition"]["range_ohms"] = 10
        value["conditions"][0]["condition"]["amplitude_mv_rms"] = 500
        with self.assertRaises(ValueError):
            build_candidate(value, 1)
        value = report()
        value["conditions"].append(copy.deepcopy(value["conditions"][0]))
        with self.assertRaises(CurveFrameError):
            build_candidate(value, 1)
        value = report()
        value["conditions"][0]["curve"][0] = float("nan")
        with self.assertRaises(CurveFrameError):
            build_candidate(value, 1)

    def test_corruption_rejected(self):
        frame = bytearray(build_candidate(report(), 7))
        frame[HEADER.size + 4] ^= 1
        with self.assertRaises(CurveFrameError):
            inspect_candidate(bytes(frame))
        frame = bytearray(build_candidate(report(), 7))
        struct.pack_into("<I", frame, 20, 0)
        struct.pack_into("<I", frame, len(frame) - TRAILER.size,
                         zlib.crc32(frame[:-TRAILER.size]))
        with self.assertRaises(CurveFrameError):
            inspect_candidate(bytes(frame))
        frame = bytearray(build_candidate(report(), 7))
        frame[-1] ^= 1
        with self.assertRaises(CurveFrameError):
            inspect_candidate(bytes(frame))


if __name__ == "__main__":
    unittest.main()
