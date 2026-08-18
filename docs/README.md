# Technical Documentation

This directory contains the living technical specification for WTK.RLCMeter.

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
