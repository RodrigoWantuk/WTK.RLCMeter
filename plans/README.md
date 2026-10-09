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
9A. [`09A-Physical-MVP-Bringup.md`](09A-Physical-MVP-Bringup.md) — bench-usable Rev.1 MVP, compact readiness status, automatic BRINGUP measurement, and first-board test sequence.
9B. [`09B-Extended-Measurement-and-Calibration.md`](09B-Extended-Measurement-and-Calibration.md) — guarded DC feasibility, supplementary PC-hosted calibration, and unresolved electrical/size gates.
9. [`09-Bringup-Qualification-and-Release.md`](09-Bringup-Qualification-and-Release.md) — board validation, metrology qualification, regression matrix, Rev.1 release evidence, Rev.2 decision inputs.


## October 2026 Rev.1 recovery, metrology and calibration review

The following complementary execution plans were written against commit `8abc1b3` and are intentionally **proposed pending physical qualification and owner decisions**. They do not replace or invalidate completed phases 01–09B. Work that is host-only, size-forensics, or safety-evidence collection can start now; changing qualification status or electrical limits cannot.

- [10 — Technical review and decision record](10-Rev1-Recovery-Review-and-Decisions.md) — current firmware/PCB baseline, blockers and owner questions.
- [11 — Flash recovery and profiles](11-Flash-Recovery-and-Profile-Split.md) — linked-byte audit, candidate cleanup, separate bringup images and size gates.
- [12 — Hardware metrology and guarded DC](12-Hardware-Metrology-and-DC-Qualification.md) — observable quantities, pad-level verification, AC physical matrix, guarded DC permission/error budget.
- [13 — Curves and uncertainty](13-Curve-Calibration-Intervals-and-Uncertainty.md) — X/Y model catalogue, reference tolerance intervals, holdouts, OSL-vs-overlay model selection and credible displayed error.
- [14 — PC calibrator and accessible reference kit](14-PC-Calibrator-Accessible-Standards-and-Scope.md) — Brazilian-store friendly components, safe scope probing, GUI UX, campaign evidence and transactional protocol prerequisites.
- [15 — Agent workstreams and release gates](15-Execution-Workstreams-and-Release-Gates.md) — ordered, parallelizable tickets, acceptance tests and product definition of done.
- [16 — Optional DC and transient physics](16-Optional-DC-and-Transient-Physics.md) — DC winding resistance, capacitor self-discharge versus known-load RC decay, dielectric absorption, inductor RL field decay, physical feasibility and go/no-go before assembly.

**Agent entrypoint:** start with A01 map attribution and A05 pre-assembly PCB/DNP audit from plan 15; run A03/A04 (host-only) in parallel. The confirmed Hantek DSO2C10 is for scope time/phase/waveform validation, not unqualified sub-1% DC voltage accuracy. Full OSL calibration moves toward the PC, with optional lean local OPEN/SHORT. Flash optimization is judged against a dynamic full-product forecast rather than a fixed 56KiB. Follow AGENTS.md and every 'REQUIRES_BENCH_VALIDATION' gate. No plan authorizes removing safety or claiming unmeasured accuracy.

**A01 evidence available (2026-10-09):** [Flash attribution and completion budget](../docs/review/a01/README.md).
A01 tooling is complete; plan 11 remains in progress with no production reduction
yet. Begin A02 using the measured 2,516 B supplementary-curve opportunity; review
A06 provisioning before changing the calibration boot gate. The report also fixes
historical double-counting of the reserved stack in RAM totals.

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
- Phase 03A may add simulator-only infrastructure and narrowly scoped Bringup diagnostics, but it must not change production behavior to make virtual tests pass.
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
