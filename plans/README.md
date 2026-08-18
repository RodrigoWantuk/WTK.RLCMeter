# Firmware Execution Plans

This directory contains the ordered implementation program for WTK.RLCMeter firmware, intended for human developers and AI coding agents.

The plans are **execution documents**, not general architecture notes. Architecture is documented under [`../docs`](../docs). Agents must also follow [`../AGENTS.md`](../AGENTS.md).

## Plan index

0. [`00-Firmware-Execution-Program.md`](00-Firmware-Execution-Program.md) — complete dependency graph, phase boundaries, cross-phase acceptance rules, and final deliverables.
1. [`01-Toolchain-CMake-and-VSCode.md`](01-Toolchain-CMake-and-VSCode.md) — C17 build foundation, Arm toolchain, presets, workspace integration, host tests, CI-ready commands.
2. [`02-Platform-BSP-and-Diagnostics.md`](02-Platform-BSP-and-Diagnostics.md) — STM32 startup, clocks, safe GPIO, JTAG/SWD remap, UART, watchdog, reset/time foundation.
3. [`03-SPI-Display-Flash-and-Input.md`](03-SPI-Display-Flash-and-Input.md) — SPI ownership, W25Q, ILI9341, buttons, backlight, buzzer, asset primitives.
3A. [`03A-Wokwi-Virtual-Hardware-Validation.md`](03A-Wokwi-Virtual-Hardware-Validation.md) — Wokwi-based virtual Blue Pill regression tests, automated GPIO/UART/SPI/TFT/Flash scenarios, and CI integration between host tests and bench validation.
4. [`04-Safety-Power-and-Range-Control.md`](04-Safety-Power-and-Range-Control.md) — residual sensing, charger/battery/NTC, K1/K2, range decoder, fail-safe state enforcement.
5. [`05-Excitation-ADC-and-DMA.md`](05-Excitation-ADC-and-DMA.md) — PWM excitation, deterministic ADC1/ADC2 sampling, timer trigger, DMA buffers, raw-capture diagnostics, quiet mode.
6. [`06-DSP-and-Impedance-Core.md`](06-DSP-and-Impedance-Core.md) — synchronous phasors, complex channel reconstruction, impedance equation, R/X/phase/RLC derivation, host vectors.
7. [`07-Autorange-Confidence-and-Calibration.md`](07-Autorange-Confidence-and-Calibration.md) — range policy, 1X/HG selection, confidence gates, OPEN/SHORT/LOAD, persistence, qualification map.
8. [`08-UI-Storage-and-Product-Integration.md`](08-UI-Storage-and-Product-Integration.md) — full UI, asset pack, settings, diagnostics console, power policy, integration hardening.
9. [`09-Bringup-Qualification-and-Release.md`](09-Bringup-Qualification-and-Release.md) — board validation, metrology qualification, regression matrix, Rev.1 release evidence, Rev.2 decision inputs.

Phase 03A is an orthogonal validation layer rather than a new firmware-feature phase. It should be established after the Phase 02/03 digital foundations and then reused by later phases. It does not replace any physical bench gate.

## How agents use these plans

Before starting a phase:

1. read `AGENTS.md`;
2. verify all previous required phases are complete or the assigned work is truly independent;
3. inspect current repository state rather than assuming the plan has not already been partially implemented;
4. identify exact files to create/change;
5. identify automatic checks and hardware-only checks;
6. keep the implementation inside the phase scope.

At completion, provide a handoff with:

- implementation summary;
- files changed;
- build/test commands executed;
- results;
- memory/size observations where relevant;
- remaining `REQUIRES_BENCH_VALIDATION` items;
- deviations from the plan;
- newly discovered blockers/risks;
- next phase readiness.

## Plan status convention

Each phase should eventually maintain one status near the top:

```text
STATUS: NOT_STARTED
STATUS: IN_PROGRESS
STATUS: BLOCKED
STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION
STATUS: COMPLETE
```

Do not mark a hardware-dependent phase `COMPLETE` merely because firmware compiles. Use `IMPLEMENTED_REQUIRES_BENCH_VALIDATION` until the plan's bench acceptance criteria have evidence.

## Scope discipline

A phase may create narrow supporting utilities required by its own acceptance criteria, but should not opportunistically implement later product features.

Examples:

- Phase 01 may create a minimal placeholder target needed to prove the toolchain, but it does not implement TFT or measurement logic.
- Phase 03 may provide basic display test screens, but it does not implement the final product UI.
- Phase 03A may add simulator-only infrastructure and narrowly scoped Lab diagnostics, but it must not change production behavior to make virtual tests pass.
- Phase 05 may stream raw ADC samples for validation, but it does not implement final impedance math.
- Phase 06 may use fixed synthetic measurement conditions in host tests, but it does not silently implement autorange.

## Decision escalation

If a phase exposes a missing architecture decision, record it explicitly. Consequential choices should be added to `docs/10-Consolidated-Design-Decisions.md` once resolved.

Typical examples:

- exact ADC sample-rate strategy;
- exact ADC1/ADC2 channel scheduling;
- HAL vs LL boundary for ADC/DMA/timers;
- C test framework selection;
- W25Q minimum supported density and partition map;
- calibration correction model;
- debug-probe/Cortex-Debug configuration.

## Definition of program success

Rev.1 firmware is not considered complete until:

- command-line CMake builds are reproducible;
- VS Code workflow works without becoming a build dependency;
- safe boot/reset/fault behavior is validated;
- peripherals are validated on hardware;
- excitation and sampling timing are measured;
- DSP passes synthetic vectors and real-reference comparisons;
- calibration records are persistent/versioned;
- autorange/confidence behavior is qualified;
- UI remains responsive without corrupting acquisition;
- a documented measurement qualification matrix exists;
- unsupported/future capabilities remain clearly outside Rev.1.
