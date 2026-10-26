# Bounded low-impedance automatic search

STATUS: IN_PROGRESS

## Trigger and prerequisites

The owner requests additional automatic conditions when useful. The nominal
0.47-uH bench check completed after one resistive 1-kHz/10-ohm attempt:
R=0.235328 ohm and X=-0.0100988 ohm. No valid L was published. See the
[original evidence](../Firmware/releases/2026-10-25/inductor-047-limit-check/README.md).
Phase 07's bounded policy, plan 20's safety orchestration and plan 22's reactive
publication are already implemented. Active engineering OSL sequence 1 has 33
records. This task does not repeat or replace the resistor calibration campaign.

## Scope and explicit policy

Keep existing ratio-based adjacent-range selection first: reduce RREF for small
DUT/reference ratio, increase it for large ratio or clipping. An additional
RREF is useful only when it improves the existing quality criteria; do not
exhaustively sweep every range or raise current at the lowest RREF.

After those quality steps, an apparently resistive, finite
result at 1 kHz on the 10-ohm or 100-ohm reference may request one 10-kHz check
at the same RREF and amplitude. The selected RET path must be usable and
unclipped. Estimated R may have either sign; existing publication guards still
reject unsupported negative resistance/loss. This targets low-impedance components whose reactance can be hidden
by winding/contact resistance at the first frequency, including zero measured
reactance. It is an exploration heuristic, not a physical accuracy threshold.

Use the existing CHECK_SECOND_FREQUENCY reason, condition support matrix and
attempt history. Keep six attempts, four range transitions, one frequency
refinement, no repeated conditions, forbidden 10-ohm/500-mV combination, and
all safety/cancellation/OPEN/SHORT/invalid-data priorities. Each acquisition
continues through the existing fresh-permit SAFE hardware transaction.

Do not lower R/C/L classification thresholds, invent phase compensation, promote
a secondary value silently, or convert contradictory evidence into an inductance.
Both results remain available; disagreement may remain MIXED/UNKNOWN. Existing
primary selection and publication gates stay intact. Higher impedances and
already-10-kHz resistive hints do not pay for this extra check.

## Expected changes and acceptance

- measurement_engine.c: pure bounded exploration predicate and frequency gate.
- test_measurement_engine.c: replay the observed 0.47-uH result, real resistor
  sequences, uncertain/reactive secondary, exclusions, terminal priorities,
  duplicate avoidance and attempt budget.
- Measurement/calibration docs and plan index: precise trigger, costs and limits.
- Relevant standard/factory host suites and all nine strict UID-bound embedded
  profiles; inspect image size and preserve the unit NTC profile.
- Verify/program PRODUCT with calibration/resources preserved, then request one
  owner-triggered Click on the same fixture and preserve actual attempts/SAFE
  teardown. If owner action is pending, say so rather than claim bench success.

No BSP, hardware, ADC/DMA, calibration model/coefficients or UI behavior is
changed. Sub-microhenry sensitivity and accuracy remain
REQUIRES_BENCH_VALIDATION regardless of software test results.

## Software checks and relay-session follow-up

Standard host CTest 46/46 and factory PRODUCT 47/47 pass, including the captured
low-Z result, resistive secondary, contradictory inductive secondary, exclusions
and a six-attempt/four-range/one-frequency sequence without repeated conditions.
The first CTest invocation omitted the multi-configuration Debug selection and
ran no tests; the corrected runs above use `ctest -C Debug`. A new regression's
initial primary-index expectation also mixed HG/1X quality; matching both
conditions' quality correctly tests the existing primary-selection policy.

The owner also asks whether K1 can remain energized through a complete search.
Current hw_metrology_measure owns a single-use permit per capture; auxiliary
residual evidence is invalidated on ADC ownership and requalified after SAFE.
Holding K1 across DSP/rerange would need an explicit bounded session ownership
contract, intentional range-transition handling, sensor-age/ADC supervision,
neutral excitation sequencing and tested emergency cleanup. Existing fault
policy treats invalid/indeterminate ranges as blockers. Do not skip the current
cleanup or reuse consumed permits as a shortcut. This optimization is a separate
safety architecture task and is not implemented by the search predicate change.

The initial added R>=0 exploration filter was removed during size review: a
finite negative estimated R also warrants a diagnostic frequency check, while
publication of unsupported negative R/loss remains blocked by the unchanged
PRODUCT guards. Searching and publishing are distinct. This also avoids an
extra soft-float comparison in the nearly full BRINGUP image. The exploratory
predicate never bypasses finite-DSP, selected-path, terminal or safety gates.
Equivalent compact frequency selection retains all three supported mappings.
A BRINGUP size check now fits at 65,528 bytes; final committed matrix is pending.
