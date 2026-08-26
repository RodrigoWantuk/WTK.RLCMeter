import unittest

from Firmware.tools.reference_impedance import (
    analyze_capture,
    parse_raw_dump,
    synthetic_capture,
)


class ReferenceImpedanceTests(unittest.TestCase):
    def test_synthetic_resistor_reference(self):
        capture = synthetic_capture(complex(1000.0, 0.0))
        result = analyze_capture(capture)
        self.assertAlmostEqual(result.z_1x.real, 1000.0, delta=8.0)
        self.assertAlmostEqual(result.z_1x.imag, 0.0, delta=8.0)

    def test_synthetic_capacitive_reference(self):
        capture = synthetic_capture(complex(0.0, -1000.0))
        result = analyze_capture(capture)
        self.assertAlmostEqual(result.z_1x.real, 0.0, delta=3.5)
        self.assertAlmostEqual(result.z_1x.imag, -1000.0, delta=3.5)

    def test_parse_current_phase05_raw_dump_format(self):
        capture = synthetic_capture(complex(1000.0, 1000.0))
        lines = ["METROLOGY_RAW_BEGIN v=1"]
        for key, value in capture.metadata.items():
            lines.append(f"{key}={value}")
        lines.append("index,vexc1,ret1x,vexc2,rethg,vmid_adc1,vmid_adc2")
        for row in capture.rows:
            lines.append(",".join(str(item) for item in row))
        lines.append("METROLOGY_RAW_END status=OK")

        parsed = parse_raw_dump("\n".join(lines))
        self.assertEqual(parsed.metadata["words_per_sample"], "3")
        self.assertEqual(len(parsed.rows), 256)
        result = analyze_capture(parsed)
        self.assertAlmostEqual(result.z_1x.real, 1000.0, delta=8.0)
        self.assertAlmostEqual(result.z_1x.imag, 1000.0, delta=8.0)

    def test_parse_compact_lab_raw_dump_format(self):
        capture = synthetic_capture(complex(470.0, -220.0))
        metadata = {
            "f": capture.metadata["frequency_hz"],
            "a": capture.metadata["amplitude_mvrms"],
            "r": capture.metadata["range"],
            "sr": capture.metadata["sample_rate_hz"],
            "n": capture.metadata["samples"],
            "wps": capture.metadata["words_per_sample"],
        }
        lines = ["RAW_BEGIN v=1"]
        for key, value in metadata.items():
            lines.append(f"{key}={value}")
        lines.append("i,v1,r1,v2,rh,vm1,vm2")
        for row in capture.rows:
            lines.append(",".join(str(item) for item in row))
        lines.append("RAW_END st=OK")

        parsed = parse_raw_dump("\n".join(lines))
        self.assertEqual(parsed.metadata["frequency_hz"], "1000")
        self.assertEqual(parsed.metadata["sample_rate_hz"], "64000")
        self.assertEqual(len(parsed.rows), 256)
        result = analyze_capture(parsed)
        self.assertAlmostEqual(result.z_1x.real, 470.0, delta=8.0)
        self.assertAlmostEqual(result.z_1x.imag, -220.0, delta=8.0)


if __name__ == "__main__":
    unittest.main()
