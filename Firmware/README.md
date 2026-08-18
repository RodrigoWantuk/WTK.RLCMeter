# Firmware

Firmware for WTK.RLCMeter, targeting the **STM32F103C8T6 / Blue Pill**.

This directory contains the firmware architecture and, progressively, the implementation of the instrument. The design goal is deterministic acquisition, fail-safe hardware control, and a responsive UI without coupling measurement algorithms to device-specific peripherals.

## Canonical development stack

- **Language:** C17.
- **Build system:** CMake.
- **Embedded compiler:** GNU Arm Embedded / `arm-none-eabi-gcc`.
- **MCU support:** CMSIS + STM32CubeF1 HAL/LL.
- **Editor:** Visual Studio Code is the primary supported workflow.
- **IDE independence:** builds must remain reproducible from the command line.
- **RTOS:** none initially.
- **Arduino:** not used; no `.ino` source files and no Arduino framework dependency.
- **Critical-path memory:** no dynamic allocation in acquisition/DSP-critical paths.
- **Testing:** host-side tests for pure code wherever practical.

STM32CubeIDE may be used as a debugger, peripheral-reference tool, or code-generation aid during bring-up, but its project metadata must not become the canonical build definition.

## Phase 01 build foundation

The canonical CMake source directory is this `Firmware/` directory. Presets are also kept here so command-line and VS Code workflows use the same project root.

Prerequisites:

- CMake 3.25 or newer;
- a host C compiler for host tests;
- GNU Arm Embedded toolchain on `PATH` for STM32 builds:
  - `arm-none-eabi-gcc`;
  - `arm-none-eabi-objcopy`;
  - `arm-none-eabi-size`.

Host build and tests:

```bash
cd Firmware
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

Embedded debug build:

```bash
cd Firmware
cmake --preset stm32-debug
cmake --build --preset stm32-debug
```

Embedded release build:

```bash
cd Firmware
cmake --preset stm32-release
cmake --build --preset stm32-release
```

The STM32 presets use `cmake/toolchains/arm-none-eabi-gcc.cmake` and target Cortex-M3 Thumb code for the STM32F103C8T6. The linker script is `cmake/stm32/STM32F103C8Tx_FLASH.ld`, with the Blue Pill baseline memory map of 64 KiB Flash and 20 KiB RAM.

Expected STM32 build artifacts are generated under the selected build directory:

```text
WTK.RLCMeter.elf
WTK.RLCMeter.bin
WTK.RLCMeter.hex
WTK.RLCMeter.map
```

Host tests currently use CTest with small C executables. No C++ test framework is required.

Warnings for project-owned C code are centralized in `cmake/modules/CompilerWarnings.cmake`. Host targets treat warnings as errors by default. STM32 warning-as-error is available through `WTK_WARNINGS_AS_ERRORS_STM32` but is off initially so vendor/header boundaries can be validated before enforcing it.

## STM32CubeF1 / CMSIS strategy

The firmware is prepared for STM32CubeF1 CMSIS/HAL/LL integration without depending on STM32CubeIDE-generated project files. Phase 01 records `STM32CubeF1` tag `v1.8.6` as the intended pinned upstream baseline.

When the package is vendored or checked out, configure STM32 builds with:

```bash
cmake --preset stm32-debug -DWTK_STM32CUBEF1_ROOT=<path-to-STM32CubeF1>
```

Phase 01's minimal embedded link-smoke target does not initialize peripherals and does not require HAL sources. Phase 02 is responsible for using this integration point when real BSP, safe GPIO, clock, UART, and watchdog code is added.

## VS Code workflow

Open the repository using:

```text
WTK.RLCMeter.code-workspace
```

Recommended extensions are versioned under `.vscode/extensions.json`. CMake Tools should use `Firmware/` as the CMake source directory once Phase 01 creates the build files.

The repository workspace is intentionally lightweight: developers and agents must still be able to configure/build through CMake from a normal terminal.

## Planned structure

```text
Firmware/
├── README.md
├── assets/              # source images/fonts before packing
├── config/              # versioned defaults and feature flags
├── src/
│   ├── app/             # state machine, orchestration, global policy
│   ├── bsp/             # STM32 clock/GPIO/ADC/DMA/timer/SPI/UART binding
│   ├── drivers/         # ILI9341, W25Q, buttons, device drivers
│   ├── hardware/        # safe instrument hardware services
│   ├── measurement/     # acquisition, DSP, impedance, autorange, confidence
│   ├── storage/         # assets, settings, calibration persistence
│   └── ui/              # screens, widgets, formatting, navigation
├── tests/               # host-side tests and known vectors
├── third_party/         # isolated external dependencies
└── tools/               # asset/calibration/log tooling
```

Each major module has its own README describing responsibilities and dependency boundaries.

## Dependency rules

```text
app
 ├── hardware
 ├── measurement
 ├── storage
 └── ui

