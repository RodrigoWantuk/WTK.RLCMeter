# Consolidated Design Decisions

This document records decisions that have already been discussed so they are not reopened without new technical evidence.

## MCU

**STM32F103C8T6 Blue Pill** remains the Rev.1 MCU.

RP2040 was evaluated, but Rev.1 remains on STM32 because the ADC/timer architecture is already consolidated and changing MCU would introduce hardware and firmware churn before Rev.1 produces measurement data.

## Firmware language and build system

The firmware baseline is **C17 + CMake**.

Rationale:

- direct compatibility with CMSIS, STM32CubeF1 HAL/LL, and STM32 reference material;
- explicit memory/timing behavior on the resource-constrained STM32F103C8T6;
- simple host-side compilation of pure DSP/state-machine modules;
- no need for C++ runtime features, RTTI, exceptions, or Arduino abstraction layers;
- clean command-line and CI integration.

**C++ and Arduino/INO are not part of Rev.1 firmware architecture.** Reopening this decision requires a documented reason and migration impact analysis.

## Editor workflow

Visual Studio Code is the primary supported editor for firmware development.

The repository provides a checked-in workspace and recommended extensions, but VS Code is not part of the build contract: all important build/test operations must remain available through CMake from the command line.

STM32CubeIDE may be used as a debugger/reference tool but must not become the only way to build the project.

## ADC

No external ADC in Rev.1.

The two internal STM32 ADCs are used for acquisition. Final quality should first be pursued through deterministic timing, DSP, calibration, and range selection rather than introducing an expensive external converter before the prototype is characterized.

## Analog front-end

TLV9064 was considered originally, but sourcing constraints led to **2 × MCP6002-E/SN**.

Consequence: lower GBW and stronger dependence on complex gain/phase characterization, especially on the high-gain channel at 10 kHz.

## High-gain path

The current PCB/BOM uses:

```text
RF_HG = 68 kΩ
RG_HG = 4.7 kΩ
Gnom  = 1 + 68/4.7 ≈ 15.47×
```

Older ~8× discussions are historical and are not the current hardware specification.

## RREF switching

Low ranges: AO3400A.

High ranges: individual 2N7002 devices in SOT-23, two per range. This replaces smaller dual packages to simplify manual soldering/rework.

## Passives

0805 is the preferred minimum package for ordinary resistors/capacitors. 1206 is used where voltage, power, low impedance, or robustness justify it.

## Display

ILI9341 over SPI, without a full framebuffer.

The UI uses incremental rendering and external assets.

## External Flash

W25Q family over conventional SPI. The current BOM uses W25Q64JVSSIQ, but the driver should recognize compatible W25Q16/32/64/128 parts.

No filesystem in the first version. Assets use a simple packed format with table, offsets, dimensions/format, and CRC.

## User input

Three buttons: UP, DOWN, and OK. A rotary encoder was intentionally removed to simplify mechanics and hardware.

## Backlight and buzzer

PB0 controls TFT backlight PWM.

PB1 controls an external passive piezo through BC817. The piezo is located in the enclosure rather than on the analog PCB.

## Safety

K1 is fail-safe and the residual-voltage detector sits in front of the measurement AFE.

`D_TVS` and `R_TVS_LINK` remain DNP initially.

The current goal is to detect/tolerate residual voltage in the approximate ±100 V observation envelope, not to measure energized high voltage.

## High voltage as a future feature

Direct AC measurement around 400 Vrms and DC measurement around 600–800 V have been discussed as future capabilities, but they are **not part of Rev.1**. A future implementation requires a dedicated front-end, connectors, protection, clearance/creepage analysis, and a new safety review.

## 4-wire measurement

Rev.1 is two-wire. Kelvin/4-wire remains a future-revision possibility.

## Debug and programming

A dedicated SWD connector is not required on the carrier board because SWD remains available on the Blue Pill module.

Native USB device mode is unavailable in Rev.1 because PA11/PA12 are reused. UART and SWD remain the primary bring-up interfaces.

## K2 / low-Z bank

K2 is a contingency option to isolate the low-Z bank if parasitic capacitance becomes problematic at high impedance.

Baseline population:

```text
R0_BANK = 0 Ω
K2      = DNP
```

This should change only if measured leakage/parasitic evidence justifies it.

## Decision-change rule

Any agent or contributor proposing to reverse a consolidated decision must document:

1. new evidence or requirement;
2. hardware impact;
3. firmware impact;
4. calibration/qualification impact;
5. migration cost;
6. affected documentation/plans.
