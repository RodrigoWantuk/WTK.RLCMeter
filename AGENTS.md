# AGENTS.md

This file defines the operating contract for AI coding agents working on WTK.RLCMeter.

The project is measurement-instrument firmware with safety-sensitive hardware behavior. Agents must optimize for **correctness, traceability, deterministic behavior, testability, and preservation of fail-safe assumptions**, not for speed of feature accumulation.

## 1. Mandatory reading order

Before changing firmware, read at least:

1. `README.md`;
2. `docs/README.md`;
3. `docs/03-Safety-and-Protection.md`;
4. `docs/04-Firmware-Architecture.md`;
5. `docs/05-Pinout-and-Interfaces.md`;
6. `docs/10-Consolidated-Design-Decisions.md`;
7. `docs/13-Detailed-Firmware-Design.md`;
8. this file;
9. `plans/README.md`;
10. the specific execution plan assigned to the task.

For metrology/DSP work also read:

- `docs/02-Measurement-Model-and-DSP.md`;
- `docs/07-Calibration-and-Validation.md`.

For UI/storage work also read:

- `docs/06-UI-UX-and-Diagnostics.md`.

For board bring-up or safety validation also read:

- `docs/01-Hardware-Architecture.md`;
- `docs/09-Rev1-Bringup.md`;
- `docs/11-Rev1-BOM-and-Assembly.md`.

## 2. Canonical technical baseline

Do not silently change these decisions:

```text
Language:              C17
Build system:          CMake
Embedded compiler:     arm-none-eabi-gcc
MCU:                   STM32F103C8T6 / Blue Pill
MCU support:           CMSIS + STM32CubeF1 HAL/LL
Primary editor:        Visual Studio Code
Canonical build:       command-line CMake
RTOS:                  none initially
Arduino / INO:         not used
C++:                   not used
Dynamic allocation:    prohibited in acquisition-critical paths
Native USB Rev.1:      unavailable
Measurement topology:  two-wire
```

Changing one of these is an architecture change. Do not perform it as incidental refactoring.

## 3. Documentation language

All repository documentation must be written in **English**.

This includes:

- Markdown files;
- agent plans;
- durable architecture comments;
- README files;
- contributor-facing instructions;
- build documentation;
- user-facing diagnostic strings when practical.

Short internal identifiers remain conventional C/engineering English.

## 4. Source-code style

Use C17, not C++.

Prefer:

- explicit structs and enums;
- fixed-width integer types when hardware/storage width matters;
- `bool` for logical state;
- `const` wherever useful;
- small translation units with narrow public headers;
- explicit ownership of buffers/resources;
- static allocation for embedded runtime data;
- return values or explicit status enums for recoverable errors;
- assertions only for programmer invariants, not for expected hardware failures.

Avoid:

- hidden global state when a context object is practical;
- blocking delays in application/UI/storage logic;
- large functions mixing policy, hardware access, and math;
- raw register access outside BSP unless the architecture explicitly documents the exception;
- macros when an enum/static inline/function is clearer;
- serialization by blindly writing C structs to Flash.

## 5. Safety invariants

These rules are non-negotiable unless the hardware architecture itself changes:

1. K1 de-energized means SAFE.
2. Boot/reset/fault must not accidentally energize K1.
3. `RANGE_EN` is disabled during range-address transitions.
4. Measurement is blocked while charger/interlock conditions are active.
5. Residual-voltage checks happen before MEASURE.
6. Fault handling stops excitation and returns the instrument toward SAFE.
7. UI code cannot directly energize relays or select range GPIOs.
8. Failure of TFT, external Flash, settings, or assets cannot disable safety checks.
9. The residual-voltage network is not a license to measure energized mains/high voltage.
10. No agent may claim CAT rating or high-voltage measurement capability for Rev.1.

If an implementation choice creates ambiguity around these invariants, stop and surface the ambiguity in the handoff rather than guessing.

## 6. Hardware source of truth

Use the following precedence:

1. latest PCB/BOM intended for assembly;
2. matching schematic;
3. current docs;
4. historical revision notes;
5. old discussion notes.

Current important values include:

