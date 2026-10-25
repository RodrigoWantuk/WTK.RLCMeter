# 10-kohm LOAD checkpoint — October 25, 2026

Status: LOAD_10000_COMPLETE; TWO_REFERENCE_GROUPS_PENDING; NOT_QUALIFIED.

The owner repeated 9.99 kohm on the requested Minipa ET-1100A 20-kohm range
and confirmed direct attachment alone across TEST_HI/TEST_LO. The owner-reported
brown/black/black/red/brown bands identify nominal 10 kohm +/-1%, retained as an
uncertain fit anchor. DMM digits are consistency evidence, without a certified
actual value, fixed lead subtraction or invented uncertainty interval.

All six 10-kohm LOAD conditions passed six stable repeats, SAFE teardown and
strict same-image OSL/float32 fit checks. Accepted and rejected paths remain
explicit; no clipped channel was promoted to usable:

| Hz | mVrms | Usable paths | Clipping mask | Fit |
| ---: | ---: | --- | ---: | --- |
| 10 | 100 | 1X, HG | 0 | 1X |
| 10 | 500 | 1X | 8 | 1X |
| 1000 | 100 | 1X, HG | 0 | 1X |
| 1000 | 500 | 1X | 8 | 1X |
| 10000 | 100 | 1X, HG | 0 | 1X |
| 10000 | 500 | 1X | 8 | 1X |

The cumulative 87/99 original observations contain 21/33 complete triplets,
all rechecked by the strict solver. This verifies fit consistency, not independent
accuracy. Exact source-7017e5e0 identity and nominal ADC provenance match.

Battery before this group: 3.70857 V; before requesting the next 100-kohm
reference: 3.70696 V, both OK. Fresh SAFE/idle readings use nominal ADC scale
and do not establish charge percentage or runtime. Protocol counter stayed at
1; no new errors or faults occurred. Calibration
storage remains blank. No complete candidate was generated or installed.
Two LOAD groups, independent holdouts and scope-dependent qualification remain
pending. Final test leads require calibration in their final geometry.

See [structured checkpoint](OSL-Load-10000.json),
[physical evidence](../../../Firmware/releases/2026-10-25/osl-load-10000-checkpoint/README.md)
and [previous checkpoint](OSL-Load-1000.md).
