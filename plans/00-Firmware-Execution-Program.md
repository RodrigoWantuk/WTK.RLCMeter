# 00 — Firmware Execution Program

STATUS: IN_PROGRESS

This document defines the complete implementation program and the contract between phases. Detailed tasks live in the individual phase plans.

## Current program state

```text
Phase 03A:
    implementation ready; real Wokwi execution remains pending external simulator/token constraints

Phase 04:
    IMPLEMENTED_REQUIRES_BENCH_VALIDATION

Phase 05:
    Stage 1 and Stage 2 implemented; excitation, ADC/DMA transport, and K1 measurement
    sequencing remain REQUIRES_BENCH_VALIDATION

Phase 06:
    software DSP/impedance core implemented with synthetic host validation;
    physical measurement comparison remains REQUIRES_BENCH_VALIDATION

Phase 07:
    Stage 1A automatic measurement policy implemented; Stage 1B end-to-end automatic
    session orchestration implemented with synthetic host validation; Stage 2A
    calibration data model/resolver/redundant persistence substrate implemented; Stage
    2B.1 OPEN-SHORT-LOAD evidence acquisition implemented; Stage 2B.2 OSL/Mobius
    coefficient solving and transactional candidate activation implemented; Stage
    2B.2.1 calibration lifecycle/HG canonicalization hardening implemented; physical
    qualification remains
    REQUIRES_BENCH_VALIDATION

Immediate next software phase after physical/software handoff:
    Phase 08 continuation — Stage 3A.1 Resource Pack v2 integrity,
    localization completion, runtime resource-error propagation, and PRODUCT
    UI SRAM hardening are implemented. Stage 3B external fonts/icons/splash
    remains blocked until additional internal-Flash headroom is recovered
    below the Stage 3A.1 minimum target.
```

## Program objectives

Deliver Rev.1 firmware that is:

- safe by default;
- deterministic in acquisition;
- buildable through CMake without an IDE dependency;
- convenient to edit/debug from Visual Studio Code;
- testable on the host for pure logic;
- testable against virtual STM32 hardware for digital integration regressions;
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
          |
          v
03A Virtual Hardware Validation
          |
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

Phase 03A is an orthogonal virtual-hardware validation layer. Establish it after the Phase 02/03 digital foundation and reuse it during later phases. It may validate digital policy and peripheral behavior, but it does not replace hardware-dependent bench criteria.

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
├── sim/
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
.github/workflows/ when CI is enabled
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

The project uses three distinct evidence layers before final bench qualification:

```text
1. Host tests
   pure C logic/state/policy/math

2. Virtual hardware tests
   real STM32 firmware artifact executing against simulated digital hardware

3. Physical bench validation
   actual Rev.1 board/electrical behavior/metrology
```

Canonical host and embedded build validation includes:

```bash
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug

cmake --preset stm32-debug
cmake --build --preset stm32-debug
```

The virtual-hardware layer is defined by [`03A-Wokwi-Virtual-Hardware-Validation.md`](03A-Wokwi-Virtual-Hardware-Validation.md). It initially uses Wokwi to execute the existing STM32 Bringup ELF and automate digital GPIO/UART/SPI/TFT/Flash regressions where the simulator supports them.

Host testing should eventually cover pure state-machine transitions, component/model classification, navigation/button semantics, calibration validity decisions, resource parsing, persistence corruption recovery, and the DSP core.

Virtual hardware testing should cover externally observable digital integration behavior such as safe boot GPIOs, UART diagnostics, buttons, SPI ownership, display/Flash smoke behavior, PWM/timer outputs, and later digital safety-policy scenarios.

Virtual tests must never be reported as electrical or metrology qualification. Simulator limitations remain explicit, and unsupported ADC2/DMA/IWDG behavior stays host/bench validated as appropriate.

## Evidence classification

Handoffs and validation reports should distinguish:

```text
HOST_TESTED
VIRTUAL_HARDWARE_TESTED
REQUIRES_BENCH_VALIDATION
BENCH_VALIDATED
```

A result may legitimately be both `VIRTUAL_HARDWARE_TESTED` and `REQUIRES_BENCH_VALIDATION`.

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

Virtual evidence should be collected before the corresponding bench step when practical, but no virtual result removes the physical bench gate.

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

For virtual-hardware evidence, record at least:

```text
firmware commit
build profile
simulator/backend version
scenario/test name
simulator limitations relevant to the test
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

Changing the primary virtual-hardware simulator is not itself a firmware architecture change, but the replacement must preserve the evidence boundary between host, virtual, and bench validation.

## End-state release artifacts

A Rev.1 firmware release should ultimately produce:

- `.elf` with symbols for debug/archive;
- `.bin` and/or `.hex` for programming;
- map/size report;
- version identifier tied to source commit;
- documented hardware revision compatibility;
- calibration schema version;
- qualification matrix;
- release notes listing known limitations;
- host and virtual regression results associated with the release candidate.

## Immediate next action

Continue Phase 08 after Stage 1.1. The product profile now has a mandatory calibration
gate, click-measurement integration through Phase 05/06/07, a minimal cooperative TFT
fallback UI, compact product view-model snapshots, shared metrology/storage scratch
ownership, PRODUCT/BRINGUP RAM gates, mandatory STM32 size gates, and linked-symbol
profile-composition checks. Remaining Phase 08 work includes the calibration wizard UI,
settings persistence, external resources/localization, menus, graph/detail pages beyond
the initial primary/details pair, and bench validation. Phase 03A virtual execution
remains pending external Wokwi constraints and must not be treated as bench validation
for analog/metrology behavior.
