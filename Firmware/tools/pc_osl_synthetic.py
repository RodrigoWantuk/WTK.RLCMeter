"""Deterministic electrical fixture and sampled acquisition, SYNTHETIC ONLY."""

from __future__ import annotations

import cmath
import math
import random

from pc_osl import CHANNELS, KEYS, Key, pair, complex_value
from reference_impedance import ADC_SCALE_V, HG_NOMINAL, _extract


class Fixture:
    """Series impedance + shunt admittance + complex channel gain, not OSL coefficients."""

    def __init__(self, ideal=False, noise_v=0.0, dc_offset_v=0.0, seed=4061):
        if not math.isfinite(noise_v) or noise_v < 0 or not math.isfinite(dc_offset_v):
            raise ValueError("invalid synthetic noise/offset")
        self.ideal, self.noise_v, self.dc_offset_v, self.seed = ideal, noise_v, dc_offset_v, seed

    def transfer(self, key, z):
        r = key.rref_ohms
        series = 0j if self.ideal else r*complex(0.007, 0.002)
        shunt = 0j if self.ideal else complex(0.0003, 0.00008)/r
        gain = 1+0j if self.ideal else cmath.rect(1.018, 0.012)
        if z is None:  # OPEN, including the defined fixture leakage.
            if shunt == 0j:
                return gain
            parallel = 1/shunt
        elif z == 0j:
            parallel = 0j
        else:
            parallel = 1/(1/z+shunt)
        branch = series+parallel
        return gain*branch/(r+branch)

    def acquire(self, key: Key, standard, z=None):
        t = self.transfer(key, z)
        vs = key.amplitude_mvrms/1000*math.sqrt(2)
        source2 = 1+0j if self.ideal else cmath.rect(0.991, -0.006)
        hg_gain = HG_NOMINAL if self.ideal else cmath.rect(15.31, 0.024)
        phasors = (complex(vs), t*vs, source2*vs, t*vs*hg_gain, 0j, 0j)
        spc = 16 if key.frequency_hz == 10000 else 64
        index = KEYS.index(key)
        standard_id = ("OPEN", "SHORT", "LOAD", "DUT").index(standard)
        rng = random.Random(self.seed+index*17+standard_id)
        clipped = []
        observed = {}
        for i, (name, phasor) in enumerate(zip(CHANNELS, phasors)):
            samples = []
            clipping = False
            for n in range(256):
                theta = 2*math.pi*(n % spc)/spc
                # These are simulated voltage samples, not hardware ADC qualification.
                value = 1.65+self.dc_offset_v*(i+1)/6+(phasor*cmath.exp(1j*theta)).real
                value += rng.uniform(-self.noise_v, self.noise_v)
                clipping |= not 0.01 < value < 3.29
                samples.append(max(0.0, min(3.3, value)))
            observed[name] = pair(_extract(samples, spc))
            clipped.append(clipping)
        valid1, validh = not clipped[1], not clipped[3]
        item = {"condition": key.as_json(), "standard": standard,
                "stable": True, "safe_capture": True, "temperature_mC": 25000,
                "phasors": observed,
                "quality": {"ret_1x_valid": valid1, "ret_hg_valid": validh,
                            "hg_observed": valid1 and validh and abs(t*vs)>1.0e-5},
                "evidence": "SYNTHETIC_NOT_PHYSICALLY_QUALIFIED"}
        if standard == "LOAD":
            item["reference"] = {"id": f"SYNTHETIC_LOAD_{key.rref_ohms}",
                                 "impedance_ohms": pair(z), "tolerance_pct": 1.0}
        return item

    def measure_transfer(self, key, z):
        capture = self.acquire(key, 'DUT', z)
        if not capture['quality']['ret_1x_valid']:
            raise ValueError('synthetic DUT clips the 1X acquisition path')
        phasors = {name: complex_value(value) for name, value in capture['phasors'].items()}
        vmid = (phasors['vmid_adc1']+phasors['vmid_adc2'])*0.5
        return (phasors['ret_1x']-vmid)/(phasors['vexc_1']-vmid)


def campaign(fixture=None, device=None):
    fixture = fixture or Fixture()
    captures = []
    for key in KEYS:
        for standard, z in (("OPEN", None), ("SHORT", 0j), ("LOAD", complex(key.rref_ohms))):
            captures.append(device.capture_standard(fixture, key, standard, z) if device
                            else fixture.acquire(key, standard, z))
    return {"format": "WTK_PC_OSL_CAPTURE_V1", "evidence": "SYNTHETIC_NOT_PHYSICALLY_QUALIFIED",
            "adc": {"values": [ADC_SCALE_V, 0.0]*6, "provenance": "SYNTHETIC_IDEAL_ADC_SCALE"},
            "fixture_model": {"series_z_over_rref": [0.007, 0.002] if not fixture.ideal else [0, 0],
                              "shunt_y_times_rref": [0.0003, 0.00008] if not fixture.ideal else [0, 0],
                              "noise_v": fixture.noise_v, "dc_offset_v": fixture.dc_offset_v,
                              "sample_count": 256, "seed": fixture.seed},
            "captures": captures}
