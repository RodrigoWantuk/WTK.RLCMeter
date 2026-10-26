# Reactive publication investigation

A SAFE, no-reset/no-request read preserved the owner's completed 103J100
polyester capacitor session before any update. All six actual attempts and
ARM ABI layout are in capacitor-failure-evidence.zip. The calibrated primary
is -20.685913 - j1656.604614 ohm, equivalent C 9.607297 nF at 10 kHz; DSP and
selected-channel quality pass, but UI derived_valid is false solely because
factory PRODUCT requires all estimated resistance to be nonnegative.

Plan 22 separates reactive C/L from unresolved loss, only for reactive session
classification with |X| > |R|, retaining all existing session/quality/provenance/
finite/sign checks. Raw negative R is preserved. Negative R cannot become ESR,
Q or D, and negative resistive/dominant-R results remain invalid. No calibration,
ADC/DSP or hardware threshold is changed; no reference is relabeled. This does
not qualify phase, absolute C/L or loss accuracy. The owner's unidentified very
small choke is not a calibrated inductor standard; a marked part is pending.
Physical installation and post-fix capacitor validation follow separately.

## Deployed result

Source 3052bdde0b69c7d957cb70f4d150ec58843789f3 was programmed/readback-verified
(54,320 bytes) and boots READY with active sequence 1, all 33 records, normal
TFT speed and 75% backlight. Passive UART SAFE_BOOT is captured; no guessed
PRODUCT measurement command is used. After two owner-triggered short OK presses
on the unchanged capacitor, actual completed results are 9.548414 nF (four
attempts) and 9.684613 nF (three attempts), both classified capacitive with
UI derived_valid/capacitance_valid true, no acquisition safety failure.
Final metadata confirms K1 SAFE, range/excitation OFF, quiet released, zero
safety faults, resources OK and active calibration unchanged. No resistor
fixture or calibration capture was repeated. Both results are compatible with
the nominal 10 nF / J=5% marking, not independent proof of instrument accuracy.

The two post-fix primary estimates of R are positive (31.91 and 29.00 ohm), so
ESR/Q/D are numerically available under the preexisting guards; their accuracy
is not qualified. This does not claim a post-fix physical negative-R primary.
The original negative-R failure is preserved and exercised by the controller
regression, including rejection of negative resistive/dominant-R cases and
invalidation of ESR/Q/D when R is negative. All 46 standard / 47 factory host
tests and nine strict unit-bound builds pass. No BSP/hardware/measurement/driver
source changed relative to the preceding display image. The unidentified small
choke remains for later validation with a marked component at the owner's request.
