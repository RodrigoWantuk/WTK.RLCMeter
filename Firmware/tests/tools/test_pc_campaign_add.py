"""Campaign entry tests use synthetic RAW, not physical calibration evidence."""

from __future__ import annotations

import hashlib
import io
import json
from contextlib import redirect_stdout
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from calibration_campaign import CampaignConflict, CampaignError, all_osl_conditions, campaign_template  # noqa: E402
from inspect_calibration_record import OslFrameSummary  # noqa: E402
from pc_campaign_add import add_capture_standard  # noqa: E402
import pc_campaign_add  # noqa: E402


def raw_capture(real_mohm: int = 1005000, imag_mohm: int = 0,
                row0_vexc: int = 2048) -> bytes:
    header = ["RAW_BEGIN v=1", "m=DUT_MEASURE", "f=1000", "a=100", "r=1K",
              "sr=64000", "n=256", "wps=3", "calibration_sequence=7",
              "pi=10", "pv=11", "k1op=10", "k1rel=8", "DSP_BEGIN v=1",
              "calibration=FOUND source=PERSISTED", "dsp_status=OK",
              "return_channel=RET_1X", f"z_real_mohm={real_mohm}",
              f"z_imag_mohm={imag_mohm}", "DSP_END", "i,v1,r1,v2,rh,vm1,vm2"]
    rows = [f"{index},{row0_vexc if index == 0 else 2048},2048,2048,2048,2048,2048"
            for index in range(256)]
    return ("\n".join(header + rows + ["RAW_END st=OK", ""])).encode("ascii")


FRAME = OslFrameSummary(7, "synthetic-frame", frozenset(all_osl_conditions()))