```text
RREF: 10 Ω / 100 Ω / 1 kΩ / 10 kΩ / 100 kΩ / 1 MΩ
RET_HG nominal: 1 + 68k/4.7k ≈ 15.47×
Display: ILI9341 SPI
Flash baseline: W25Q64JVSSIQ
Backlight: PB0
Buzzer: PB1
Charger detect: PA15
SWD: PA13/PA14
```

Do not substitute historical ~8× high-gain assumptions into current firmware.

## 7. Dependency boundaries

Target dependency direction:

```text
app
 ├── hardware
 ├── measurement
 ├── storage
 └── ui

hardware -> bsp
measurement -> pure types + acquisition abstraction
storage -> drivers/W25Q
ui -> drivers/ILI9341 + storage/assets

drivers -> bsp
bsp -> CMSIS/HAL/LL
```

Specific prohibitions:

- `measurement` must not include TFT or W25Q headers;
- `ui` must not include low-level relay/range GPIO control;
- `drivers` must not own application policy;
- `bsp` must not know RLC measurement equations;
- calibration math must not depend on a screen implementation;
- host-testable code must not acquire accidental HAL dependencies.

## 8. Build-system contract

CMake is the canonical build system.

The first execution phase must establish:

- top-level firmware `CMakeLists.txt`;
- Arm toolchain file;
- CMake presets;
- host-test build path;
- generated `compile_commands.json` where practical;
- documented configure/build/test commands;
- VS Code CMake Tools compatibility.

Do not add Makefile-only, CubeIDE-only, PlatformIO-only, Arduino CLI, or vendor-project-only build flows as canonical alternatives.

## 9. Visual Studio Code contract

VS Code is the primary supported editor, not a build dependency.

The repository may contain:

- `WTK.RLCMeter.code-workspace`;
- `.vscode/extensions.json`;
- `.vscode/settings.json`;
- optional debug configurations once the chosen SWD/debug flow is implemented.

Workspace settings should point CMake Tools at `Firmware/` and use its compile database/configuration provider.

Do not hard-code developer-specific absolute paths.

## 10. Testing policy

### Host-side tests

Pure modules should be tested on the host whenever practical:

- complex math;
- phasor extraction;
- impedance equation;
- equivalent R/C/L derivation;
- calibration application;
- autorange;
- confidence;
- persistent-record parsing/CRC;
- asset manifest parsing;
- pure state-machine transitions.

### Embedded tests

Hardware-facing modules need diagnostic or bring-up tests appropriate to the actual board:

- GPIO safe defaults;
- UART;
- SPI ownership;
- W25Q JEDEC/read/write;
- ILI9341 ID/init/rendering;
- buttons;
- backlight/buzzer;
- residual ADCs;
- K1/K2/range switching;
- excitation;
- ADC/DMA timing.

### Never fake hardware qualification

An agent may implement code and synthetic tests, but must not report real-world accuracy, SNR, phase error, leakage, relay behavior, or safety thresholds as validated unless bench evidence exists.

Mark such items `REQUIRES_BENCH_VALIDATION` in the handoff/plan status.

## 11. Commit/PR scope for agents

Prefer one execution phase or clearly bounded sub-phase per change set.

Before editing:

1. identify the active plan;
2. identify prerequisites;
3. inspect existing implementation;
4. list expected files;
5. verify no newer decision contradicts the plan.

During implementation:

- do not opportunistically implement later phases;
- avoid unrelated formatting/refactors;
- preserve public contracts unless the plan explicitly changes them;
- add tests together with pure logic;
- update docs when public behavior/contracts change.

At completion:

- run all available relevant checks;
- summarize files changed;
- summarize tests run and results;
- identify hardware checks that remain;
- identify any plan assumptions proven wrong;
- update the plan status only when the acceptance criteria are actually met.

## 12. Definition of done

A firmware task is not complete merely because it compiles.

Depending on the phase, completion may require:

- embedded target builds;
- host tests;
- static warnings clean enough for the configured policy;
- documented API contract;
- deterministic failure behavior;
- diagnostic observability;
- no architecture-boundary violations;
- bench-validation checklist prepared or executed.