hardware -> bsp
measurement -> acquisition abstraction + pure types
storage -> drivers/W25Q
ui -> drivers/ILI9341 + storage/assets

drivers -> bsp
bsp -> CMSIS/HAL/LL
```

Mandatory boundaries:

- `measurement` must not include ILI9341, W25Q, or GPIO headers;
- `ui` must not directly drive K1, K2, or `RANGE_EN`;
- only `hardware` implements safe relay/range sequences;
- only `bsp` owns STM32 pin-mux, register, and HAL/LL details;
- ISRs move or signal data and remain short;
- rendering, storage operations, and DSP execute outside interrupt context;
- every transition capable of connecting the DUT to the AFE has an explicit SAFE abort path.

## Cooperative scheduler

No RTOS is planned for the first implementation. The application uses short non-blocking steps:

```c
while (1)
{
    safety_poll();
    input_poll();
    app_step();
    measurement_step();
    ui_step();
    storage_step();
    diagnostics_step();
    watchdog_service();
}
```

Long operations such as Flash erase/program, animations, and large TFT updates must be decomposed into states or chunks rather than implemented as blocking waits.

## Interrupt usage

Planned interrupt responsibilities:

- ADC trigger timing;
- DMA half/full completion;
- UART RX where required;
- buzzer timebase if implemented through timed GPIO toggling;
- SysTick only as a low-resolution system tick, not as the metrology sampling source.

No complex transformation, screen rendering, storage transaction, or autorange decision runs inside an ISR.

## Timer baseline

- **TIM1_CH1 / PA8** — `PWM_EXC` carrier.
- **TIM2** — candidate deterministic acquisition trigger.
- **TIM3_CH3 / PB0** — continuous TFT backlight PWM.
- **PB1** — buzzer output; although PB1 is TIM3_CH4, buzzer and backlight cannot have independent base frequencies when sharing TIM3 ARR/prescaler.
- **TIM4** — candidate buzzer timebase to toggle PB1 in software while preserving independent backlight PWM frequency.

The exact timer/clock plan must be frozen during Phase 02/05 after validating the STM32 clock tree and acquisition requirements.

## Safe boot state

Before TFT, external Flash, or persisted settings are initialized:

```text
RANGE_EN = 0
K1_CMD    = 0
K2_CMD    = 0
BUZZER    = off
TFT_CS    = 1
FLASH_CS  = 1
PWM_EXC   = neutral/off
```

Then:

1. configure clocks and watchdog;
2. establish safe GPIO defaults;
3. disable JTAG while preserving SWD, freeing PA15/PB3/PB4;
4. start diagnostic UART;
5. initialize SPI, W25Q, and ILI9341;
6. initialize ADC/DMA/timers;
7. validate persisted configuration/calibration;
8. run self-test;
9. enter `SAFE_CHECK`.

A TFT or Flash failure must never authorize a measurement that safety logic would otherwise block.

## Instrument state machine

```text
BOOT
  |
  v
SELF_TEST
  |
  v
SAFE_CHECK <------------------------------+
  |                                       |
  +-- residual/charger/fault --> WAIT ----+
  |
  v
READY
  |
  v
PREPARE_RANGE
  |
  v
PRE_EXCITATION
  |
  v
K1_MEASURE
  |
  v
SETTLING
  |
  v
ACQUIRE
  |
  v
K1_SAFE
  |
  v
PROCESS
  |
  +--> RETRY / RERANGE
  |
  v
