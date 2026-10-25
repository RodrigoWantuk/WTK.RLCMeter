"""Numerical/provenance tests; no physical accuracy or qualification claim."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import ntc_factory_profile as n

PROFILE = Path(__file__).resolve().parents[2] / "config/units/54ff70068370525760380567-ntc-engineering.json"


class FactoryProfileTests(unittest.TestCase):
    def setUp(self):
        self.profile = json.loads(PROFILE.read_text())

    def test_float32_lut_and_binary_binding(self):
        meta = n.compile_profile(self.profile)
        table = bytes.fromhex(meta["lut_le_hex"])
        rows = [struct.unpack_from("<2f", table, i*8) for i in range(21)]
        self.assertTrue(all(a[0] < b[0] and a[1] > b[1] for a,b in zip(rows,rows[1:])))
        for point in self.profile["points"]:
            row = min(rows, key=lambda row: abs(row[0]-point["temperature_c"]))
            self.assertAlmostEqual(row[0], point["temperature_c"], places=4)
            self.assertAlmostEqual(row[1]/100000, point["adc_raw"]/(4095-point["adc_raw"]), places=6)
        flags = dict(WTK_NTC_FACTORY_CALIBRATION="1u", WTK_NTC_FACTORY_UID=meta["uid"],
                     WTK_NTC_PROFILE_SHA256=meta["profile_sha256"])
        n.validate_binary(meta, b"prefix"+table+b"suffix", flags)
        for binary in (b"missing table", table+table):
            with self.assertRaises(ValueError): n.validate_binary(meta,binary,flags)
        bad = copy.deepcopy(meta); bad["effective_beta_k"] += 1
        with self.assertRaises(ValueError): n.validate_binary(bad,table,flags)

    def test_wrong_unit_and_missing_uid_are_rejected(self):
        meta = n.compile_profile(self.profile)
        n.require_target(meta,meta["uid"])
        for uid in ("", "0"*24):
            with self.assertRaises(ValueError): n.require_target(meta,uid)
        n.require_target(None,"")

    def test_invalid_reference_and_false_qualification(self):
        mutations = [lambda p:p.update(qualified=True), lambda p:p.update(owner_authorized=False),
                     lambda p:p.update(uid="bad"), lambda p:p["points"][0].update(adc_raw=True),
                     lambda p:p["points"][0].update(temperature_c=float("nan")),
                     lambda p:p["points"][1].update(temperature_c=24),
                     lambda p:p["points"][1].update(adc_raw=2000)]
        for mutate in mutations:
            profile=copy.deepcopy(self.profile); mutate(profile)
            with self.assertRaises(ValueError): n.compile_profile(profile)

    def test_default_build_cannot_hide_factory_metadata(self):
        n.validate_binary(None,b"",{})
        with self.assertRaises(ValueError): n.validate_binary(n.compile_profile(self.profile),b"",{})
        with self.assertRaises(ValueError): n.validate_binary(None,b"",dict(WTK_NTC_FACTORY_CALIBRATION="2u"))

    def test_generator_and_duplicate_key_rejection(self):
        with tempfile.TemporaryDirectory() as folder:
            folder=Path(folder)
            n.generate(PROFILE,folder/"table.h",folder/"meta.json")
            self.assertEqual(json.loads((folder/"meta.json").read_text()),n.compile_profile(self.profile))
            self.assertIn("static const ntc_lut_point_t",(folder/"table.h").read_text())
            (folder/"bad.json").write_text('{"format":1,"format":2}')
            with self.assertRaises(ValueError): n.generate(folder/"bad.json",folder/"h",folder/"m")


if __name__ == "__main__": unittest.main()
