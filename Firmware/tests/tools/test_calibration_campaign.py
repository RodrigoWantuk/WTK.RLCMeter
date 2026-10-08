"""Synthetic tests for the host-only interval calibration prototype."""

from __future__ import annotations

import math
import hashlib
import io
import json
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from calibration_campaign import (  # noqa: E402
    CampaignConflict,
    CampaignError,
    IDENTITY,
    all_osl_conditions,
    bind_campaign_captures,
    campaign_template,
    constraint_rank,
    corrected_impedance,
    minimum_change_fit,
    solve_campaign,
    standard_constraints,
)
from inspect_calibration_record import OslFrameSummary  # noqa: E402
import calibration_campaign  # noqa: E402


KEY = (1000, 1000, 100)


def standard(kind: str, nominal: float, real: float, imag: float,
             tolerance: float = 0.01, **extra):
    return {
        "id": "sample-1", "type": kind, "nominal_si": nominal,
        "capture_id": "capture-1", "capture_safe": True,
        "capture_dsp_status": "OK", "capture_calibration_sequence": 1,
        "tolerance_fraction": tolerance, "frequency_hz": 1000,
        "amplitude_mv_rms": 100, "datasheet_frequency_hz": 1000,
        "measured_z_re_ohms": real, "measured_z_im_ohms": imag, **extra,
    }


def campaign(standards, prior=None):
    return {
        "schema_version": 1, "hardware_revision": 0x00010001,
        "osl_conditions": [
            {"range_ohms": r, "frequency_hz": f, "amplitude_mv_rms": a}
            for r, f, a in sorted(all_osl_conditions())
        ],
        "conditions": [{
            "condition": {"range_ohms": 1000, "frequency_hz": 1000,
                          "amplitude_mv_rms": 100},
            "prior_curve": list(IDENTITY if prior is None else prior),
            "standards": standards,
        }],
    }


def raw_capture() -> str:
    header = ["RAW_BEGIN v=1", "m=DUT_MEASURE", "f=1000", "a=100", "r=1K",
              "sr=64000", "n=256", "wps=3", "calibration_sequence=7",
              "pi=10", "pv=11", "k1op=10", "k1rel=8", "DSP_BEGIN v=1",
              "calibration=FOUND source=PERSISTED", "dsp_status=OK",
              "return_channel=RET_1X", "z_real_mohm=1005000",
              "z_imag_mohm=0", "DSP_END", "i,v1,r1,v2,rh,vm1,vm2"]
    return "\n".join(header +
                     [f"{index},2048,2048,2048,2048,2048,2048"
                      for index in range(256)] + ["RAW_END st=OK", ""])


