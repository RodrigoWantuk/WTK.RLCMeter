"""Synthetic tests for the host-only interval calibration prototype."""

from __future__ import annotations

import math
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from calibration_campaign import (  # noqa: E402
    CampaignConflict,
    CampaignError,
    IDENTITY,
    all_osl_conditions,
    campaign_template,
    corrected_impedance,
    minimum_change_fit,
    solve_campaign,
    standard_constraints,
)


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
        self.assertEqual(result["qualification"], "UNQUALIFIED")

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
        with self.assertRaises(CampaignConflict):
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


if __name__ == "__main__":
    unittest.main()
