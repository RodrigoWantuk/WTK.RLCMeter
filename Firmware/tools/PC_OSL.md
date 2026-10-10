# PC OSL input and provisioning contract

The reusable backend is `pc_osl.py`; CLI is `pc_osl_calibrate.py` (Python 3.9+,
standard library only). [A06.1 evidence and commands](../../docs/review/a06/README.md)
and [A06.2 acceptance](../../docs/review/a06/A06.2-checklist.md) define its limits.

Input is UTF-8 JSON with `format: "WTK_PC_OSL_CAPTURE_V1"`, an `adc` object
(`values`: six scale/offset pairs in vexc1, ret1x, vexc2, retHG, vmid1, vmid2 order;
`provenance`: nonempty string), and `captures`: exactly 99 records. Scales are volts
per ADC count, offsets volts. Capture phasors are complex peak volts relative to GND,
after the supplied ADC corrections; the backend subtracts the mean VMID phasor.
Peak/RMS convention must match across all channels. A synthetic JSON can serve as
an executable example; physical acquisition/export is not implemented yet.

Each capture includes:

```json
{
  "condition": {"rref_ohms": 1000, "frequency_hz": 1000, "amplitude_mvrms": 100},
  "standard": "LOAD",
  "stable": true,
  "safe_capture": true,
  "temperature_mC": 25000,
  "phasors": {
    "vexc_1": [0.141421, 0], "ret_1x": [0.0707105, 0],
    "vexc_2": [0.141421, 0], "ret_hg": [1.094, 0],
    "vmid_adc1": [0, 0], "vmid_adc2": [0, 0]
  },
  "quality": {"ret_1x_valid": true, "ret_hg_valid": true, "hg_observed": true},
  "reference": {"id": "R1000_LOT_A", "impedance_ohms": [1000, 0], "tolerance_pct": 1}
}
```

`reference` is required for LOAD only. A resistance uses imaginary component zero;
passive complex impedance may include either reactance sign, but must be nonzero.
Tolerance must be finite, positive and below 100%; it remains an uncertainty statement
about the supplied center, not evidence of exactness. For a printed resistor it is
the resistance interval; complex standards additionally require physical characterization
of frequency/phase uncertainty before qualification. No propagated instrument bound is
computed. Temperature is optional and bounded to -40,000..125,000 mC.

Quality flags must reflect actual path qualification, including clipping and stability;
observed HG requires both usable paths. `stable` and `safe_capture` must be true.
They are trusted import metadata and must eventually come from real embedded evidence.
For initial provisioning, do not use ordinary uncalibrated DUT results as standards.
Sources below 1 microvolt, indistinguishable transfers (separation <=1e-5), nonfinite
or unserializable binary32 values, and incomplete conditions are rejected.

`build_candidate(document, sequence)` returns canonical frame bytes and immutable
solutions; `campaign_standards`, `solve`, `serialize`, `validate_frame` and `report`
are separately reusable. Sequence is explicit and nonzero; obtain the next sequence
from the future device interface. Deterministic sorting follows the existing Rev.1
record key order. Imported data cannot self-assert QUALIFIED. ADC provenance, LOAD
IDs/tolerances and synthetic evidence stay in the sidecar report, since unchanged
schema v2 has no new fields for them. Preserve the capture JSON/report with the frame.

`transfer_frames` produces simulator PLC1 packets; `FakeDevice` models bounded receive,
safe factory capture, inactive NOR programming, commit/readback and reboot recovery.
It has no serial transport and does not change any hardware permissions. The actual
C solver/frame/store/PLC1 codecs are tested using host-only `wtk_pc_osl_bridge`.
Its `PCOS` normalized-input bridge format is test-only, never a wire protocol.

Run `cmake --build` and CTest using the existing host presets/configuration to build
the bridge. `pc_osl_compatibility.py --bridge <path-to-wtk_pc_osl_bridge>` can also run
the same compatibility suite directly. Python unit discovery lives under `tests/tools`.
