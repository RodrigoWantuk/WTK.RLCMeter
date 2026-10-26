# 22 — Reactive result publication with unresolved loss

STATUS: IN_PROGRESS

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
