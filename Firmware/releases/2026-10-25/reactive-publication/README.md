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
