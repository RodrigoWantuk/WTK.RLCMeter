# Seven-sample trimmed complex measurements

Source: 1b25fe38dd05a704e90d8635e2fd3cf92a3278df. Unit: 54ff70068370525760380567.
Factory PRODUCT is implemented, deployed and observed completing seven valid
samples on the owner-triggered inductor check. Absolute accuracy, phase/loss,
transients/abort timing and physical display appearance remain
REQUIRES_BENCH_VALIDATION. No new resistor calibration campaign was required.

## Behavior

After autorange selects the primary condition, its original calibrated complex
capture seeds six additional valid coherent windows at the same RREF, frequency,
amplitude, selected RET path and calibration provenance. For R and signed X
separately, sort seven values, discard two at each end and average the middle
three. Preserve all seven points and separate retained-index masks. Recompute
magnitude/phase/C/L/conditional Q/D from the single aggregate; never average
wrapped phase angles or independent derived values. Raw search history remains
unchanged. Details mark completed estimates AVG7/3.

The hardware owner retains K1/neutral/quiet/ADC/workspace ownership, bounding
search to six captures and statistical extras to eight (at most two rejected
windows), requiring all seven valid points. The first extra can return from the
secondary frequency to primary; later extras are condition-locked and cannot
resume search. Same-condition repeats skip redundant range addressing. Initial
fresh authorization, live charger/fault/ownership checks, the original 500-ms
connected deadline, original battery-evidence expiry and SAFE-before-publication
remain mandatory. Incomplete or faulted estimates are rejected, with no silent
single-reading fallback. Fixed/legacy/calibration capture behavior is unchanged.
Safety/residual/battery monitoring does not use this statistical delay.

Persisted engineering UNQUALIFIED calibration is accepted with matching
status/uncalibrated flag/set/model/condition, preserving its existing confidence.
The first deployed integration incorrectly treated that flag as missing applied
coefficients and failed closed after four search captures with zero statistical
samples. The corrected image and regression cover actual engineering provenance;
no coefficients or qualification flags were altered. Both physical runs are
preserved in validation-evidence.zip, including the failure rather than claiming
it completed a mean.

## Actual basic board check

The same isolated passive inductor was used (suspected nominal 470 uH; marking
unconfirmed). One owner-triggered short OK completed four search conditions plus
six statistical windows: seven valid primary samples, zero rejected extras,
R retained mask 69, X mask 74. Independent replay of saved points reproduces
7.412449 + j2.907871 ohms at 1 kHz/RREF10/HG and published **462.802 uH**.
The seven points retain different coordinate subsets, as designed.

Successful connected duration reconstructed from firmware deadlines is **386 ms**:
(last release-guard deadline - 8 ms) - (connected deadline - 500 ms), modulo
uint32. This is a software-clock observation, not a scope measurement of relay
contacts. Final hardware metadata confirms IDLE, K1 commanded SAFE, range and
excitation OFF, quiet/workspace released and no fault. The owner reported the
corrected measurement appeared to work. The earlier four-capture session has an
explicit single-relay-cycle witness in the separate session/menu follow-up;
no new explicit relay-count witness is invented for this ten-capture run.

Complete internal-Flash readback matches image SHA-256
a6d1ac882dcda35111e09e5d82ceccb4738677345c937050cbb0f50ed20b1a38
(57,964 B). Boot produced one passive TTL SAFE_BOOT and READY/SAFE, with OSL
sequence 1/all 33 records active, assets/settings and the UID-bound experimental
NTC profile preserved. No external-Flash write or battery voltage read via SWD
was requested. The read-only result checks do not reset, request acquisition or
inject application state. Owner OK initiated the measurements.

## Validation and memory

Standard host CTest **47/47**, factory **48/48**, and focused final guards **4/4**
pass. Tests cover signed-coordinate tails, stable ties, independent masks,
nonfinite/bounded input, wrapped phase, same-path/calibration admission, qualified
and actual engineering provenance, bounded rejection, unchanged raw search
history, second-frequency return, cancellation/late cleanup, statistical locks,
role/count/deadline/battery/charger bounds and all existing regressions.
All nine strict UID-bound embedded profiles pass:

| Profile | Flash bytes | Accounted RAM bytes | Trimmed estimate |
| --- | ---: | ---: | --- |
| factory-release | 57964 | 16136 | ON |
| bringup | 63828 | 15380 | OFF |
| product-debug | 61280 | 15200 | OFF |
| product-release | 64036 | 15520 | OFF |
| factory-debug | 57588 | 16056 | ON |
| bringup-cal | 37572 | 13744 | OFF |
| bench-supply | 65272 | 15468 | OFF |
| k1-test | 65392 | 15396 | OFF |
| peripheral | 14628 | 12252 | OFF |

Factory adds 96 B accounted RAM over the prior session/menu image and remains
inside existing preferred/hard gates. There is no heap, framebuffer, seven ADC
buffers, calibration rewrite or new persistent record format. BSP, acquisition
clock/ADC behavior, drivers, complex calibration/DSP equations and NTC coefficients
are unchanged. Legacy images retain their footprint boundaries.

firmware-images.zip includes all nine images/maps/ELFs and the guarded factory
manifest. validation-evidence.zip includes deployment/readback, compiler-derived
ABI snapshots, raw seven points, SAFE metadata, host/build logs, the first failed
integration and final successful run. checkpoint.json includes the independently
replayed physical checkpoint. No statistical accuracy bound is claimed. Physical
capacitor filtering, independent metrology, fault/transient timing, sub-uH limits
and the AVG7/3 screen indicator remain pending bench qualification.

See [plan 25](../../../../plans/25-Trimmed-Measurement-Estimate.md).
