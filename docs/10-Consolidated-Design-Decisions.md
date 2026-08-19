# Consolidated Design Decisions

This document records decisions that have already been discussed so they are not reopened without new technical evidence.

## MCU

**STM32F103C8T6 Blue Pill** remains the Rev.1 MCU.

RP2040 was evaluated, but Rev.1 remains on STM32 because the ADC/timer architecture is already consolidated and changing MCU would introduce hardware and firmware churn before Rev.1 produces measurement data.

The firmware must fit the guaranteed STM32F103C8T6 baseline of 64 KiB internal Flash and 20 KiB SRAM. Do not rely on clone-specific or non-guaranteed extra Flash.

## Firmware language and build system

The firmware baseline is **C17 + CMake**.

Rationale:

- direct compatibility with CMSIS, STM32CubeF1 HAL/LL, and STM32 reference material;
- explicit memory/timing behavior on the resource-constrained STM32F103C8T6;
- simple host-side compilation of pure DSP/state-machine modules;
- no need for C++ runtime features, RTTI, exceptions, or Arduino abstraction layers;
- clean command-line and CI integration.

**C++ and Arduino/INO are not part of Rev.1 firmware architecture.** Reopening this decision requires a documented reason and migration impact analysis.

## Runtime architecture

Rev.1 uses **no RTOS**.

The runtime baseline is an event-driven cooperative superloop with modular finite-state machines by responsibility. Expected domains include application/boot policy, measurement sequencing, autorange/refinement, calibration, UI/navigation, buttons, storage, and power/safety.

Long operations use states/events/timeouts rather than blocking delays. ISR code is short and bounded; heavy DSP, UI, storage, and verbose logging remain outside critical ISR paths.

A future RTOS proposal requires explicit evidence that the cooperative architecture is insufficient and must include RAM, timing, complexity, and migration impact.

## Clock and platform baseline

The Phase 02 STM32 platform baseline expects the Blue Pill 8 MHz HSE and configures:

```text
SYSCLK:      72 MHz via HSE PLL x9
HCLK/AHB:    72 MHz
PCLK1/APB1:  36 MHz
PCLK2/APB2:  72 MHz
APB1 timers: 72 MHz
APB2 timers: 72 MHz
ADC clock:   12 MHz via PCLK2 / 6
SysTick:     1 kHz low-resolution cooperative timebase
```

The SysTick timebase is for diagnostics, debounce, watchdog-friendly timeouts, and cooperative state machines. It is not the metrology sample clock.

If HSE/PLL startup fails, firmware remains in the safe GPIO state on HSI and reports the clock fault. Later phases must not assume the 72 MHz clock was achieved without checking the BSP clock status/summary.

The independent watchdog starts after safe GPIO, clock/timebase setup, USART1 initialization, and the boot diagnostics banner. The cooperative shell services it each loop; future long operations must be decomposed so watchdog service remains possible.

## STM32CubeF1 dependency strategy

Firmware integrates CMSIS and STM32CubeF1 HAL/LL through CMake and official ST component submodules, not through STM32CubeIDE project metadata or developer-local package paths.

The selected compatibility baseline is `STM32CubeF1` `v1.8.7`. Instead of checking in the full monolithic package and unrelated middleware/BSPs, the repository pins only the required official components under `Firmware/third_party/st/`:

```text
cmsis_core             afc5ca6af0a49232fde7eb4548dd0962d119ce14
cmsis_device_f1        c8e9a4a4f16b6d2cb2a2083cbe5161025280fb22
stm32f1xx_hal_driver   fee494a92b5ad331f92ad21f76c66a5cb83773ee
```

A fresh clone is made reproducible with:

```bash
git submodule update --init --recursive
```

The CMake integration requires the STM32F103xB device definition and exposes CMSIS Core, CMSIS Device F1, and STM32F1 HAL/LL headers plus the HAL/LL source directory for Phase 02. The minimal Phase 01 link-smoke target intentionally does not compile HAL peripheral sources.

The Phase 01 `SystemInit()` / `SystemCoreClock` file is a temporary link-smoke stub. Phase 02 must replace it cleanly when real clock/platform initialization is implemented, without duplicate CMSIS system symbols.