class CampaignEntryTests(unittest.TestCase):
    def test_add_to_correct_condition_without_mutating_input(self):
        data = campaign_template()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            raw = raw_capture()
            (root / "capture.raw").write_bytes(raw)
            added, report = add_capture_standard(data, root, FRAME, "capture.raw",
                                                 "R-001", "R", 1000.0, 0.01)
        self.assertEqual(data["conditions"], [])
        self.assertEqual(added["conditions"][0]["condition"], {
            "range_ohms": 1000, "frequency_hz": 1000, "amplitude_mv_rms": 100})
        standard = added["conditions"][0]["standards"][0]
        self.assertEqual(standard["capture_id"], hashlib.sha256(raw).hexdigest())
        self.assertEqual(standard["datasheet_frequency_hz"], 1000)
        self.assertAlmostEqual(report["conditions"][0]["standard_evidence"][0]["corrected_value_si"],
                               1005.0)
        self.assertEqual(report["qualification"], "UNQUALIFIED")

    def test_duplicate_id_or_capture_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "capture.raw").write_bytes(raw_capture())
            first, _ = add_capture_standard(campaign_template(), root, FRAME,
                                            "capture.raw", "R-001", "R", 1000.0, 0.01)
            with self.assertRaises(CampaignError):
                add_capture_standard(first, root, FRAME, "capture.raw",
                                     "R-001", "R", 1000.0, 0.01)
            with self.assertRaises(CampaignError):
                add_capture_standard(first, root, FRAME, "capture.raw",
                                     "R-002", "R", 1000.0, 0.01)
            with self.assertRaises(CampaignError):
                add_capture_standard(campaign_template(), root, FRAME, "capture.raw",
                                     "R-003", "R", 1000.0, 0.01,
                                     extra={"capture_id": "forged"})

    def test_wrong_osl_sequence_and_path_escape_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "captures"
            root.mkdir()
            (root / "capture.raw").write_bytes(raw_capture())
            (root.parent / "outside.raw").write_bytes(raw_capture())
            wrong = OslFrameSummary(8, "wrong", FRAME.conditions)
            with self.assertRaises(CampaignError):
                add_capture_standard(campaign_template(), root, wrong,
                                     "capture.raw", "R-001", "R", 1000.0, 0.01)
            with self.assertRaises(CampaignError):
                add_capture_standard(campaign_template(), root, FRAME,
                                     "../outside.raw", "R-001", "R", 1000.0, 0.01)

    def test_conflicting_new_fit_does_not_mutate_existing_campaign(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "first.raw").write_bytes(raw_capture())
            (root / "second.raw").write_bytes(raw_capture(row0_vexc=2049))
            first, _ = add_capture_standard(campaign_template(), root, FRAME,
                                            "first.raw", "R-001", "R", 900.0, 0.01)
            before = json.dumps(first, sort_keys=True)
            with self.assertRaises(CampaignConflict):
                add_capture_standard(first, root, FRAME, "second.raw",
                                     "R-002", "R", 1100.0, 0.01)
            self.assertEqual(json.dumps(first, sort_keys=True), before)

    def test_failed_holdout_is_recorded_but_cannot_pull_fit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "fit.raw").write_bytes(raw_capture())
            (root / "holdout.raw").write_bytes(raw_capture(real_mohm=1200000))
            first, _ = add_capture_standard(campaign_template(), root, FRAME,
                                            "fit.raw", "R-001", "R", 1000.0, 0.01)
            added, report = add_capture_standard(first, root, FRAME, "holdout.raw",
                                                 "R-002", "R", 1000.0, 0.01,
                                                 role="VALIDATION")
        self.assertEqual(len(added["conditions"][0]["standards"]), 2)
        self.assertEqual(report["status"], "HOST_VALIDATION_FAILED")
        self.assertEqual(report["conditions"][0]["failed_validation_ids"], ["R-002"])

    def test_optional_capacitor_loss_fields_reach_joint_fit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "cap.raw").write_bytes(raw_capture(real_mohm=3000, imag_mohm=-159155))
            extra = {"esr_max_ohms": 2.0, "d_max": 0.01,
                     "board_temperature_calibrated": True,
                     "board_temperature_c": 25.0, "datasheet_temperature_c": 25.0}
            _, report = add_capture_standard(campaign_template(), root, FRAME, "cap.raw",
                                             "C-001", "C", 1.0e-6, 0.1, extra=extra)
        evidence = report["conditions"][0]["standard_evidence"][0]
        self.assertTrue(evidence["within_all_specified_intervals"])
        self.assertLessEqual(evidence["corrected_z_re_ohms"], 2.0 + 1.0e-6)
        self.assertLessEqual(evidence["corrected_z_re_ohms"] /
                             -evidence["corrected_z_im_ohms"], 0.01 + 1.0e-6)

    def test_cli_writes_campaign_atomically_and_reports_added_sha(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            campaign_path = root / "campaign.json"
            campaign_path.write_text(json.dumps(campaign_template()), encoding="utf-8")
            (root / "active.bin").write_bytes(b"synthetic frame")
            raw = raw_capture()
            (root / "capture.raw").write_bytes(raw)
            argv = ["pc_campaign_add.py", str(campaign_path), "--capture-root", str(root),
                    "--osl-frame", str(root / "active.bin"), "--capture", "capture.raw",
                    "--id", "R-001", "--type", "R", "--nominal-si", "1000",
                    "--tolerance-fraction", "0.01"]
            with patch.object(sys, "argv", argv), \
                    patch.object(pc_campaign_add, "decode_full_rev1_frame", return_value=FRAME), \
                    redirect_stdout(io.StringIO()) as output:
                self.assertEqual(pc_campaign_add.main(), 0)
            self.assertIn(hashlib.sha256(raw).hexdigest(), output.getvalue())
            self.assertEqual(len(json.loads(campaign_path.read_text(encoding="utf-8"))["conditions"]), 1)
            self.assertEqual(list(root.glob("campaign.json.*.tmp")), [])

    def test_cli_conflict_preserves_previous_campaign_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            campaign_path = root / "campaign.json"
            (root / "active.bin").write_bytes(b"synthetic frame")
            (root / "first.raw").write_bytes(raw_capture())
            (root / "second.raw").write_bytes(raw_capture(row0_vexc=2049))
            first, _ = add_capture_standard(campaign_template(), root, FRAME,
                                            "first.raw", "R-001", "R", 900.0, 0.01)
            campaign_path.write_text(json.dumps(first), encoding="utf-8")
            before = campaign_path.read_bytes()
            argv = ["pc_campaign_add.py", str(campaign_path), "--capture-root", str(root),
                    "--osl-frame", str(root / "active.bin"), "--capture", "second.raw",
                    "--id", "R-002", "--type", "R", "--nominal-si", "1100",
                    "--tolerance-fraction", "0.01"]
            with patch.object(sys, "argv", argv), \
                    patch.object(pc_campaign_add, "decode_full_rev1_frame", return_value=FRAME), \
                    patch("sys.stderr", new=io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    pc_campaign_add.main()
            self.assertEqual(error.exception.code, 2)
            self.assertEqual(campaign_path.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
