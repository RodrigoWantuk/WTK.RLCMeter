# 1-kohm LOAD checkpoint — October 25, 2026

Status: LOAD_1000_COMPLETE; THREE_REFERENCE_GROUPS_PENDING; NOT_QUALIFIED.

The owner repeated the Minipa ET-1100A reading of 992 ohm on the requested
2000-ohm range and confirmed direct attachment alone across TEST_HI/TEST_LO.
Nominal 1000 ohm +/-1% remains the uncertain fit anchor; the DMM is consistency
evidence. The earlier band direction remains inferred, without an independent
certificate or invented measured interval.

All six 1-kohm LOAD conditions (10 Hz / 1 kHz / 10 kHz, 100 / 500 mVrms)
passed six stable repeats, SAFE teardown and strict same-image OSL/float32 fit
checks. HG was clipped and rejected at 500 mVrms; 1X remained valid. The three
100-mVrms captures had both paths valid and no clipping. All six fits used 1X.
The cumulative 81/99 original observations contain 15/33 complete triplets,
all rechecked by the strict solver. This verifies fit consistency, not independent
accuracy. Source-7017e5e0 identity, CRC/SHA and original CCO1 metadata match.

Battery before this group: 3.71502 V; before requesting the next 10-kohm
reference: 3.71179 V, both OK. These fresh SAFE/idle nominal-ADC snapshots
do not establish charge percentage or remaining runtime. The owner's per-value
battery check remains mandatory. Protocol counter stayed at
1; no new errors or faults occurred.
Calibration storage remains blank. No complete candidate was generated or installed.
Three LOAD groups, independent holdouts and scope-dependent qualification remain
pending; final test leads require calibration in their final geometry.

See [structured checkpoint](OSL-Load-1000.json),
[physical evidence](../../../Firmware/releases/2026-10-25/osl-load-1000-checkpoint/README.md)
and [previous checkpoint](OSL-Load-100.md).
