# High-Z OPEN repeatability pilot — diagnostic only

`open-pilot.zip` preserves six separate physical OPEN CCO1 artifacts at
1 Mohm / 10 kHz / 100 mVrms, decoded observations, the complete UART log and
the bounded acquisition script. Source is f86b7f4e, UID
54ff70068370525760380567, hardware 0x10002/model5. Each original artifact contains
six accepted hardware blocks, zero rejected blocks, HG-only validity and no
observed HG overlap. These 36 hardware blocks are not a new integrated observation.

Across the six exported six-block means, relative complex standard deviation is
6.91360% for normalized 1X transfer and 6.83089% for effective HG transfer.
The mean-of-ratios gain is diagnostic only; no coefficient or validity flag was
installed. This is not accuracy, physical qualification or proof of a noise model.

The final serial battery reading is 3.942 V, nominal scaling. All six captures
returned SAFE/idle, fault/protocol counters stayed zero, and active OSL remains
absent. Original source-7017e5e0 observations are unchanged and separate.
The owner subsequently remeasured the resistor as 996 kohm and confirmed direct
attachment alone. `load-pilot.zip` adds six independent LOAD artifacts at
100 mVrms and six at 500 mVrms, complete UART evidence and the bounded scripts.
The nominal 1 Mohm +/-1% uncertain reference remains separate from the DMM
consistency reading; no certified actual resistance is inferred.

Across the six exported means, normalized 1X / effective HG relative complex
standard deviations are 4.53700% / 4.66066% at 100 mVrms and
1.94897% / 1.90355% at 500 mVrms. These are diagnostics of exported means, not
the embedded within-artifact stability test. All 12 artifacts retain HG-only
validity and no observed HG overlap. No flags or coefficients were promoted.

Fresh serial battery was 3.947 V before LOAD and 3.934 V at the end. All captures
returned SAFE, with zero safety faults/protocol errors and no calibration
installation. Source and physical acquisition remain unchanged. Higher amplitude
improved repeatability but does not establish a component fault, a validated
integration design or measurement accuracy.

The owner then confirmed "open". `open-500-pilot.zip` completes the matched
comparison with six OPEN artifacts at500 mVrms on the same source. Normalized
1X / effective HG dispersion across means is2.45374% /2.42160%, still with HG-only
artifact flags and no observed overlap. Final serial battery is3.941 V, SAFE/idle,
fault/protocol counters zero. See `open-500-checkpoint.json` for exact evidence.
The next phase implements and tests bounded coherent groups before any new
physical pilot. No grouped result has yet been acquired or qualified.
See [the execution plan](../../../../plans/14C-High-Z-Integration-Investigation.md).
