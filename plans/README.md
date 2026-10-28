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
- [14A — Experimental unit NTC factory profile](14A-Unit-NTC-Factory-Profile.md) — owner-authorized, UID-bound two-point lookup-table delivery; independent thermal qualification remains pending.
- [14B — Calibration serial battery telemetry](14B-Calibration-Battery-Telemetry.md) — fresh nominal VBAT over read-only TTL STATUS; first-unit charger-absent reading verified.
- [14C — High-Z integration investigation](14C-High-Z-Integration-Investigation.md) — bounded OPEN/LOAD repeatability diagnostics before changing integration or calibration provenance.
- [14D — Condition-local calibration reuse](14D-Condition-Local-Calibration-Reuse.md) — explicitly audited reuse of complete compatible triplets, preserving source identity and all 33-condition validation gates.
- [14E — Installation sensor refresh](14E-Installation-Sensor-Refresh.md) — complete engineering installation and reboot persistence verified; wait through all eight real residual requalification sweeps, retaining hard interlocks and deadlines.
- [15 — Agent workstreams and release gates](15-Execution-Workstreams-and-Release-Gates.md) — ordered, parallelizable tickets, acceptance tests and product definition of done.
- [16 — Optional DC and transient physics](16-Optional-DC-and-Transient-Physics.md) — DC winding resistance, capacitor self-discharge versus known-load RC decay, dielectric absorption, inductor RL field decay, physical feasibility and go/no-go before assembly.

**Agent entrypoint:** start with A01 map attribution and A05 pre-assembly PCB/DNP audit from plan 15; run A03/A04 (host-only) in parallel. The confirmed Hantek DSO2C10 is for scope time/phase/waveform validation, not unqualified sub-1% DC voltage accuracy. Full OSL calibration moves toward the PC, with optional lean local OPEN/SHORT. Flash optimization is judged against a dynamic full-product forecast rather than a fixed 56KiB. Follow AGENTS.md and every 'REQUIRES_BENCH_VALIDATION' gate. No plan authorizes removing safety or claiming unmeasured accuracy.

**A01 evidence available (2026-10-09):** [Flash attribution and completion budget](../docs/review/a01/README.md).
A01 tooling and [A02.1 curve isolation](../docs/review/a02/README.md) are complete
as software evidence; plan 11 remains in progress. PRODUCT Release defaults to
curves OFF at 61,580 B Flash / 14,976 B RAM (2,516 B Flash / 64 B RAM saved).
Budget the same measured cost when restoring the runtime. Review A06 provisioning
before changing the calibration boot gate. Physical qualification remains pending.

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

## First-board consolidation

[Plan17](17-Bringup-Consolidation-and-Publish.md) integrates October24 bring-up findings, current owner exports, CHG input analysis, all firmware profiles and Git publication.

[Plan18](18-First-Board-Resource-Provisioning.md) records the owner-requested
production resource installation, physical W25Q readback, preserved mutable data
and return to standard BRINGUP for remaining bench work.

[Plan 19](19-Shared-Boot-Splash-and-Provisioning.md) adds the owner-supplied splash
to every firmware profile and the external resource provisioning procedure.

[Plan 20](20-Calibrated-Product-Handoff.md) transitions the calibrated first unit
to PRODUCT without another resistor campaign, correcting application scheduling
around idle ranges, residual requalification and slow TFT work.

[Plan 21](21-Product-Display-Feedback.md) records the owner-confirmed display
speed, duplicate-READY rendering and default-brightness correction.
[Plan 22](22-Reactive-Result-Publication.md) separates dominant reactive C/L
publication from unresolved loss; two actual capacitor readings pass, while
the nominal 470-uH basic check is observed and independent accuracy qualification
remains pending.

[Plan 23](23-Bounded-Low-Impedance-Search.md) adds a bounded 10-kHz check for
apparently resistive low-impedance results after ratio-based range selection.
The original nominal 0.47-uH result was not resolved; additional search does not
qualify sub-microhenry capability or override contradictory phase evidence.

[Plan 24](24-Measurement-Evidence-Session-and-UI.md) implements trustworthy
classification/trend evidence, bounded hardware-owned K1 continuation and exact
incremental menus in PC-provisioned PRODUCT. Raw captures/calibration are
preserved; unattended validation cannot replace pending physical switching/UI
checks. Legacy full-wizard and fixed-capture profiles retain their safe paths.

[Plan 25](25-Trimmed-Measurement-Estimate.md) implements a bounded seven-sample
coordinate-wise trimmed complex estimate after autorange, preserving raw search
history, calibration provenance and the original K1/battery deadlines. Physical
follow-up to plan 24 now records two valid inductive results and an owner-confirmed
single K1 engagement/release across four captures; independent qualification remains pending.

[Plan 26](26-Three-Frequency-Product-Survey.md) integrates all three existing
frequencies before final factory PRODUCT interpretation, guarded SAFE/fresh-
permit segmentation for 10 Hz, twelve-record traceability and seven-point
primary estimates without increasing the fast continuous authorization.

[Plan 27](27-Single-Relay-Continuous-Survey.md) replaces factory segmentation
with one K1 engagement and final release across the three-frequency/seven-sample
survey, using real interleaved power sampling and unchanged evidence freshness.
Legacy captures retain their authorization limits; physical acceptance is
tracked separately from software tests.

[Plan 28](28-Serial-OK-Control.md) adds explicit bounded PRODUCT serial short-OK
control through the ordinary UI path for an owner-confirmed unchanged passive
DUT, preserving real permission, duplicate rejection and quiet acquisition.
Diagnostic profiles retain their existing protocols.

[Plan 29](29-Unit-Extended-Flash-Capacity.md) records the owner-authorized first-unit
upper-Flash experiment: complete patterns, reset persistence and execution near
128 KiB pass despite the 64-KiB factory register. Contents and calibrated SAFE
boot are restored; normal C8 build/programming limits remain 64 KiB.

[Plan 30](30-Production-Firmware-Audit-and-Completion.md) records a complete
production audit and ordered completion work: real asynchronous settings saves,
idle/backlight policy, audible feedback, fault/watchdog handling, consistent
production profiles, bounded UART diagnostics, explicit unit-only 128-KiB
deployment and native quick OPEN/SHORT lead compensation. The audit is complete;
the [correction work is now in progress](../docs/review/production-completion-2026-10-27/README.md).
The native menu is now implemented; final software checks and physical acceptance are tracked separately. Existing engineering OSL and
owner assets must be preserved, without another factory LOAD campaign.

Production completion: [sealed software image, memory and acceptance handoff](../Firmware/releases/2026-10-27/production-completion/README.md). Software checks passed; deployment and physical qualification are pending.

[Plan 31](31-Display-Throughput-and-Charging-UX.md) delivers glyph/window batching, normal charging navigation with measurement inhibition, and API 5 bilingual external resources while preserving owner artwork. The [current delivery](../Firmware/releases/2026-10-27/display-charging-ui/README.md) is programmed, fully readback verified and boots READY/SAFE. TFT appearance/speed and charging-menu acceptance remain REQUIRES_BENCH_VALIDATION.

[Plan 32](32-Product-Status-and-Backlight-Feedback.md) follows actual display feedback: About overflow, Celsius diagnostics, direct quick-lead menu discovery and measured backlight-command investigation. The original API 5 pack and splash remain unchanged; actual LED/driver brightness requires further bench evidence.
