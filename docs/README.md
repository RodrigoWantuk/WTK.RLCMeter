# Technical Documentation

Latest first-unit checkpoint: [calibrated PRODUCT READY handoff](../Firmware/releases/2026-10-25/calibrated-product-ready/README.md).
The 33-condition engineering OSL is installed and persistent. PRODUCT now loads
it and reaches READY with SAFE outputs. The owner confirms splash and READY;
independent metrology qualification remains pending. The linked handoff also records current
C/L, polarized-capacitor, small-inductor, Click/Live and frequency-search limits.
Chronological checkpoints below retain their original historical status and are
superseded where the latest handoff provides newer evidence.

Subsequent owner feedback: [display speed/redraw/brightness delivery](../Firmware/releases/2026-10-25/display-feedback/README.md)
is installed and visibly improved. [Reactive result publication](../Firmware/releases/2026-10-25/reactive-publication/README.md)
fixes intermittent capacitor n/a caused by a blanket nonnegative-R presentation
guard; actual post-fix readings are 9.55/9.68 nF on the unchanged 103J100. Negative
estimated R never becomes valid ESR/Q/D. Unknown small-choke observations still
need a marked inductor; calibrated value availability is not an accuracy certificate.

[Known 470-uH follow-up](../Firmware/releases/2026-10-25/inductor-470-check/README.md)
now records two actual INDUCTIVE results (461.4/472.2 uH) and SAFE teardown.
The full frequency/range spread is preserved; 0.15/2.2-uH sensitivity and
independent accuracy remain unvalidated.

Current October24 CAD/firmware, CHG_VBUS5.5V analysis, calibration identity and bench status: [consolidation report](review/bringup-2026-10-24/README.md).

This directory contains the living technical specification for WTK.RLCMeter.

October 25 continuation: [serial battery telemetry](../Firmware/releases/2026-10-25/battery-serial-telemetry/README.md)
is deployed and reports 3.949 V after charging, using COM4 only. The separate
[high-Z OPEN pilot](../Firmware/releases/2026-10-25/osl-integration-pilot/README.md)
records repeatability evidence; the three high-Z/10-kHz OSL fits remain pending.

The documentation deliberately separates **existing Rev.1 hardware**, **planned firmware architecture**, **unqualified engineering targets**, **experimental/DNP options**, and **future-revision concepts**. Measured prototype behavior takes precedence over theoretical expectations.

## Index

1. [`01-Hardware-Architecture.md`](01-Hardware-Architecture.md) — analog front-end, RREF bank, power, relays, TFT, and Flash.
2. [`02-Measurement-Model-and-DSP.md`](02-Measurement-Model-and-DSP.md) — complex equations, synchronous acquisition, and R/L/C derivation.
3. [`03-Safety-and-Protection.md`](03-Safety-and-Protection.md) — SAFE/MEASURE, residual voltage, interlocks, and limits.
4. [`04-Firmware-Architecture.md`](04-Firmware-Architecture.md) — C17/CMake architecture, state machine, timers, DMA, persistence, and editor/build policy.
5. [`05-Pinout-and-Interfaces.md`](05-Pinout-and-Interfaces.md) — STM32 pin allocation and Rev.1 connectors.
6. [`06-UI-UX-and-Diagnostics.md`](06-UI-UX-and-Diagnostics.md) — ILI9341, assets, screens, visualizations, buttons, backlight, buzzer, and diagnostics.
7. [`07-Calibration-and-Validation.md`](07-Calibration-and-Validation.md) — OPEN/SHORT/LOAD, confidence, and metrology qualification.
8. [`08-Roadmap.md`](08-Roadmap.md) — implementation milestones and future-revision boundary.
9. [`09-Rev1-Bringup.md`](09-Rev1-Bringup.md) — safe staged assembly and power-up procedure.
10. [`10-Consolidated-Design-Decisions.md`](10-Consolidated-Design-Decisions.md) — decisions that should not be reopened without new evidence.
11. [`11-Rev1-BOM-and-Assembly.md`](11-Rev1-BOM-and-Assembly.md) — structural components, DNP population, manual assembly, and substitution rules.
12. [`12-Functional-Specification.md`](12-Functional-Specification.md) — feature catalog, maturity labels, engineering ranges, and functional acceptance boundaries.
13. [`13-Detailed-Firmware-Design.md`](13-Detailed-Firmware-Design.md) — module contracts, data structures, persistence, DSP, and fault behavior.
14. [`14-Future-Extensions.md`](14-Future-Extensions.md) — Kelvin/4-wire, high voltage, USB, guard, K2, and other concepts outside Rev.1.
15. [`15-Measurement-Operating-Modes.md`](15-Measurement-Operating-Modes.md) — Click and Live measurement semantics, scheduling, result publication, UI interaction, and safety boundaries.
16. [`16-Rev1-MVP-Bench-Guide.md`](16-Rev1-MVP-Bench-Guide.md) — first-board BRINGUP/Product procedure for the bench-usable Rev.1 MVP.

