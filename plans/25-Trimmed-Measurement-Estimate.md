# Seven-sample trimmed complex estimate

STATUS: IMPLEMENTED_DEPLOYED_BASIC_BENCH_OBSERVED; REQUIRES_BENCH_VALIDATION (accuracy/transients/UI)

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

## First physical integration correction

The first deployed filter image (source 7903edf) reached the primary result but
rejected its seed: it incorrectly interpreted provenance.uncalibrated as missing
applied coefficients. The existing engineering resolver deliberately sets that
flag for persisted UNQUALIFIED records whose physical accuracy is not qualified.
The first owner-triggered session therefore made four search captures, zero
statistical samples and a rejected FAILED result, then completed SAFE cleanup.
No seven-sample result is claimed for that run. The correction admits persisted
FOUND or UNQUALIFIED records with matching status/flag/set/model/condition, while
keeping their original confidence and refusing ideal/missing/corrupt records.
Regression tests now exercise both qualified and actual engineering provenance.
No calibration coefficient, qualification flag or persisted record was changed.

## Delivery and observed basic check

Final source 1b25fe38dd05a704e90d8635e2fd3cf92a3278df passes 48 factory host tests,
47 standard host tests, focused final guards and all nine strict UID-bound
embedded profiles. Factory PRODUCT uses 57,964 B Flash and 16,136 B accounted
RAM. Internal programming/readback matches SHA-256
a6d1ac882dcda35111e09e5d82ceccb4738677345c937050cbb0f50ed20b1a38.
Controlled boot is READY/SAFE with OSL sequence 1/all 33 records and the
experimental NTC profile preserved; no external-Flash write or SWD battery
voltage read was requested.

An owner-triggered short OK on the same passive inductor completed four search
conditions and six additional statistical windows: seven valid same-primary
samples, no rejected extras, masks R=69/X=74, aggregate 7.412449 + j2.907871 ohms
at 1 kHz/RREF10/HG and published 462.802 uH. An independent replay of the seven
stored points reproduces the masks/mean/L. Final metadata is hardware IDLE,
K1 SAFE, ranges/excitation OFF, quiet/workspace released, no hardware faults and
unchanged active calibration. Connected duration reconstructed from the firmware
K1/release deadlines is 386 ms, below the unchanged 500-ms budget. This is a
software-tick observation, not scope qualification of physical relay latency.
The owner reported that the corrected measurement appeared to work. No explicit
new relay-count witness was given for this run; the prior four-capture session
has the explicit single-cycle witness preserved separately.

No resistor recapture campaign was required. Physical capacitor filtering,
independent accuracy, phase/loss, switching/abort timing, sub-uH capability and
AVG7/3 display appearance remain REQUIRES_BENCH_VALIDATION. See the release
artifacts under Firmware/releases/2026-10-26/trimmed-measurements/.
