"""Synthetic evidence only: no Rev.1 board qualification."""
import copy
from dataclasses import replace
import hashlib
import pathlib
import struct
import tempfile
import unittest
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / 'tools'))

import pc_osl as osl
from pc_osl_synthetic import Fixture, campaign
from pc_osl_provision import FakeDevice, transfer_frames


class CalibrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ideal = campaign(Fixture(ideal=True))
        cls.noisy = campaign(Fixture(noise_v=0.0002, dc_offset_v=0.025))
        cls.frame, cls.solutions = osl.build_candidate(cls.ideal, 7)

    def test_golden(self):
        self.assertEqual(hashlib.sha256(self.frame).hexdigest(),
                         'd7bb946c5c94c902c7e2cbbf9e3bcb821284e1be90b78142feca622e6b63845b')
        self.assertEqual(len(self.frame), 2760)
        self.assertEqual(len(osl.validate_frame(self.frame).conditions), 33)
        for s in self.solutions:
            self.assertAlmostEqual(s.k.real, -s.key.rref_ohms, delta=s.key.rref_ohms*1e-6)
            self.assertAlmostEqual(s.apply(0.25).real, s.key.rref_ohms/3, delta=s.key.rref_ohms*1e-6)

    def test_deterministic_order(self):
        doc = copy.deepcopy(self.ideal)
        doc['captures'].reverse()
        self.assertEqual(osl.build_candidate(doc, 7)[0], self.frame)

    def test_noisy_holdouts(self):
        frame, solutions = osl.build_candidate(self.noisy, 1)
        dev = FakeDevice()
        with self.assertRaisesRegex(ValueError, 'CALIBRATION_REQUIRED'):
            dev.standalone_measure(osl.KEYS[0], 0.2)
        for packet in transfer_frames(frame):
            dev.receive(packet)
        dev.reboot()
        fixture = Fixture(noise_v=0.0002, dc_offset_v=0.025)
        for s in solutions:
            z = s.key.rref_ohms*complex(0.37, -0.21)
            result = dev.standalone_measure(s.key, fixture.measure_transfer(s.key, z))
            self.assertLess(abs(result['z_ohms']-z)/abs(z), 0.002)
            self.assertFalse(result['qualified'])

    def test_bad_campaigns(self):
        mutations = [lambda d: d['captures'].pop(),
                     lambda d: d['captures'].append(copy.deepcopy(d['captures'][0])),
                     lambda d: d['captures'][0].update(stable=False),
                     lambda d: d['captures'][0].update(safe_capture=False),
                     lambda d: d['captures'][0]['condition'].update(amplitude_mvrms=500),
                     lambda d: d['captures'][0]['phasors'].update(ret_1x=[float('nan'), 0]),
                     lambda d: d['captures'][2]['reference'].update(tolerance_pct=0),
                     lambda d: d['captures'][2]['reference'].update(impedance_ohms=[-1, 0]),
                     lambda d: d['captures'][2]['reference'].update(impedance_ohms=[1e100, 0]),
                     lambda d: d['captures'][0]['quality'].update(hg_observed=1),
                     lambda d: d['adc']['values'].__setitem__(0, 0)]
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                doc = copy.deepcopy(self.ideal)
                mutate(doc)
                with self.assertRaises(ValueError):
                    osl.build_candidate(doc, 1)

    def test_singular_and_mismatch(self):
        group = osl.campaign_standards(self.ideal)[0]
        for delta in (0, 1e-7):
            bad = [replace(s, t_1x=complex(delta), t_hg_raw=complex(delta)) for s in group]
            with self.assertRaises(ValueError):
                osl.solve(bad)
        with self.assertRaises(ValueError):
            osl.solve([group[0], replace(group[1], key=osl.KEYS[1]), group[2]])
        with self.assertRaises(ValueError):
            self.solutions[0].apply(self.solutions[0].opened)

    def test_effective_hg_fallback(self):
        group = osl.campaign_standards(self.ideal)[0]
        # LOAD supplies overlap; OPEN loses 1X, forcing normalized HG fit.
        hg = complex(14.8, 0.2)
        group = [replace(s, t_hg_raw=s.t_1x*hg, ret_hg_valid=True,
                         hg_observed=s.standard == 'LOAD',
                         ret_1x_valid=s.standard != 'OPEN') for s in group]
        solution = osl.solve(group)
        self.assertEqual(solution.fit_channel, 'HG')
        self.assertLess(abs(solution.hg-hg), 1e-5)
        self.assertLess(abs(solution.apply(0.5)-group[2].load), 1e-4)
        with self.assertRaises(ValueError):
            osl.solve([replace(s, hg_observed=False) for s in group])

    def test_sequence_crc_and_trailing(self):
        for seq in (0, -1, 0x100000000, True):
            with self.assertRaises(ValueError):
                osl.build_candidate(self.ideal, seq)
        for offset in (0, 56, 60, 120, 2759):
            bad = bytearray(self.frame)
            bad[offset] ^= 1
            with self.assertRaises(ValueError):
                osl.validate_frame(bytes(bad))
        with self.assertRaises(ValueError):
            osl.validate_frame(self.frame+b'\x00')

    def test_duplicate_json_fields(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory)/'bad.json'
            path.write_text('{"stable":true,"stable":false}', encoding='utf-8')
            with self.assertRaises(ValueError):
                osl.load_document(path)

    def test_tolerance_is_not_truth(self):
        key = osl.KEYS[0]
        fixture = Fixture(ideal=True)
        captures = [fixture.acquire(key, name, z) for name, z in
                    [('OPEN', None), ('SHORT', 0j), ('LOAD', key.rref_ohms*1.005)]]
        captures[2]['reference']['impedance_ohms'] = [key.rref_ohms, 0]
        solution = osl.solve([osl.standard_from_capture(c) for c in captures])
        actual = key.rref_ohms*0.4
        corrected = solution.apply(fixture.transfer(key, actual))
        self.assertGreater(abs(corrected-actual)/actual, 0.004)
        report = osl.report(self.ideal, self.frame, self.solutions)
        self.assertIn('not certified true values', report)
        self.assertIn('REQUIRES_BENCH_VALIDATION', report)

    def test_complex_known_load(self):
        fixture, key = Fixture(ideal=True), osl.KEYS[0]
        for known in (complex(0, -key.rref_ohms), complex(key.rref_ohms, key.rref_ohms*0.3)):
            captures = [fixture.acquire(key, name, z) for name, z in
                        [('OPEN', None), ('SHORT', 0j), ('LOAD', known)]]
            solution = osl.solve([osl.standard_from_capture(c) for c in captures])
            self.assertLess(abs(solution.apply(fixture.transfer(key, known))-known), abs(known)*1e-6)


class ProvisionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        document = campaign(Fixture(ideal=True))
        cls.old = osl.build_candidate(document, 1)[0]
        cls.new = osl.build_candidate(document, 2)[0]

    def device(self):
        dev = FakeDevice()
        dev.install(self.old)
        dev.reboot()
        return dev

    def test_every_transfer_interruption(self):
        packets = transfer_frames(self.new)
        for cut in range(len(packets)):
            dev = self.device()
            for packet in packets[:cut]:
                dev.receive(packet)
            dev.reboot()
            self.assertEqual(dev.active_frame, self.old)

    def test_every_byte_program_interruption(self):
        for cut in range(len(self.new)):
            dev = self.device()
            with self.assertRaises(InterruptedError):
                dev.install(self.new, cut)
            self.assertEqual(dev.active_frame, self.old)
            dev.reboot()
            self.assertEqual(dev.active_frame, self.old, f'cut={cut}')

    def test_blank_interruption_and_success(self):
        dev = FakeDevice()
        with self.assertRaises(InterruptedError):
            dev.install(self.old, 2759)
        dev.reboot()
        self.assertIsNone(dev.active_frame)
        dev.install(self.old)
        dev.install(self.new)
        dev.reboot()
        self.assertEqual(dev.active_frame, self.new)
        dev.slots[dev.active_slot][125] ^= 1
        dev.reboot()
        self.assertEqual(dev.active_frame, self.old)

    def test_invalid_candidate_preserves_flash(self):
        dev = self.device()
        before = [bytes(s) for s in dev.slots]
        corrupt = bytearray(self.new)
        corrupt[200] ^= 1
        for candidate in (bytes(corrupt), self.old, self.new[:-1]):
            with self.assertRaises(ValueError):
                dev.install(candidate)
            self.assertEqual([bytes(s) for s in dev.slots], before)

    def test_crc_valid_but_semantically_invalid_candidate(self):
        dev = self.device()
        before = [bytes(s) for s in dev.slots]
        for mutation in ('duplicate', 'qualified', 'zero_sequence', 'skipped_sequence'):
            bad = bytearray(self.new)
            if mutation == 'duplicate':
                bad[200:280] = bad[120:200]
            elif mutation == 'qualified':
                flags = struct.unpack_from('<I', bad, 138)[0]
                struct.pack_into('<I', bad, 138, flags | 256)
            else:
                struct.pack_into('<I', bad, 12, 0 if mutation == 'zero_sequence' else 3)
            struct.pack_into('<I', bad, 56, osl.wire.crc_frame(bad, 2696))
            with self.assertRaises(ValueError):
                dev.install(bytes(bad))
            self.assertEqual([bytes(s) for s in dev.slots], before)

    def test_permissions_and_protocol(self):
        packets = transfer_frames(self.new)
        for field, value in [('safe', False), ('charger', True), ('residual_safe', False)]:
            dev = self.device()
            setattr(dev, field, value)
            for operation in (lambda: dev.receive(packets[0]), lambda: dev.install(self.new),
                              lambda: dev.capture_standard(Fixture(), osl.KEYS[0], 'OPEN')):
                with self.assertRaises(ValueError):
                    operation()
            self.assertEqual(dev.active_frame, self.old)
        for offset in (6, 8, 12, 16):
            dev = self.device()
            bad = bytearray(packets[0])
            bad[offset] ^= 1
            with self.assertRaises(ValueError):
                dev.receive(bytes(bad))
        dev = self.device()
        dev.receive(packets[0])
        with self.assertRaises(ValueError):
            dev.receive(packets[2])
        with self.assertRaises(ValueError):
            dev.receive(packets[-1])


if __name__ == '__main__':
    unittest.main()
