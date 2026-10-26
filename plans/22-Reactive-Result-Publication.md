# 22 — Reactive result publication with unresolved loss

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION — TWO_ACTUAL_CAPACITOR_READINGS_PASS

The owner reports intermittent n/a on the isolated 103J100 polyester capacitor
and an unidentified very small choke shown as about 0.5 ohm. The capacitor is
the known nominal 10 nF / 5% part. The owner later reconnected TTL. Before any
reset, a SAFE read-only PRODUCT snapshot preserved completed session 16 and all
six attempts, with exact firmware/ABI verification and no measurement command.
The primary result is calibrated complex Z = -20.685913 - j1656.604614 ohm at
10 kHz / 1 kohm / 100 mVrms, equivalent C = 9.607297 nF. DSP is OK, selected HG
usable/calibrated/unclipped, final classification capacitive and quality valid,
but PRODUCT derived_valid=false because its general guard demands R >= 0.
Other well-ranged attempts gave 9.782696 nF at 1 kHz and 9.747803 nF at 10 kHz,
also with negative estimated R. These observations do not qualify phase or ESR.

Separate reactive value publication from loss publication. Keep all existing
finite-data, selected-channel, calibration-key/provenance, successful-session,
quality, reactance-sign and classification checks. Negative estimated R may
retain C/L only when the session class is reactive and |X| > |R| (reactance
dominates), so a negative-R-dominated result remains invalid. Preserve raw R;
do not clamp it to zero or fabricate ESR/Q/D. ESR requires R >= 0; Q/D continue
to require positive finite R. Error bounds/qualification remain unchanged and
uncharacterized. This is a publication-model rule, not an accuracy guarantee.

Expected files: app_product publication guard and factory controller regression
using the captured capacitor values, negative-loss L model, rejected negative
resistive/dominant-R cases and existing nonfinite/provenance/clipping tests.
No hardware, ADC timing, DSP/calibration model, coefficients, safety threshold
or resistor fixture is changed. Test standard/factory host paths and relevant
embedded profiles, preserve evidence and program verified PRODUCT. With the
103J100 unchanged, request one user-triggered short OK and read the completed
value/flags/SAFE teardown; physical UI confirmation can follow.

The owner cannot identify the previous choke and will later provide a marked
part. Small L may be resistance-dominated at the probe frequency; do not infer
a broken AFE or change classification thresholds from an unidentified DUT.
Known-inductor validation remains pending after the capacitor fix.

Source 3052bdde0b69c7d957cb70f4d150ec58843789f3 passes 46/47 host CTests by profile
and nine strict UID-bound builds. Verified PRODUCT programming/boot preserves
sequence 1 and all 33 records. Two owner-triggered 103J100 readings return
9.548414 and 9.684613 nF with valid published C. Both new primaries have positive
R; the original negative-R failure is replayed in the host controller regression,
not relabeled as a post-fix physical negative-R capture. Actual final teardown
is SAFE, range/excitation OFF, no safety faults. Known-inductor, independent
C/L/loss/phase accuracy and final display confirmation remain pending.
See [delivery](../Firmware/releases/2026-10-25/reactive-publication/README.md).

## Subsequent known-inductor basic check

The owner then fitted a marked nominal 470-uH choke and triggered two Click
measurements. Both finish INDUCTIVE with published L: 461.356 and 472.200 uH
at 1 kHz / 10 ohm / HG, four then two attempts. Actual SAFE teardown, zero
faults and unchanged sequence 1 are verified. The full 1/10-kHz/range history
is retained, including lower secondary L estimates; nominal tolerance, phase,
loss and absolute accuracy are unqualified. No code or calibration was changed.
The basic known-inductor check is now observed; 0.15/2.2-uH sensitivity and
independent qualification remain pending. See
[physical evidence](../Firmware/releases/2026-10-25/inductor-470-check/README.md).

## Nominal 0.47-uH automatic-mode limit observation

The owner then fitted a nominal 0.47-uH inductor and triggered session 5.
One 1-kHz / 10-ohm / HG attempt produced R=0.235328 ohm, X=-0.0100988 ohm,
RESISTIVE classification and no published L or C. The current policy skips
frequency refinement for resistive results, so 10 kHz was not tested. This
observes a current automatic-search limitation, not a proven hardware lower
limit. Do not relabel the negative reactance as a successful inductance result.
Actual SAFE teardown, zero faults and the same 33-record calibration were
verified; no code/calibration was changed. Sub-microhenry sensitivity, phase
and fixture compensation remain REQUIRES_BENCH_VALIDATION. See
[physical evidence](../Firmware/releases/2026-10-25/inductor-047-limit-check/README.md).