AI-assisted firmware execution is planned separately under [`../plans`](../plans/).

## Terminology

- `DUT`: Device Under Test.
- `VEXC`: analog excitation voltage.
- `VMID`: virtual reference near half of 3.3 V.
- `RET`: node between RREF and DUT.
- `RREF`: selected reference impedance.
- `RET_1X`: return channel without additional gain.
- `RET_HG`: high-gain return channel; current nominal hardware gain is approximately 15.47×.
- `SAFE`: DUT isolated from the measurement AFE and connected to the protection/sensing network.
- `MEASURE`: DUT connected to the measurement path.
- `NOMINAL`: qualified main operating region.
- `EXTENDED`: functional region outside the primary qualified region.
- `LOW_CONFIDENCE`: result without sufficient evidence for ordinary publication.
- `DNP`: footprint present, component intentionally not populated.

## Source-of-truth precedence

When historical documents disagree with the current implementation, use this precedence:

1. latest PCB/BOM that will actually be assembled;
2. matching schematic;
3. current technical documentation;
4. historical revision notes;
5. older discussions/estimates.

This is especially important for values that changed during design, such as op-amp selection, high-gain ratio, K2 population, and package choices.

## Metrology truth rules

- Accuracy is measured, not inferred from the BOM.
- Engineering targets around 1 Ω–10 MΩ, 1 nF–10 mF, and 10 µH–10 H still require qualification.
- The 1 MΩ range depends strongly on real leakage and parasitic capacitance.
- MCP6002 response at 10 kHz requires complex calibration/validation.
- Nominal `RET_HG` gain does not replace a calibrated transfer response.
- Residual-voltage protection does not imply a CAT rating.
- Interfaces unavailable in the current pinout must not be described as existing capabilities.
- Derived UI visualizations must not be confused with acquisition modes that have not been implemented/qualified.
- Future features must remain explicitly marked as outside Rev.1.

## Documentation language

All repository documentation is maintained in **English**. New Markdown files, architecture notes, agent plans, contributor documentation, and durable code comments intended as documentation should also be written in English.

## Documentation update rule

When hardware changes affect analog response or safety:

1. update schematic/BOM/PCB artifacts;
2. update `10-Consolidated-Design-Decisions.md` when the decision changes;
3. update pinout/hardware/safety documentation as applicable;
4. update the `hardware_revision` identity used by calibration;
5. review bring-up and qualification procedures;
6. update affected agent execution plans if implementation assumptions changed.

[Personalized boot splash delivery, October 25](review/bringup-2026-10-25/Boot-Splash.md)
records all nine builds, physical pack readback, preserved mutable data and final
SAFE BRINGUP. The owner will inspect visible TFT appearance later.

[First physical OSL OPEN checkpoint, October 25](review/bringup-2026-10-25/OSL-Open.md)
records 33 accepted OPEN conditions, corrected capture/client behavior and the
current nine-image bundle. SHORT/LOAD and calibration installation remain pending.