The linker script enforces the STM32F103C8T6 project baseline of 64 KiB Flash and 20 KiB SRAM. Official STM32F103xB templates may describe 128 KiB Flash for the broader xB family; WTK.RLCMeter must not rely on clone-specific extra Flash.

## Editor workflow

Visual Studio Code is the primary supported editor for firmware development.

The repository provides a checked-in workspace and recommended extensions, but VS Code is not part of the build contract: all important build/test operations must remain available through CMake from the command line.

STM32CubeIDE may be used as a debugger/reference tool but must not become the only way to build the project.

## ADC

No external ADC in Rev.1.

The two internal STM32 ADCs are used for acquisition. Final quality should first be pursued through deterministic timing, DSP, calibration, and range selection rather than introducing an expensive external converter before the prototype is characterized.

### Rev.1 Stage 1 metrology ADC (frozen)

```text
PA0 ADC_VEXC, PA1 ADC_VMID, PA2 ADC_RET_1X, PA3 ADC_RET_HG
ADC clock 12 MHz (HSE/PLL only); metrology blocked on HSI fallback
Dual regular simultaneous ADC1+ADC2, 7.5-cycle sample time on ranks
3-rank sequence per TIM2 trigger:
  ADC1: VEXC, VEXC, VMID
  ADC2: RET_1X, RET_HG, VMID
TIM2_CC2 internal compare trigger; PA1 never configured as TIM2 GPIO output
DMA1 Channel 1, packed ADC1/ADC2 32-bit words, 768-word static buffer (3072 B)
Sample rates: 6400 / 64000 / 160000 SPS at 100 Hz / 1 kHz / 10 kHz excitation
256 sample instants per block (64 spc x4 or 16 spc x16)
```

### Rev.1 Stage 1 excitation (frozen)

```text
PA8 TIM1_CH1 PWM carrier 450 kHz (PSC=0, ARR=159)
45-point Q15 sine LUT via DMA1 Channel 5 -> TIM1_CCR1
RCR 99/9/0 for 100 Hz / 1 kHz / 10 kHz
100 mVrms and 500 mVrms nominal classes; 500 mVrms forbidden on 10 Ω RREF
OFF / NEUTRAL(50%) / SINE states; boot/fault = OFF
```

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

The UI uses incremental rendering, procedural graphics, and external resources.

## External Flash

W25Q family over conventional SPI. The current BOM uses W25Q64JVSSIQ, but the driver should recognize compatible W25Q16/32/64/128 parts.

The W25Q is the external resource/data ROM of the instrument, not merely an image store. No filesystem is planned in the first version. Assets/resources use a simple packed format with table, offsets, dimensions/format, metrics, flags, and CRC.

Custom UI fonts must not be stored as large glyph arrays in STM32 internal Flash. Authoring fonts such as TTF/OTF are converted offline into compact MCU-oriented raster resources. The STM32 firmware does not parse TTF/OTF and does not embed FreeType.

The external resource pack may contain multiple rasterized font sizes, large numeric glyphs, measurement symbols such as Ω, µ, °, and ±, icons, glyph metrics, localization resources where useful, and optional compact compression such as simple RLE.

At runtime, the STM32 reads glyph/resource data from W25Q in small chunks and renders it to the ILI9341 using independent chip selects on the shared SPI bus. The design uses a fixed small scratch buffer, renderer state, and possibly a tiny metadata cache; installed font/resource size must not create proportional SRAM usage.

STM32 internal Flash retains only executable rendering/decoding code and a tiny emergency fallback font sufficient for basic diagnostic/safety messages if W25Q is absent or corrupted. Procedural graphics such as lines, boxes, scales, phase vectors, equivalent circuits, graphs, and similar instrument drawings should normally be generated by code rather than stored as full-screen bitmaps.

The W25Q is not an executable-code extension. Rev.1 executable firmware must fit the MCU internal Flash budget.

## Automatic component/model detection

Normal Rev.1 operation does **not** require the user to preselect resistor, capacitor, or inductor.

The fundamental measurement is complex impedance `Z = R + jX`. Automatic R/C/L/mixed interpretation occurs only after impedance calculation and may use phase, R/X dominance, derived quantities, quality metadata, and cross-frequency behavior.

The classifier must be able to return mixed/unknown/low-confidence rather than forcing a false R/L/C label.

