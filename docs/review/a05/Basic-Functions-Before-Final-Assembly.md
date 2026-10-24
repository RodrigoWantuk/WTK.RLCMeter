# Basic functions before final assembly

Status: IN_PROGRESS; NO FINAL-ASSEMBLY GO-AHEAD ISSUED.

The owner requested a bounded functional check before completing assembly, with
calibration deferred until afterwards. Active scope is physical09A / plan12
M12-03/M12-06/M12-09. Do not expand this check into full metrology qualification,
and do not silently waive a known electrical assembly blocker.

## Functional checks and actual evidence

| Function | Current evidence | Remaining basic check |
| --- | --- | --- |
| Boot, MCU clock and UART | Actual boot and repeated COM3 transactions on firmware480aa310f557 | Retain safe startup on the final fitted configuration |
| Supply rails | Owner reports5V and3.3V; original COUT bridge repaired | Verify supply suitability/current and any newly fitted power assembly |
| Buzzer and K1 actuation | Owner confirmed audible buzzer and relay actuation | Contact/safety behavior is separate from audible actuation |
| ADC/DMA and cleanup | Repeated256-row/768-word captures, no DMA error; commanded SAFE/range-off/excitation-off cleanup | Do not equate UART state with contact or waveform feedback |
| Resistance measurement |10K,33K,99.7K,329.5K,996K fixtures measured; RREF10K/100K/1M exercised | Remaining low ranges are untested; preserve observed channel/range disagreement |
| Capacitive measurement | Actual103J100 fixture: three1kHz/10K/100mV captures, negative X, host C10.56..10.58nF, SAFE cleanup | Phase-helper fix was deployed as12f03bb; polarized method is a separate unresolved requested capability |
| Inductive measurement | Blind2.2uH indication near1uH was unreliable;470uH fixture gave three positive-X/INDUCTIVE captures at10kHz/100R, equivalent L425.574..429.423uH, SAFE cleanup | Basic inductive response observed; small-L resolution, absolute L/phase/ESR and transient behavior remain unqualified |
| Final peripherals and power | Display attached, white screen; owner confirms distinct button tones. Peripheral-only image e4c8633 has no active measurement path. Battery absent and sense input floating | Diagnose TFT communication/rendering; verify power/battery separately |

The10nF check is complete. The owner then fitted a blind electrolytic25V part,
negative on TEST_HI; no polarized capture was performed. It was superseded by
owner-confirmed2.2uH and470uH inductor fixtures. The third inductor test was canceled.
Before the peripheral image, the last active capture cleaned up SAFE/off/fault0,
bench source revoked and COM3 closed. UART wires are now removed to fit the TFT.
SWD on the peripheral image confirms K1/range/excitation off and rendering stage8
without a software transfer failure. This does not demonstrate visible pixels. See
[reactive evidence and pending polarized method](Reactive-Basic-Checks-2026-10-24.md).
The owner explicitly expects polarized-capacitor capability; assess a bounded
method/domain before an assembly recommendation, rather than silently dropping it
or claiming that all polar parts are unsupported in principle.

## Decision at the end of the basic checks

Give a scoped assembly recommendation only after the intended R/C/L observations
exist and the owner identifies what remains to be fitted. State which functions
were demonstrated and which ranges were not tested. Stop a subtask if a fault,
invalid required channel, unsafe cleanup, unexplained heating or a relevant
electrical assembly blocker appears; diagnose that observation before proceeding.

The recommendation must distinguish mechanical/peripheral completion from first
battery/charger operation. Display readiness reported with the TFT disconnected
does not prove physical display or backlight compatibility. Battery/charger wiring,
power ratings and interlock behavior need their own applicable checks before that
power configuration is energized. Pending A05 interface/electrical findings remain
applicable; do not mark the historical checklist globally passed.

No OSL record is created in this functional pass. Calibration and precision,
repeatability, full range/frequency coverage, gate/reset/power/inductive transient
qualification, and final product release remain REQUIRES_BENCH_VALIDATION. Use
host complex math/atan2 as an independent reactive cross-check; the former deployed
phase-field defect was fixed in12f03bb. Nominal values are plausibility controls, not tight
ESR/Q or accuracy standards.

Detailed resistor evidence and diagnostic limitations are in
[the bench reference report](Bench-Reference-Checks-2026-10-24.md).