class CampaignTests(unittest.TestCase):
    def test_template_is_only_a_key_skeleton(self):
        template = campaign_template()
        self.assertEqual(len(template["osl_conditions"]), 33)
        self.assertEqual(template["conditions"], [])

    def test_full_osl_required(self):
        data = campaign([])
        data["osl_conditions"].pop()
        with self.assertRaises(CampaignError):
            solve_campaign(data)

    def test_unchanged_when_within_tolerance(self):
        result = solve_campaign(campaign([standard("R", 1000, 1005, 0)]))
        self.assertEqual(result["conditions"][0]["curve"], list(IDENTITY))
        self.assertEqual(result["conditions"][0]["constraint_rank"], 1)
        self.assertEqual(result["conditions"][0]["coverage"], "PARTIAL_LINEAR_SPAN")
        self.assertEqual(result["qualification"], "UNQUALIFIED")

    def test_validation_standards_do_not_pull_fit_and_can_fail(self):
        fit = standard("R", 1000, 1000, 0)
        check = standard("R", 1000, 1000, 0, role="VALIDATION")
        check["id"] = "held-out"
        check["capture_id"] = "held-out-capture"
        passed = solve_campaign(campaign([fit, check]))["conditions"][0]
        self.assertEqual(passed["curve"], list(IDENTITY))
        self.assertEqual(passed["fit_count"], 1)
        self.assertEqual(passed["validation_count"], 1)
        self.assertEqual(passed["validation"], "PASS")
        check["measured_z_re_ohms"] = 1200
        failed_campaign = solve_campaign(campaign([fit, check]))
        failed = failed_campaign["conditions"][0]
        self.assertEqual(failed["curve"], passed["curve"])
        self.assertEqual(failed["validation"], "FAIL")
        self.assertEqual(failed["failed_validation_ids"], ["held-out"])
        self.assertEqual(failed["standard_evidence"][0]["role"], "FIT")
        self.assertTrue(failed["standard_evidence"][0]["within_all_specified_intervals"])
        self.assertEqual(failed["standard_evidence"][1]["role"], "VALIDATION")
        self.assertFalse(failed["standard_evidence"][1]["within_all_specified_intervals"])
        self.assertEqual(failed["standard_evidence"][1]["corrected_value_si"], 1200.0)
        self.assertEqual(failed["standard_evidence"][1]["specified_max_si"], 1010.0)
        self.assertEqual(failed_campaign["status"], "HOST_VALIDATION_FAILED")

    def test_evidence_reports_physical_values_without_qualification(self):
        omega = 2.0 * math.pi * 1000.0
        cases = (("R", 1000.0, 1000.0, 0.0),
                 ("C", 1.0e-6, 2.0, -1.0 / (omega * 1.0e-6)),
                 ("L", 0.1, 2.0, omega * 0.1))
        for kind, nominal, real, imag in cases:
            with self.subTest(kind=kind):
                result = solve_campaign(campaign([standard(kind, nominal, real, imag)]))
                evidence = result["conditions"][0]["standard_evidence"][0]
                self.assertEqual(result["qualification"], "UNQUALIFIED")
                self.assertEqual(evidence["type"], kind)
                self.assertTrue(evidence["within_all_specified_intervals"])
                self.assertAlmostEqual(evidence["corrected_value_si"], nominal,
                                       delta=nominal * 1.0e-6)
                self.assertAlmostEqual(evidence["corrected_z_re_ohms"], real, places=6)
                self.assertAlmostEqual(evidence["corrected_z_im_ohms"], imag, places=6)

    def test_nonfinite_standard_constraint_rejected(self):
        with self.assertRaises(CampaignError):
            solve_campaign(campaign([standard("L", 1.0e308, 1000.0, 0.0)]))

    def test_validation_failure_cli_exits_nonzero(self):
        fit = standard("R", 1000, 1000, 0)
        check = standard("R", 1000, 1200, 0, role="VALIDATION")
        check["id"] = "held-out"
        check["capture_id"] = "held-out-capture"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "campaign.json"
            path.write_text(json.dumps(campaign([fit, check])), encoding="utf-8")
            with patch.object(sys, "argv", ["calibration_campaign.py", str(path), "--synthetic-unbound"]), \
                    redirect_stdout(io.StringIO()) as output:
                self.assertEqual(calibration_campaign.main(), 1)
            self.assertEqual(json.loads(output.getvalue())["status"], "HOST_VALIDATION_FAILED")

    def test_constraint_rank_is_span_not_sample_count(self):
        unit = [([float(index == column) for index in range(12)], 1.0)
                for column in range(12)]
        self.assertEqual(constraint_rank(unit), 12)
        self.assertEqual(constraint_rank(unit + [unit[0]] * 200), 12)
        self.assertEqual(constraint_rank(unit[:-1]), 11)
        invalid = standard("R", 1000, 1000, 0, role="UNKNOWN")
        with self.assertRaises(CampaignError):
            solve_campaign(campaign([invalid]))

    def test_old_curve_moves_to_current_boundary(self):
        prior = list(IDENTITY)
        prior[4] = 1.1
        result = solve_campaign(campaign([standard("R", 1000, 1000, 0)], prior))
        curve = result["conditions"][0]["curve"]
        corrected, _ = corrected_impedance(curve, 1000, 0, 1000)
        self.assertAlmostEqual(corrected, 1010.0, places=5)
        self.assertGreater(curve[4], 1.0)

    def test_all_current_standards_jointly_constrain(self):
        standards = [standard("R", 1000, 1020, 0)]
        second = standard("R", 1000, 990, 0)
        second["id"] = "sample-2"
        second["capture_id"] = "capture-2"
        standards.append(second)
        curve = solve_campaign(campaign(standards))["conditions"][0]["curve"]
        for raw in (1020, 990):
            corrected, _ = corrected_impedance(curve, raw, 0, 1000)
            self.assertGreaterEqual(corrected, 990 - 1e-6)
            self.assertLessEqual(corrected, 1010 + 1e-6)

    def test_conflicting_standards_rejected(self):
        standards = [standard("R", 900, 1000, 0)]
        second = standard("R", 1100, 1000, 0)
        second["id"] = "sample-2"
        second["capture_id"] = "capture-2"
        standards.append(second)
        with self.assertRaisesRegex(CampaignConflict, "sample-1.*sample-2"):
            solve_campaign(campaign(standards))

    def test_capacitor_optional_esr_and_d_independent(self):
        x = -1.0 / (2.0 * math.pi * 1000 * 1e-6)
        base = standard("C", 1e-6, 3.0, x, tolerance=0.1)
        self.assertTrue(standard_constraints(base, KEY))
        temp = {"board_temperature_calibrated": True,
                "board_temperature_c": 25, "datasheet_temperature_c": 25}
        self.assertTrue(standard_constraints({**base, **temp, "esr_max_ohms": 2.0}, KEY))
        self.assertTrue(standard_constraints({**base, **temp, "d_max": 0.01}, KEY))
        both = {**base, **temp, "esr_max_ohms": 2.0, "d_max": 0.01}
        curve = solve_campaign(campaign([both]))["conditions"][0]["curve"]
        corrected_r, corrected_x = corrected_impedance(curve, 3.0, x, 1000)
        self.assertLessEqual(corrected_r, 2.0 + 1e-6)
        self.assertLessEqual(corrected_r / -corrected_x, 0.01 + 1e-6)

    def test_capacitor_nominal_esr_and_d_with_explicit_tolerance(self):
        x = -1.0 / (2.0 * math.pi * 1000 * 1e-6)
        temp = {"board_temperature_calibrated": True,
                "board_temperature_c": 25, "datasheet_temperature_c": 25}
        esr = standard("C", 1e-6, 3.0, x, tolerance=0.1, **temp,
                       esr_nominal_ohms=3.3, esr_tolerance_fraction=0.1)
        d = standard("C", 1e-6, 3.0, x, tolerance=0.1, **temp,
                     d_nominal=0.02, d_tolerance_fraction=0.1)
        for sample in (esr, d):
            curve = solve_campaign(campaign([sample]))["conditions"][0]["curve"]
            corrected_r, corrected_x = corrected_impedance(curve, 3.0, x, 1000)
            if sample is esr:
                self.assertGreaterEqual(corrected_r, 2.97 - 1e-6)
                self.assertLessEqual(corrected_r, 3.63 + 1e-6)
            else:
                self.assertGreaterEqual(corrected_r / -corrected_x, 0.018 - 1e-6)
                self.assertLessEqual(corrected_r / -corrected_x, 0.022 + 1e-6)
        both = {**esr, "d_nominal": 0.02, "d_tolerance_fraction": 0.1}
        curve = solve_campaign(campaign([both]))["conditions"][0]["curve"]
        corrected_r, corrected_x = corrected_impedance(curve, 3.0, x, 1000)
        self.assertGreaterEqual(corrected_r, 2.97 - 1e-6)
        self.assertLessEqual(corrected_r / -corrected_x, 0.022 + 1e-6)

    def test_capacitor_loss_nominal_needs_tolerance_and_consistency(self):
        x = -1.0 / (2.0 * math.pi * 1000 * 1e-6)
        temp = {"board_temperature_calibrated": True,
                "board_temperature_c": 25, "datasheet_temperature_c": 25}
        base = standard("C", 1e-6, 3.0, x, tolerance=0.1, **temp)
        with self.assertRaises(CampaignError):
            standard_constraints({**base, "esr_nominal_ohms": 3.0}, KEY)
        with self.assertRaises(CampaignError):
            standard_constraints({**base, "d_tolerance_fraction": 0.1}, KEY)
        with self.assertRaises(CampaignError):
            standard_constraints({**base, "d_nominal": 0.02,
                                  "d_tolerance_fraction": 0.0}, KEY)
        conflicting = {**base, "esr_nominal_ohms": 3.0,
                       "esr_tolerance_fraction": 0.01,
                       "d_nominal": 0.1, "d_tolerance_fraction": 0.01}
        with self.assertRaises(CampaignConflict):
            solve_campaign(campaign([conflicting]))

    def test_inductor_q_min_does_not_accept_negative_series_resistance(self):
        sample = standard("L", 0.1, -10.0, 2.0 * math.pi * 1000.0 * 0.1,
                          q_min=100.0, board_temperature_calibrated=True,
                          board_temperature_c=25.0, datasheet_temperature_c=25.0)
        curve = solve_campaign(campaign([sample]))["conditions"][0]["curve"]
        real, imag = corrected_impedance(curve, sample["measured_z_re_ohms"],
                                         sample["measured_z_im_ohms"], 1000)
        self.assertGreaterEqual(real, -1.0e-6)
        self.assertLessEqual(real, imag / 100.0 + 1.0e-6)

    def test_out_of_band_nominal_loss_is_not_applied(self):
        x = -1.0 / (2.0 * math.pi * 1000 * 1e-6)
        sample = standard("C", 1e-6, 3.0, x, tolerance=0.1,
                          esr_nominal_ohms=0.1, esr_tolerance_fraction=0.1,
                          esr_nominal_ohms_frequency_hz=100000)
        result = solve_campaign(campaign([sample]))["conditions"][0]
        self.assertEqual(result["curve"], list(IDENTITY))
        self.assertEqual(result["ignored_loss_specs"],
                         ["sample-1:esr_nominal_ohms:OUT_OF_BAND"])

    def test_loss_temperature_gate_is_loss_only(self):
        x = -1.0 / (2.0 * math.pi * 1000 * 1e-6)
        base = standard("C", 1e-6, 2.0, x)
        self.assertTrue(standard_constraints(base, KEY))
        with self.assertRaises(CampaignError):
            standard_constraints({**base, "esr_max_ohms": 2.0,
                                  "board_temperature_calibrated": True,
                                  "board_temperature_c": 36,
                                  "datasheet_temperature_c": 25}, KEY)

    def test_out_of_band_esr_does_not_block_in_band_capacitance(self):
        x = -1.0 / (2.0 * math.pi * 1000 * 1e-6)
        sample = standard("C", 1e-6, 2.0, x,
                          esr_max_ohms=0.1, esr_max_ohms_frequency_hz=100000)
        result = solve_campaign(campaign([sample]))["conditions"][0]
        self.assertEqual(result["curve"], list(IDENTITY))
        self.assertEqual(result["ignored_loss_specs"], ["sample-1:esr_max_ohms:OUT_OF_BAND"])

    def test_exact_frequency_and_domain_only(self):
        sample = standard("L", 0.1, 10, 628.3)
        self.assertTrue(standard_constraints(sample, KEY))
        sample["datasheet_frequency_hz"] = 100000
        with self.assertRaises(CampaignError):
            standard_constraints(sample, KEY)
        with self.assertRaises(CampaignError):
            corrected_impedance(list(IDENTITY), 100000, 0, 1000)

    def test_no_standards_preserve_prior(self):
        prior = list(IDENTITY)
        prior[0] = 1.02
        result = solve_campaign(campaign([], prior))
        self.assertEqual(result["conditions"][0]["curve"], prior)
        self.assertEqual(result["conditions"][0]["coverage"], "PRIOR_UNCHANGED")

    def test_inconsistent_zero_sensitivity_fails(self):
        with self.assertRaises(CampaignConflict):
            minimum_change_fit(list(IDENTITY), [([0.0] * 12, -0.1)])

    def test_typo_or_wrong_loss_type_cannot_be_silently_ignored(self):
        with self.assertRaises(CampaignError):
            standard_constraints(standard("C", 1e-6, 1, -159, esr_max_ohm=2), KEY)
        with self.assertRaises(CampaignError):
            standard_constraints(standard("R", 1000, 1000, 0,
                                          esr_max_ohms=2,
                                          esr_max_ohms_frequency_hz=100000), KEY)

    def test_large_current_campaign_is_bounded_and_joint(self):
        standards = []
        for index in range(200):
            measured = 900.0 + float(index % 21) * 10.0
            item = standard("R", measured, measured, 0.0, tolerance=0.01)
            item["id"] = f"sample-{index}"
            item["capture_id"] = f"capture-{index}"
            standards.append(item)
        prior = list(IDENTITY)
        prior[4] = 1.1
        result = solve_campaign(campaign(standards, prior))["conditions"][0]
        self.assertEqual(result["sample_count"], 200)
        for item in standards:
            raw = item["measured_z_re_ohms"]
            corrected, _ = corrected_impedance(result["curve"], raw, 0.0, 1000)
            self.assertLessEqual(abs(corrected - raw), raw * 0.01 + 1e-4)

    def test_raw_capture_binding_derives_measurement_and_provenance(self):
        text = raw_capture()
        sample = standard("R", 1000, 1005, 0, capture_file="capture.raw")
        for field in ("capture_safe", "capture_dsp_status", "capture_calibration_sequence",
                      "frequency_hz", "amplitude_mv_rms", "measured_z_re_ohms",
                      "measured_z_im_ohms"):
            del sample[field]
        sample["capture_id"] = hashlib.sha256(text.encode("ascii")).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "capture.raw").write_bytes(text.encode("ascii"))
            bound = bind_campaign_captures(campaign([sample]), Path(directory))
        found = bound["conditions"][0]["standards"][0]
        self.assertEqual(found["measured_z_re_ohms"], 1005.0)
        self.assertEqual(found["capture_calibration_sequence"], 7)
        self.assertEqual(solve_campaign(bound)["conditions"][0]["curve"], list(IDENTITY))

    def test_raw_capture_binding_rejects_tamper_or_claim_mismatch(self):
        text = raw_capture()
        sample = standard("R", 1000, 1005, 0, capture_file="capture.raw")
        sample["capture_id"] = hashlib.sha256(text.encode("ascii")).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "capture.raw")
            path.write_bytes(text.encode("ascii"))
            with self.assertRaises(CampaignError):
                bind_campaign_captures(campaign([sample]), Path(directory))
            sample["capture_calibration_sequence"] = 7
            self.assertEqual(len(bind_campaign_captures(campaign([sample]), Path(directory))["conditions"]), 1)
            path.write_bytes(text.replace("z_real_mohm=1005000", "z_real_mohm=1006000").encode("ascii"))
            with self.assertRaises(CampaignError):
                bind_campaign_captures(campaign([sample]), Path(directory))

    def test_mixed_osl_sequences_rejected(self):
        first = standard("R", 1000, 1000, 0)
        second = standard("R", 1000, 1000, 0)
        second.update(id="second", capture_id="second-capture", capture_calibration_sequence=2)
        with self.assertRaises(CampaignError):
            solve_campaign(campaign([first, second]))

    def test_capture_binding_rejects_unpersisted_osl_and_path_escape(self):
        text = raw_capture().replace("source=PERSISTED", "source=IDEAL")
        sample = standard("R", 1000, 1005, 0, capture_file="capture.raw",
                          capture_calibration_sequence=7)
        sample["capture_id"] = hashlib.sha256(text.encode("ascii")).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory, "captures")
            root.mkdir()
            Path(root, "capture.raw").write_bytes(text.encode("ascii"))
            with self.assertRaises(CampaignError):
                bind_campaign_captures(campaign([sample]), root)
            sample["capture_file"] = "../outside.raw"
            Path(directory, "outside.raw").write_bytes(text.encode("ascii"))
            with self.assertRaises(CampaignError):
                bind_campaign_captures(campaign([sample]), root)

    def test_capture_binding_requires_supplied_osl_sequence(self):
        text = raw_capture()
        sample = standard("R", 1000, 1005, 0, capture_file="capture.raw",
                          capture_calibration_sequence=7)
        sample["capture_id"] = hashlib.sha256(text.encode("ascii")).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "capture.raw").write_bytes(text.encode("ascii"))
            frame = OslFrameSummary(7, "frame-hash", frozenset(all_osl_conditions()))
            bound = bind_campaign_captures(campaign([sample]), Path(directory), frame)
            self.assertEqual(solve_campaign(bound)["conditions"][0]["sample_count"], 1)
            wrong = OslFrameSummary(8, "other-frame", frame.conditions)
            with self.assertRaises(CampaignError):
                bind_campaign_captures(campaign([sample]), Path(directory), wrong)

    def test_cli_requires_frame_and_binds_capture(self):
        text = raw_capture()
        sample = standard("R", 1000, 1005, 0, capture_file="capture.raw",
                          capture_calibration_sequence=7)
        sample["capture_id"] = hashlib.sha256(text.encode("ascii")).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "capture.raw").write_bytes(text.encode("ascii"))
            (root / "active.bin").write_bytes(b"frame")
            (root / "campaign.json").write_text(json.dumps(campaign([sample])), encoding="utf-8")
            args = ["calibration_campaign.py", str(root / "campaign.json"),
                    "--capture-root", str(root)]
            with patch.object(sys, "argv", args), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    calibration_campaign.main()
            self.assertEqual(error.exception.code, 2)
            args.extend(["--osl-frame", str(root / "active.bin")])
            frame = OslFrameSummary(7, "frame-hash", frozenset(all_osl_conditions()))
            output = io.StringIO()
            with patch.object(sys, "argv", args), \
                    patch.object(calibration_campaign, "decode_full_rev1_frame", return_value=frame), \
                    redirect_stdout(output):
                self.assertEqual(calibration_campaign.main(), 0)
            result = json.loads(output.getvalue())
            self.assertEqual(result["capture_binding"], "RAW_SHA256_BOUND")
            self.assertEqual(result["supplied_osl_sequence"], 7)


if __name__ == "__main__":
    unittest.main()