Manual component/model selection is not part of the normal product menu. A future Lab/Debug tool may expose manual measurement controls without changing the underlying impedance computation.

## Primary user interaction

Normal measurement/result context:

```text
OK short     start a measurement
UP/DOWN      browse pages of the last result
OK long      open the main menu
```

The primary result page uses large detected-component/value typography. Small footer metadata identifies the excitation amplitude and frequency most directly associated with the displayed primary value.

Additional pages expose electrical details, measurement metadata, useful graphs, and an optional on-screen debug console.

During a multi-attempt/refined measurement, validated partial results may be displayed together with a clear waiting/progress indication. TFT/Flash rendering occurs between acquisition-critical windows, not continuously during ADC/DMA acquisition.

## Main menu

Rev.1 main menu baseline:

```text
Calibration
Display
Sound
Language
Debug
About
```

Menu interaction:

```text
UP/DOWN      navigate/change
OK short     select/confirm
OK long      back
```

If the backlight is fully off after inactivity timeout, the first button press wakes the display and is consumed without executing its normal action.

## Calibration boot policy

Calibration validity is mandatory product state, not only a menu feature.

On every boot, firmware validates the required persisted calibration set for integrity, schema/model compatibility, hardware revision compatibility, and completeness.

If calibration is missing, corrupt, incompatible, or incomplete, normal measurement-ready operation is blocked and the user is forced into the Calibration workflow until a valid required set has been written and verified.

After a valid calibration exists, Calibration is entered manually through the menu unless a later validation failure reasserts the mandatory gate.

Manual recalibration must preserve the previous valid calibration until a replacement candidate has been completely written, read back, validated, and activated.

Calibration persistence uses W25Q. The STM32F103C8T6 has no native EEPROM; repository documentation/code must not describe this as MCU internal EEPROM.

## Localization

Initial planned UI languages are Portuguese and English.

UI logic uses stable resource/text IDs rather than scattering translated literals through screen code. Localization resources may reside in W25Q while fundamental fallback safety/error text remains internally available.

## Debug console

Debug settings include at least console enable/disable and log level.

When the TFT debug console is enabled, it becomes an additional page in the normal UP/DOWN result-page sequence. The TFT console is a fixed-size recent-event ring buffer; UART remains the higher-volume diagnostic stream.

Debug logging respects acquisition quiet mode and does not perform verbose work in critical ISR paths.

## User input

Three buttons: UP, DOWN, and OK. A rotary encoder was intentionally removed to simplify mechanics and hardware.

The button driver reports debounced event semantics; navigation policy is implemented above the driver.

## Backlight and buzzer

PB0 controls TFT backlight PWM.

PB1 controls an external passive piezo through BC817. The piezo is located in the enclosure rather than on the analog PCB.

Display settings include brightness and backlight timeout.

Sound settings include at least persistent enable/disable for ordinary UI feedback. The policy for mandatory safety-critical audible warnings while ordinary sound is disabled must be decided explicitly before release.

## Safety

K1 is fail-safe and the residual-voltage detector sits in front of the measurement AFE.

`D_TVS` and `R_TVS_LINK` remain DNP initially.

The current goal is to detect/tolerate residual voltage in the approximate ±100 V observation envelope, not to measure energized high voltage.

### Rev.1 Stage 2 K1 guards and permit (Phase 05)

```text
K1_OPERATE_GUARD_MS = 10   (REQUIRES_BENCH_VALIDATION)
K1_RELEASE_GUARD_MS = 8    (REQUIRES_BENCH_VALIDATION)
HW_MEASURE_PERMIT_TTL_MS = 5
```

Lab DUT measure (`hw_metrology_measure`) issues the measurement permit after excitation NEUTRAL settle and quiet entry; validate is consumed immediately before `hw_k1_request_measure()`. The application shell continues global safety evaluation but skips `hw_k1_force_safe()` while the measure module owns K1. Successful measure shutdown returns excitation to NEUTRAL for 1 ms before commanding K1 SAFE; emergency abort during K1 MEASURE commands excitation OFF immediately. After K1 returns SAFE, the 8 ms release guard must complete before auxiliary ADC resume when K1 had reached MEASURE.

Raw capture (`hw_metrology_session`, `lab metrology capture`) keeps K1 SAFE and does not consume a permit.

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