The phase plan defines the exact acceptance criteria.

## 13. Compiler-warning policy

Phase 01 should establish a strict warning baseline appropriate to GCC, for example a curated subset around:

```text
-Wall
-Wextra
-Wpedantic
-Wshadow
-Wconversion
-Wsign-conversion
-Wdouble-promotion
-Wformat=2
-Wundef
```

Warnings must be chosen pragmatically for STM32/vendor headers. Do not globally disable useful warnings to silence third-party code; isolate vendor/third-party warning policy where possible.

Whether `-Werror` is enabled globally should be decided during Phase 01 after validating vendor-header behavior. Project-owned host-test code should aim for warning-clean builds.

## 14. Memory policy

STM32F103C8T6 memory is constrained.

Agents must:

- avoid full-screen framebuffers;
- avoid unbounded queues;
- prefer fixed-size buffers;
- document large static allocations;
- inspect linker/map output when significant buffers are added;
- keep acquisition buffers deliberately sized;
- avoid heap dependency in runtime-critical code.

Large graphics assets belong in W25Q and are streamed in blocks.

## 15. ISR and concurrency policy

ISRs should:

- acknowledge hardware;
- move/store minimal data;
- update bounded counters/flags;
- signal that work is ready.

ISRs should not:

- render UI;
- erase/program Flash;
- perform complex DSP;
- run autorange policy;
- call blocking HAL operations;
- log formatted strings at high volume.

Shared ISR/main-loop data must use clear ownership and appropriate `volatile`/critical-section semantics. Do not apply `volatile` as a substitute for synchronization reasoning.

## 16. Persistent-data policy

Settings/calibration/asset metadata must be versioned.

Do not persist raw compiler-dependent structs as the only format. Define field widths, bounds, endianness assumptions, version, and CRC behavior.

Power-loss behavior must be designed explicitly, using redundant slots or a journal-like scheme where appropriate.

## 17. Measurement math policy

The central equation is:

```text
Vs = VEXC - VMID
Vx = RET  - VMID
Zx = ZREF * Vx / (Vs - Vx)
```

The implementation must operate on calibrated complex quantities.

Do not:

- reduce the design to scalar RMS-only math;
- assume nominal 15.47× HG gain is exact;
- assume RREF is purely nominal/ideal at all frequencies;
- publish derived L/C when the reactance sign/quality does not support the model;
- hide singular/near-OPEN/near-SHORT behavior.

## 18. Autorange policy

Autorange must consider:

- RREF;
- RET 1X/HG clipping;
- SNR;
- excitation current/headroom;
- frequency;
- amplitude;
- settling;
- qualification map;
- proximity to OPEN/SHORT.

Never switch the range address while `RANGE_EN` is active.

## 19. Quiet-mode policy

Critical acquisition windows may suspend:

- buzzer output;
- large TFT transfers;
- external-Flash transactions;
- verbose UART logging;
- nonessential state transitions.

Backlight PWM should remain stable unless qualification demonstrates a better controlled condition.

## 20. No silent architecture invention

If an implementation requires a decision not already defined—examples: ADC sample rate, exact DMA topology, HAL vs LL for a critical peripheral, Flash partition addresses, test framework, SWD debug adapter configuration—do one of the following:

1. choose a conservative implementation explicitly allowed by the active plan and document it; or
2. record the unresolved decision and stop that subtask.

Do not hide a consequential architectural choice inside a helper function or build script.

## 21. Execution plans

The plans under `plans/` are ordered. A later phase may depend on interfaces created by earlier phases.

The expected program is:

```text
01 Toolchain / CMake / VS Code / host tests
02 Platform BSP / safe boot / UART / watchdog
03 SPI / W25Q / ILI9341 / buttons / backlight / buzzer
04 Safety / power sensing / K1-K2 / range control
05 Excitation / ADC1-ADC2 / timer trigger / DMA
06 DSP / phasors / impedance core
07 Autorange / confidence / calibration
08 UI / storage / product integration
09 Hardware bring-up / qualification / release criteria
```

Agents should begin at the earliest incomplete prerequisite phase unless explicitly assigned a later isolated task that does not violate dependencies.
