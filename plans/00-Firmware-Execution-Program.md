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
- modular enough for later hardware revisions without rewriting the metrology core;
- simple to operate without forcing the user to select R/L/C before a normal measurement.

## Global constraints

```text
Language: C17
Build: CMake
Target: STM32F103C8T6
Compiler: arm-none-eabi-gcc
MCU libraries: CMSIS + STM32CubeF1 HAL/LL
Editor: VS Code supported, editor-independent build
RTOS: none initially
Runtime model: cooperative event-driven superloop + modular finite-state machines
Arduino: prohibited in baseline
C++: prohibited in baseline
Heap in critical path: prohibited
```

## Runtime architecture contract

Rev.1 does not use an RTOS. Runtime orchestration is based on small cooperative state machines by responsibility rather than one monolithic state switch.

Expected domains include:

```text
application/boot policy
measurement sequence
autorange/attempt control
calibration workflow
UI/navigation
button/debounce
storage operations
power/safety
```

Interrupts remain available for deterministic hardware events such as timers and ADC/DMA, but ISR code stays short and publishes bounded state/events. DSP, UI rendering, storage activity, and verbose logging run outside critical ISR paths.

Long waits are represented as states plus monotonic time/event checks. Blocking delay-driven application architecture is prohibited.

## Product UX contract

Normal operation is intentionally menu-light:

```text
short OK    start measurement
UP/DOWN     browse pages of the last result
long OK     open main menu
```

The normal product does not ask the user to select resistor/capacitor/inductor before measurement. The metrology core calculates complex impedance first; a later classifier interprets the dominant model automatically and may return low-confidence/mixed/unknown rather than forcing an incorrect label.

The main menu baseline is:

```text
Calibration
Display
Sound
Language
Debug
About
```

The primary result page uses large detected-component/value typography and small footer metadata for the excitation amplitude/frequency most directly associated with the displayed value. Additional pages expose electrical details, measurement metadata, useful graphs, and—when enabled—the on-screen debug console.

During multi-attempt/refined measurement, valid intermediate results may be shown with a clear waiting/progress indication. TFT/Flash updates occur only between critical acquisition windows.

## Calibration product gate

Calibration validity is mandatory product state.

Every boot validates the required persisted calibration set before normal READY operation. Missing, corrupt, incomplete, incompatible, or CRC-invalid calibration forces `CALIBRATION_REQUIRED` and the Calibration wizard. Normal measurement is unavailable until a valid required calibration set exists.

Manual recalibration must preserve the previous valid calibration until the candidate replacement has been fully written, read back, validated, and activated.

Calibration persistence uses the external W25Q storage architecture. The STM32F103C8T6 has no native EEPROM; plans and code must not describe this persistence as MCU internal EEPROM.

## External resource ROM contract

The W25Q is the external resource/data ROM as well as persistent storage. UI resource use includes rasterized fonts, large numeric glyphs, symbols, icons, localization resources, and optional graphics.

Desktop authoring fonts are converted offline; the MCU does not parse TTF/OTF. Runtime resource size must not cause proportional SRAM use: access is chunked through a small fixed scratch buffer, while a minimal emergency font remains in MCU internal Flash.

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
06 DSP/Impedance + basic model derivation
                     |
                     v
07 Autorange/Confidence/Classification/Calibration
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

Consumes calibrated channel data and returns complex measurement results plus quality metadata. Component/model classification consumes measured results; it does not alter the underlying impedance equation.

### Storage boundary

Provides validated versioned records and external resources by logical IDs.

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
- no fake hardware qualification;
- no unbounded UI/storage/logging work inside acquisition-critical windows;
- no hidden user setting capable of bypassing mandatory safety or calibration gates.

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

Host testing should eventually cover pure state-machine transitions, component/model classification, navigation/button semantics, calibration validity decisions, resource parsing, persistence corruption recovery, and the DSP core.

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
10. autorange/classification and qualification matrix;
11. integrated UI/result-progress/quiet-mode behavior.

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
- changing the cooperative state-machine execution model materially;
- introducing an external ADC;
- changing MCU;
- changing pinout;
- changing RREF values/switch technology;
- changing the SAFE/MEASURE concept;
- enabling native USB through hardware changes;
- changing the calibration model in a way that invalidates stored records;
- changing persistent formats incompatibly;
- removing automatic component/model classification from normal operation;
- changing the mandatory calibration boot gate.

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

Complete the current Phase 01 acceptance criteria before beginning hardware feature implementation. Subsequent agents must follow the phase plans and the contracts above rather than implementing later product/UI behavior opportunistically.
