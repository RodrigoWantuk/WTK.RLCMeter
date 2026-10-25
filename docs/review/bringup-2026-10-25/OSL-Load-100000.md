# 100-kohm LOAD capture and unresolved fit — October 25, 2026

Status: SIX_CAPTURES_SAVED; FIVE_FITS_ACCEPTED; ONE_FIT_BLOCKED; NOT_QUALIFIED.

The owner reported 99.8 kohm on the requested Minipa ET-1100A 200-kohm range
and confirmed attachment alone. Nominal 100 kohm +/-1% remains the uncertain
fit anchor, with DMM digits retained as consistency evidence.

All six LOAD observations are preserved with six stable repeats, matching
source-7017e5e0 identity, nominal ADC provenance, CRC/SHA and SAFE teardown.
Five triplets passed strict OSL/float32 fit checks. The sixth, 100 kohm / 10 kHz /
100 mVrms, is blocked: OPEN and LOAD have only HG valid, SHORT has both paths,
and none has actual observed HG overlap. The strict Python and C solvers require
observed gain to select HG. A finite ratio inferred from invalid 1X data is not
a substitute; no flag, threshold, gain or solver rule was changed.

The first sweep stopped after saving the fifth raw frame and the solver rejection.
The previously unattempted 10-kHz/500-mVrms condition was then captured separately
under the unchanged owner-confirmed fixture and passed using HG with actual
same-key overlap. The rejected condition was not retried. Both reports and wire
logs remain intact. Protocol counter stayed at 1; safety fault mask stayed zero.

There are 93/99 original captures, 27 complete triplets, 26 solvable fits and one
blocked fit. Six 1-Mohm LOAD observations remain absent. These numbers do not
constitute a full usable candidate or an independent accuracy claim.

Fresh battery before this group was 3.70696 V; before requesting the 1-Mohm
reference it was 3.70374 V, both OK. Snapshots were taken only SAFE/idle without
reset. The owner requires a fresh battery check at every new resistor value.
Calibration storage remains blank. No candidate was generated or installed.
Independent holdouts, scope-dependent qualification and the blocked key remain
pending; do not mark the missing calibration condition qualified or complete.

See [structured path/fit audit](OSL-Load-100000.json),
[physical evidence](../../../Firmware/releases/2026-10-25/osl-load-100000-checkpoint/README.md)
and [previous checkpoint](OSL-Load-10000.md).