RESULT
```

The DUT should remain connected to the analog measurement path only for the time required to settle and acquire the requested data. Heavy processing and rendering should preferably occur after K1 returns to SAFE.

## Acquisition and DSP

Planned flow:

1. configure excitation frequency and amplitude;
2. select RREF with `RANGE_EN=0` during switching;
3. enforce dead-time and settling;
4. energize K1 only after all safety gates pass;
5. timer-trigger ADC sampling deterministically;
6. transfer blocks through DMA;
7. calculate I/Q components or an equivalent single-bin DFT outside the ISR;
8. form calibrated complex phasors for VEXC, VMID, and RET;
9. compute complex impedance;
10. apply confidence gates and rerange/retry if needed;
11. return K1 to SAFE before heavy UI work.

The current `RET_HG` hardware has nominal gain:

```text
1 + 68 kΩ / 4.7 kΩ ≈ 15.47×
```

Firmware must use a calibrated complex response rather than treating 15.47 as an exact frequency-independent gain.

## Autorange

Range decisions consider more than estimated DUT magnitude:

- `RET_1X` and `RET_HG` clipping;
- SNR;
- allowed excitation current;
- analog headroom;
- frequency;
- amplitude;
- qualified range/frequency/amplitude combinations;
- proximity to OPEN/SHORT behavior.

Safe switch sequence:

```text
RANGE_EN=0
set A0/A1/A2
wait dead-time
RANGE_EN=1
wait settling
```

The 10 Ω reference range must not use 500 mVrms excitation.

## Quiet mode

During critical acquisition windows:

- buzzer is off;
- large TFT writes are suspended;
- unnecessary W25Q reads/program/erase operations are suspended;
- high-volume UART logging is suspended;
- backlight duty remains stable; if bench testing shows measurable coupling, it may be frozen to a qualified condition.

## Persistence

No filesystem is planned initially. W25Q is logically divided into:

- asset pack;
- calibration records;
- settings;
- optional diagnostic/event data.

Persistent records should include at least:

```text
magic
schema_version
hardware_revision
sequence
payload_length
crc32
payload
```

Settings/calibration should use redundant slots or a small journal strategy to tolerate power loss during updates.

## Assets

A full 240×320 RGB565 framebuffer consumes 153.6 kB, exceeding Blue Pill RAM. Therefore:

- there is no full-screen framebuffer;
- rendering is incremental;
- large bitmaps are streamed from W25Q in small blocks;
- assets may be preconverted to RGB565, simple RLE, or compact masks;
- TFT and Flash share SPI with independent CS lines.

## Diagnostics

Compact log levels:

```text
ERROR
WARN
INFO
DEBUG
TRACE   # laboratory builds only
```

Bring-up diagnostics should expose through UART and/or TFT:

- application state;
- raw and converted ADC values;
- active range/RREF;
- K1/K2 state;
- `CHG_VBUS`;
- battery and NTC;
- W25Q JEDEC ID/status;
- TFT status;
- clipping/SNR/confidence;
- fault codes.

## Planned build profiles

- `Debug` — assertions, full symbols, development logging.
- `Lab` — extra instrumentation, TRACE, engineering screens.
- `Release` — product behavior with reduced logging.
- `HostTests` — pure modules compiled for the development host without STM32 dependencies.

## Implementation program

Firmware implementation is governed by [`../AGENTS.md`](../AGENTS.md) and the plans under [`../plans`](../plans).

The intended sequence is:

1. toolchain, CMake, VS Code integration, and host-test harness;
2. platform/BSP, safe GPIO, UART, watchdog, and timing foundation;
3. SPI, W25Q, ILI9341, buttons, backlight, and buzzer;
4. safety sensing, power monitoring, K1/K2, and range control;
5. PWM excitation, ADC1/ADC2, timer trigger, and DMA;
6. phasor extraction and complex impedance calculation;
7. autorange, confidence, and calibration persistence;
8. final UI, asset pack, settings, and product-level diagnostics;
9. hardware bring-up and metrology qualification.

See also:

- [`../docs/04-Firmware-Architecture.md`](../docs/04-Firmware-Architecture.md)
- [`../docs/13-Detailed-Firmware-Design.md`](../docs/13-Detailed-Firmware-Design.md)
- [`../plans/README.md`](../plans/README.md)