[The first SHORT checkpoint and path audit](review/bringup-2026-10-25/OSL-Short.md)
preserve 33 accepted SHORT observations and three OPEN/SHORT path conflicts.
Zero-return SHORT handling is corrected before a same-image recapture; no
calibration is installed or physically qualified.

[Corrected SHORT checkpoint](review/bringup-2026-10-25/OSL-Zero-Short.md)
records 33/33 accepted conditions with both return paths on source `7017e5e0`.
The source-bound nine-image bundle and deployment readback are preserved.
Same-image OPEN/LOAD, installation and independent qualification remain pending.

[Same-image OPEN/SHORT completion](review/bringup-2026-10-25/OSL-Same-Image-Endpoints.md)
records 66 accepted captures on source `7017e5e0`, with common return paths at all
33 keys. LOAD, solver validation, installation and independent qualification
remain pending; earlier endpoint checkpoints retain their historical status.

[First 10-ohm LOAD group](review/bringup-2026-10-25/OSL-Load-10.md) records three
accepted same-image triplets (69/99 observations), their strict float32 fit
checks and the owner's battery check requirement before every new resistor.
Five LOAD groups, independent validation and installation remain pending.

[100-ohm LOAD completion](review/bringup-2026-10-25/OSL-Load-100.md) records six
more accepted captures and fit checks: 75/99 observations, 9/33 complete
conditions. Battery remains OK; four LOAD groups and independent validation remain.

[1-kohm LOAD completion](review/bringup-2026-10-25/OSL-Load-1000.md) records six
more accepted captures: 81/99 observations, 15/33 complete fit checks. Fresh
battery is OK; the 10-kohm, 100-kohm and 1-Mohm LOAD groups remain pending.

[10-kohm LOAD completion](review/bringup-2026-10-25/OSL-Load-10000.md) records
87/99 observations and 21/33 complete fit checks. Battery remains OK; the
100-kohm and 1-Mohm LOAD groups, independent validation and installation remain.

[100-kohm capture and unresolved fit](review/bringup-2026-10-25/OSL-Load-100000.md)
records 93/99 observations, 26 accepted fits and one blocked fit at 100 kohm /
10 kHz / 100 mVrms because actual HG overlap is absent. All six raw 100-kohm
captures are preserved. The 1-Mohm LOAD group and resolution of that fit remain
pending; no full candidate or calibration installation exists.

[Complete 99-capture campaign and diagnostic](review/bringup-2026-10-25/OSL-All-99.md)
records 30 valid fits and three blocked high-Z 10-kHz conditions. Three separate
OPEN repeats reproduce the missing HG-overlap evidence; compiler-derived RAM
statistics explain the final LOAD rejection. The candidate builder remains
blocked. A one-point NTC comparison using an owner-provided contact thermometer
is next; no calibration has been installed or physically qualified.

[NTC cold comparison](review/bringup-2026-10-25/NTC-Comparison.md) preserves the
external 23.5-degC bead-contact reference against firmware 27.27030 degC. The
purchased NTC is listed only as 100 kohm; Beta is unknown. A warm reference is
pending before fitting an effective divider ratio and Beta. No trim is applied.

[Experimental NTC factory profile delivery](review/bringup-2026-10-25/NTC-Factory-Profile.md)
supersedes that cold-comparison checkpoint: the owner-authorized two-point model
is compiled into all nine UID-bound unit images and verified on the board.
Independent thermal accuracy is not qualified. The three RLC fit blocks remain.

The [nominal 0.47-uH follow-up](../Firmware/releases/2026-10-25/inductor-047-limit-check/README.md) was resistance-like at 1 kHz
and did not publish L. Automatic frequency search and sub-microhenry
capability remain unvalidated; this is not a proven hardware lower limit.

[Bounded low-impedance search](../Firmware/releases/2026-10-25/low-impedance-search/README.md)
is deployed and software-tested across both host profiles and nine embedded
profiles. It adds a supporting 10-kHz check for usable low-RREF resistive
results without changing classification thresholds or calibration. Actual
extra-frequency capture is pending; K1 remains SAFE between attempts.
