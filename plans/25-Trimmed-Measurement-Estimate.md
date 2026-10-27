# Seven-sample trimmed complex estimate

STATUS: IN_PROGRESS

The owner requests seven readings, discarding the two highest and two lowest
and averaging the remaining three for measurement quantities. Observed speed
is acceptable. This follows plan 24 on main; its relay/session ownership and
qualification gates remain mandatory. The owner has now observed a single K1
engagement/release across four captures and two valid inductive results, but
independent metrology and UI qualification remain pending.

## Statistical contract

Use seven valid calibrated complex impedances at the selected primary condition
(same RREF, frequency, amplitude, RET channel and calibration provenance). Seed
with the original primary capture, then collect six additional valid windows.
Sort R and X independently, discard two values at each end of each coordinate,
and average the three middle coordinates to form one complex estimate. Preserve
all seven points and the separate retained-index masks for R and X: the masks
may differ, so do not claim they are three complete retained raw captures.

Derive magnitude, wrapped phase, C/L and conditional ESR/Q/D from that single
complex estimate. Never average wrapped phase angles or independently smooth
inconsistent derived quantities. Raw autorange/frequency history remains intact;
the final primary explicitly declares that it is an aggregate. Genuine contrary
classification evidence remains inconclusive. No confidence/accuracy class is
upgraded merely because an average exists. This reduces sensitivity to spikes;
it does not correct systematic error or qualify sub-microhenry capability.

Hardware failures/cancellation revoke the session immediately. Invalid/clipped/
weak/nonfinite or mismatched-path/calibration windows cannot enter the sample
set. Allow at most eight extra capture attempts (two rejected extras), requiring
all seven valid points for publication. Incomplete aggregation is explicit and
does not silently fall back to an unfiltered value.

## Bounded hardware contract

Enable only in PC-provisioned PRODUCT. Keep the six-condition autorange budget.
The hardware owner distinguishes extra statistical windows from search captures:
at most six search captures plus eight extra windows (14 total). Once statistical
sampling starts, its condition is locked; no return to range exploration or
condition changes is allowed. The first statistical continuation may return from
the secondary frequency to the primary condition. Subsequent identical windows
skip redundant range addressing, retaining neutral/excitation settling and the
existing ADC/DMA configuration. K1 remains under the same consumed initial
authorization; no additional relay activation or forged residual evidence.

The 500-ms cooperative connected deadline and original 1,000-ms battery-evidence
expiry remain unchanged. Count expansion is explicit for this new bounded role,
not removal of a safety limit. If the current budget cannot complete the sample
set, report failure/instability and SAFE cleanup; record physical timing before
considering a separately justified deadline change. Fixed diagnostics/calibration
and legacy PRODUCT keep their existing behavior and persistence formats.

## Implementation and acceptance

Expected changes: pure fixed-size trimmed estimator and tests; factory session
collection/publication and tests; explicitly counted/locked statistical hardware
continuations and failure tests; filter metadata in final/result details; CMake
profile flag and docs. No heap, seven raw buffers, new settings/calibration record
format, calibration coefficient change or new automatic boot measurement.

Test deterministic trimming (signed X, outliers, ties, nonfinite, phase wrapping),
same-key/path admission, bounded rejection, unchanged raw history, cleanup faults,
cancellation, hardware lock/count/time limits and all existing host regressions.
Build all nine strict UID-bound profiles, inspect RAM/Flash and preserve the
unit NTC/OSL/assets. Program/readback-check factory PRODUCT, verify SAFE boot,
then request one owner-triggered measurement and preserve actual aggregation
samples, masks/counts/condition and SAFE cleanup. No new resistor campaign.

Before touching another fixture, finish/preserve the already observed inductor
sessions 3/4 (467.0 and 468.8 uH; nominal part unconfirmed) and the owner's
single-relay-cycle confirmation. Branch codex/trimmed-measurements starts at
the merged main rather than reuse the closed PR 24 branch.
