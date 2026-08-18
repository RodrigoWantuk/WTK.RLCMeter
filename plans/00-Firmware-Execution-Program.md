# 00 — Firmware Execution Program

STATUS: IN_PROGRESS

This document defines the complete implementation program and the contract between phases. Detailed tasks live in the individual phase plans.

## Program objectives

Deliver Rev.1 firmware that is:

- safe by default;
- deterministic in acquisition;
- buildable through CMake without an IDE dependency;
- convenient to edit/debug from Visual Studio Code;
- testable on the host for pure logic;
- observable enough for hardware bring-up without breakpoints;
- calibrated/qualified rather than based only on nominal component values;
- modular enough for later hardware revisions without rewriting the metrology core.

## Global constraints

```text
Language: C17
Build: CMake
Target: STM32F103C8T6
Compiler: arm-none-eabi-gcc
MCU libraries: CMSIS + STM32CubeF1 HAL/LL
Editor: VS Code supported, editor-independent build
RTOS: none initially
Arduino: prohibited in baseline
C++: prohibited in baseline
Heap in critical path: prohibited
```

## Phase dependency graph

```text
01 Toolchain/CMake/VSCode
          |
          v
02 Platform/BSP/Diagnostics
          |
          +--------------------+
          |                    |
          v                    v
03 SPI/UI Peripherals      04 Safety/Range
          |                    |
          +----------+---------+
                     |
                     v
05 Excitation/ADC/DMA
                     |
                     v
06 DSP/Impedance
                     |
                     v
07 Autorange/Confidence/Calibration
                     |
                     v
08 Product Integration/UI/Storage
                     |
                     v
09 Bring-up/Qualification/Release
```

Phase 03 and part of Phase 04 can progress in parallel after Phase 02, provided they do not create conflicting BSP ownership.

## Cross-phase interfaces

The program should converge on stable boundaries early:

### BSP boundary

Provides MCU-level primitives and handles. No RLC policy.

### Hardware-services boundary

Provides safe semantic operations such as range selection, relay control, power/charger state, excitation configuration, and sensors.

### Acquisition boundary

Provides synchronized raw sample blocks plus explicit metadata.

### Measurement boundary

Consumes calibrated channel data and returns complex measurement results plus quality metadata.

### Storage boundary

Provides validated versioned records and assets by logical IDs.

### UI boundary

Consumes application state/results and emits user-intent events. It does not manipulate metrology hardware directly.

## Global deliverables

By the end of the program the repository should contain at least:

```text
Firmware/
├── CMakeLists.txt
├── cmake/
│   ├── toolchains/
│   └── modules/
├── src/
├── tests/
├── tools/
├── assets/
├── config/
└── third_party/
```

Plus root-level:

```text
CMakePresets.json or Firmware/CMakePresets.json (decision in Phase 01)
WTK.RLCMeter.code-workspace
.vscode/
AGENTS.md
plans/
```

## Global quality gates

Every phase must preserve:

- command-line buildability;
- warning policy;
- safe GPIO/relay defaults;
- dependency boundaries;
- English documentation;
- no accidental dynamic-allocation dependency in critical runtime code;
- no fake hardware qualification.

## Automated validation strategy

The program should eventually support at least:

```bash
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug

cmake --preset stm32-debug
cmake --build --preset stm32-debug
```

Exact preset names may differ if Phase 01 documents a better naming convention.

Future CI can run host tests and cross-compile the embedded target even without hardware attached.

## Bench-validation strategy

Hardware validation is staged so a failure cannot masquerade as a DSP bug:

1. supply rails and VMID;
2. UART/GPIO safe boot;
3. SPI/TFT/Flash/input peripherals;
4. residual sensing and relay/range controls;
5. excitation waveform;
6. ADC/DMA raw capture;
7. fixed known resistors;
8. capacitors/inductors;
9. OPEN/SHORT/LOAD calibration;
10. autorange and qualification matrix.

## Evidence requirements

For bench-sensitive phases, evidence should identify:

```text
hardware revision
assembled BOM / DNP state
firmware commit
build profile
bench supply settings
reference instrument / fixture
measured values
logs/screenshots/captures
pass/fail conclusion
```

## Phase handoff requirements

Each phase handoff answers:

1. What was implemented?
2. What exact commands build/test it?
3. What interfaces are now stable?
4. What remains hardware-unverified?
5. Did any assumption from the plan prove false?
6. What new risks were discovered?
7. Is the next phase unblocked?

## Architecture-change policy

The following require explicit documentation updates before or with implementation:

- changing firmware language;
- adding RTOS;
- introducing an external ADC;
- changing MCU;
- changing pinout;
- changing RREF values/switch technology;
- changing the SAFE/MEASURE concept;
- enabling native USB through hardware changes;
- changing the calibration model in a way that invalidates stored records;
- changing persistent formats incompatibly.

## End-state release artifacts

A Rev.1 firmware release should ultimately produce:

- `.elf` with symbols for debug/archive;
- `.bin` and/or `.hex` for programming;
- map/size report;
- version identifier tied to source commit;
- documented hardware revision compatibility;
- calibration schema version;
- qualification matrix;
- release notes listing known limitations.

## Immediate next action

The first implementation task is [`01-Toolchain-CMake-and-VSCode.md`](01-Toolchain-CMake-and-VSCode.md). No hardware feature should be implemented before that phase establishes the canonical project/build/test skeleton.
