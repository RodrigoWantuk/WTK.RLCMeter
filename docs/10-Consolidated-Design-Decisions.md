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

Manual component/model selection is not part of the normal product menu. A future Bringup/Debug tool may expose manual measurement controls without changing the underlying impedance computation.

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

## Calibration persistence and correction substrate

The persisted calibration substrate uses two independent compatibility versions:

```text
schema_version = 2              portable byte/framing schema
model_version  = 4              OSL/Mobius model with effective HG normalization
hardware_rev   = 0x00010001     Rev.1 STM32F103C8T6 firmware/hardware contract
```

`schema_version` is not a firmware version and is not the same as `model_version`.
Changing the byte layout and changing the correction math are separate compatibility
events.

Calibration records are serialized manually as little-endian fields. Raw C structs are
not persisted. The Stage 2A frame uses:

```text
magic
record_type
schema_version
header_size = 64 bytes
payload_length
sequence
hardware_revision
model_version
payload CRC32
commit marker
payload
```

The CRC32 is the standard reflected IEEE CRC32 over all header fields except the CRC and
commit marker plus the payload. The commit marker is programmed last so an interrupted
write cannot make an incomplete candidate look valid.

Floating correction coefficients are serialized as explicit IEEE754 binary32 fields.
Temperature metadata is stored as signed integer millidegrees C.

Development-era model versions 1/2/3 are not executable firmware models. Current
firmware only executes model version 4. Older model numbers may still be diagnosed from
frame headers as `INCOMPATIBLE_MODEL`, but the embedded runtime does not retain direct
affine correction branches.

Model version 4 keeps the same portable schema and defines persisted HG normalization as
an effective normalized transfer:

```text
volts = raw * code_to_volts + offset_volts       global per ADC stream
H_HG_effective = (RET_HG_raw / VEXC_2) / (RET_1X / VEXC_1)
RET = VMID + (RET_HG - VMID) / H_HG_effective
t = Vx / Vs
Z_corrected = K * (t - t_short) / (t - t_open)
```

For the current OSL model, the six persisted complex fields are named and interpreted as:

```text
effective_hg_transfer = effective normalized HG transfer
load_z_ohms           = known LOAD reference used for the solve
t_short               = measured SHORT transfer
t_open                = measured OPEN transfer
k                     = projective OSL scale coefficient
reserved              = reserved zero
```

The C struct now uses the OSL names directly while the portable field-by-field byte
layout remains 80 bytes, so `schema_version` stays at 2. A current OSL record must carry
the OSL model flag, load-reference flag, finite nondegenerate coefficients, and no
legacy direct-output correction flag bits. Missing exact calibration can still be
explicitly resolved to ideal defaults for bring-up/debug, but the provenance remains
`MISSING` and uncalibrated. Product qualification must not treat that fallback as
calibrated.

Calibration keys include hardware revision, model version, range, frequency, and
amplitude. RET channel and RET strategy are deliberately excluded from the persistent
pre-DSP lookup key because Phase 05 captures both return channels and Phase 06 selects
the usable return path after DSP. Each condition record carries both RET_1X and RET_HG
output-correction terms, avoiding a circular dependency between calibration lookup and
RET selection. Stage 2A.1 resolution is exact-condition only; there is no silent
cross-frequency/range extrapolation.

W25Q layout reserves the mutable tail of each supported part:

```text
resource pack       from address 0 to mutable tail start
calibration slot A  4096 bytes
calibration slot B  4096 bytes
settings            4096 bytes
diagnostics         16384 bytes
bring-up test       final 4096-byte sector
```

The final sector remains the controlled W25Q bring-up test sector. Assets/resources,
calibration, settings, diagnostics, and bring-up scratch space remain separate logical
regions.

Calibration set validity is not a boolean. Firmware distinguishes missing, corrupt,
schema-incompatible, hardware-incompatible, model-incompatible, incomplete, and valid
sets with diagnostic flags. Phase 08 will consume this validity model for the mandatory
calibration boot gate; Stage 2A only provides the substrate and tests.

The calibration store is asynchronous against the W25Q driver. Erase and program use
start/wait states and driver polling, and the store never starts a second W25Q mutation
while one is active. Active calibration selection chooses the newest usable compatible
complete set, not merely the newest CRC-valid frame; diagnostics still expose newer
rejected slots.

### Rev.1 condition domain and calibration integrity

Rev.1 firmware separates:

```text
hardware-supported condition
calibratable condition
qualified/product-enabled condition
```

`unqualified`, `uncalibrated`, and `unsupported` are different states. The authoritative
Stage 2A.2 physical support rule is centralized in `measurement_condition.c/.h` and is
shared by automatic measurement policy, calibration requirements generation, Bringup
fixed-condition validation, and future qualification-map generation.

The Stage 2A.2 Rev.1 hardware-supported domain is:

```text
range:      10 Ohm / 100 Ohm / 1 kOhm / 10 kOhm / 100 kOhm / 1 MOhm
frequency: 100 Hz / 1 kHz / 10 kHz
amplitude: 100 mVrms / 500 mVrms
```

with the hard physical/product transport prohibition:

```text
10 Ohm + 500 mVrms is not supported at any frequency
```

Therefore the current hardware-supported and calibratable Rev.1 matrix contains 33
conditions. The 100 kOhm and 1 MOhm high-frequency conditions remain representable for
calibration. They may later be marked `UNQUALIFIED`, `EXTENDED`, or `DISABLED` by real
qualification evidence, but they are not removed from calibration storage merely because
parasitics, leakage, SNR, or phase error may make them difficult.

Calibration record identity is the complete key:

```text
hardware_revision
model_version
range_id
frequency
amplitude
```

`condition_id` is diagnostic/provenance metadata derived from CRC32 of that complete
key. It is not an authoritative lookup key, and a condition-ID collision must not select
the wrong correction. Calibration set ADD fails on an existing complete key; REPLACE
fails when the complete key does not already exist. Decoded or manually assembled sets
with duplicate complete keys are invalid.

Slot parsing is staged. Firmware first determines structural/integrity state, then
schema/hardware/model compatibility, then payload completeness/usability. A committed,
CRC-valid frame from an older schema or different hardware/model is diagnosed as
`INCOMPATIBLE_SCHEMA`, `INCOMPATIBLE_HARDWARE`, or `INCOMPATIBLE_MODEL`, preserving
header metadata such as sequence/schema/hardware/model for diagnostics. Genuine CRC,
magic, length, or torn-write failures remain `CORRUPT`.

The active production calibration set remains coefficient-focused. OPEN/SHORT/LOAD raw
captures for Stage 2B should be used to derive condition corrections; if raw evidence is
retained for audit/debug, it belongs in a separate diagnostic/evidence record path rather
than permanently inflating the active `measurement_cal_set_t`.

### Calibration SRAM and runtime ownership

The calibration runtime is application-owned, not Bringup-console-owned. The ownership model
is intentionally split:

```text
app_calibration_service_t:
    product-owned calibration service
    owns runtime, persistent store scratch, active OSL workflow
    owns current OSL campaign and explicit candidate lifecycle

app_calibration_session_t:
    application-level OSL capture controller
    connects the service workflow to Phase 05 fixed-condition measurements
    owns capture/cancel events, not raw DMA storage

app_calibration_campaign_t:
    compact current-condition OSL campaign
    stores OPEN/SHORT/LOAD summaries and one solved condition

app_calibration_runtime_t:
    active decoded coefficient set
    active slot/provenance
    compact slot diagnostics

measurement_cal_store_t:
    one 3072-byte serialized frame image
    one decoded scan scratch set
    async W25Q erase/program/verify metadata

app_bringup_console_t:
    bring-up command/dump state only
    pointer to app_calibration_service_t/app_calibration_session_t
    does not own store scratch, calibration campaign state, or automatic session state
```

Calibration store terminal states are acknowledged explicitly. After `DONE`, the
service activates the verified set exactly once, refreshes active-slot diagnostics, and
acknowledges the store back to `IDLE` before candidate scratch may be reused. After
`ERROR`, discard/recovery is explicit and the previous active calibration remains
unchanged.

`measurement_cal_store_write_start()` must not allocate multiple complete
`measurement_cal_set_t` instances on the stack. It serializes the const candidate into
the store-owned image, preflights through the store-owned scan scratch, and keeps compact
expected identity metadata for post-write verification. Post-write identity is checked
from the complete condition-key fields rather than from the diagnostic `condition_id`.

The STM32F103C8T6 SRAM policy reserves at least 2048 bytes for stack headroom in every
target build. STM32 builds produce GCC `.su` stack-usage files so calibration store/load
frames can be audited. Calibration workflows must not duplicate the Phase 05 raw ADC
DMA buffer; Stage 2B must consume raw captures in place, derive compact coefficients,
and discard transient acquisition evidence unless an explicit future diagnostic storage
path is specified.

### Shared metrology/storage workspace

Phase 08 Stage 1.1 makes large transient application storage explicit. The firmware owns
one 3072-byte `app_io_workspace_t` arena shared by Phase 05 raw metrology capture and
calibration-store frame serialization/verification. Ownership is tagged as `METROLOGY`
or `CALIBRATION_STORE`; a second borrower receives `BSP_STATUS_BUSY`. Calibration
commits keep the workspace until the terminal store state is acknowledged, and metrology
releases it when the raw block is acknowledged.

The BSP no longer owns hidden raw sample storage, and the calibration store no longer
contains an internal frame image. This saves one full 3072-byte buffer in PRODUCT builds
without changing the Phase 05 raw data format or the persistent calibration schema.

PRODUCT builds have a 16 KiB preferred accounted-RAM target and a 17 KiB hard gate.
BRINGUP builds have an 18 KiB accounted-RAM hard gate. These gates include static
`.data/.bss/.noinit` plus the linker-reserved stack/heap floor and are enforced by
`Firmware/tools/firmware_size.py`.

### Compact product rendering state

The Phase 08 product UI stores a compact `ui_product_measurement_t` presentation
snapshot instead of embedding a complete `measurement_session_result_t`. The Phase 07
policy result remains the authoritative measurement record; UI state holds only the
fields needed to render the current page.

The fallback TFT renderer is cooperative and quiet-aware. It chunks fills through the
ILI9341 driver, draws at most one scaled fallback-font character per step, coalesces
newer product view generations, and performs partial same-page body clears instead of
full-screen clears when possible. The current pixel chunk size remains intentionally
small until physical SPI/display timing validates a larger scratch buffer.

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

Bringup DUT measure (`hw_metrology_measure`) issues the measurement permit after excitation NEUTRAL settle and quiet entry; validate is consumed immediately before `hw_k1_request_measure()`. The application shell continues global safety evaluation but skips `hw_k1_force_safe()` while the measure module owns K1. Successful measure shutdown returns excitation to NEUTRAL for 1 ms before commanding K1 SAFE; emergency abort during K1 MEASURE commands excitation OFF immediately. After K1 returns SAFE, the 8 ms release guard must complete before auxiliary ADC resume when K1 had reached MEASURE.

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

## Phase 06 DSP conventions

The first DSP/impedance core consumes the current Phase 05 raw acquisition block:

```text
256 sample instants
3 packed 32-bit ADC words per instant
VEXC_1/RET_1X, VEXC_2/RET_HG, VMID/VMID streams
```

The phasor convention is:

```text
x[n] = dc + Re{Vpeak * exp(j * theta[n])}
Vpeak = (2 / N) * sum(x[n] * (cos(theta[n]) - j*sin(theta[n])))
```

Positive phase therefore means the waveform leads the cosine reference. Phasor amplitudes
are peak volts. Derived phase is reported in radians.

The STM32 implementation uses `float` and project-owned complex helpers. It avoids
target-side runtime trigonometric calls inside the sample loop by using fixed recurrence
coefficients for the current coherent sample profiles:

```text
100 Hz  / 1 kHz: 64 samples per cycle
10 kHz:          16 samples per cycle
```

The target build currently links without `libm`; narrow local approximations cover
`sqrt`/`atan2` needs for diagnostic magnitude/phase. Python host tooling uses independent
double-precision complex math for reference comparisons.

Raw ADC conversion is explicit per channel:

```text
volts = raw * code_to_volts + offset_volts
```

The nominal Phase 06 default is `3.3 / 4095`, but the API is designed for later
per-channel calibration.

RET_HG is reconstructed through a complex transfer:

```text
RET = VMID + (RET_HG - VMID) / H_HG
```

The synthetic default is nominally `15.468085 + j0`. This is not a final calibrated gain.

`ZREF` is represented as a complex value so Phase 07 can provide
frequency/range-dependent calibrated reference impedances. The Phase 06 ideal defaults
are only nominal real resistor values.

## Phase 07 Stage 1 automatic measurement policy and orchestration

The first automatic measurement engine is implemented in
`Firmware/src/measurement/measurement_engine.c/.h` as a pure policy layer. It consumes
completed Phase 05 fixed-condition transactions and Phase 06 DSP results, then decides
whether to publish a partial result, request another bounded attempt, or terminate with
a final result.

The application orchestration layer is implemented in
`Firmware/src/app/app_measurement_session.c/.h`. It connects the pure policy to Phase
05 fixed-condition measurements and Phase 06 DSP:

```text
policy requested attempt
    -> Phase 05 transaction
    -> SAFE teardown
    -> Phase 06 DSP
    -> policy submit
    -> partial / next / final
```

Neither layer manipulates GPIO, relays, range pins, excitation registers, ADC/DMA, TFT,
W25Q, permits, charger state, or residual safety state directly. Every automatic attempt
is a fresh Phase 05 safety transaction.

No-history initial probe:

```text
RREF = 1 kOhm
frequency = 1 kHz
amplitude = 100 mVrms
RET strategy = DSP_AUTO
```

Live-mode previous-result hints may seed range/frequency/amplitude/channel preference,
but they do not carry safety authorization. Every attempt still goes through the Phase
04/05 safety permit, range transition, K1 ownership, excitation, and SAFE teardown
contract.

Stage 1 deterministic limits:

```text
maximum attempts per session = 6
maximum range transitions = 4
maximum frequency refinements = 1
maximum repeated conditions = 1
```

Stage 1 provisional policy thresholds:

```text
|Z| / |ZREF| <= 0.20      request lower RREF when possible
|Z| / |ZREF| >= 5.00      request higher RREF when possible
|Z| / |ZREF| >= 100       OPEN-like at the 1 MOhm upper edge
return peak < 2 mV        weak signal candidate
return peak >= 10 mV      strong return candidate
|X| / |R| <= 0.10         resistive dominance
|X| / |R| >= 0.25         reactive dominance
```

These thresholds are conservative software defaults and remain
`REQUIRES_BENCH_VALIDATION`.

The automatic engine never intentionally requests `10 Ohm + 500 mVrms`; the lower
excitation service also rejects that combination.

Confidence is represented as `NOMINAL`, `EXTENDED`, `LOW_CONFIDENCE`, or `REJECTED`.
It is separate from both mathematical quality and physical qualification:

```text
measurement quality:
    GOOD / DEGRADED / INVALID

qualification:
    UNQUALIFIED / NOMINAL / EXTENDED / DISABLED

publication confidence:
    NOMINAL / EXTENDED / LOW_CONFIDENCE / REJECTED
```

`NOMINAL` requires explicit qualification evidence. In the current unqualified Rev.1
software state, a mathematically clean measurement remains
`qualification=UNQUALIFIED` and `publication confidence=LOW_CONFIDENCE`.

The final displayed value references an explicit `primary_attempt_index` and
`primary_attempt`. Primary selection uses deterministic quality ordering such as DSP
validity, clipping, signal level, denominator conditioning, useful DUT/RREF ratio, RET
selection, preferred primary frequency, and qualification status. It does not select the
largest impedance magnitude, because reactive impedance magnitude changes naturally with
frequency.

Phase 05 captures both RET paths and Phase 06 selects the mathematically usable channel.
Phase 07 records RET_1X/RET_HG usability, selected channel, and overlap evidence for
confidence/calibration diagnostics, but it does not command a separate RET hardware path.

Session classification is downstream of complex impedance calculation and may return
`RESISTIVE`, `CAPACITIVE`, `INDUCTIVE`, or `MIXED_OR_UNKNOWN`. Ambiguous or inconsistent
evidence remains `MIXED_OR_UNKNOWN` rather than being forced into an R/C/L label.
Multi-frequency evidence is explicit and qualitative: capacitive tendency expects
negative reactance with generally decreasing `|X|` as frequency increases, while
inductive tendency expects positive reactance with generally increasing `|X|`. The
implementation tolerates non-ideal ESR/winding resistance and flags inconsistent trends.

## Product Calibration Service Ownership

Phase 07 Stage 2B.1 makes calibration storage a product-owned service, not a Bringup-console
scratch object. `app_calibration_service_t` owns the active calibration runtime, the
single `measurement_cal_store_t` scratch context, and the active OPEN/SHORT/LOAD
evidence workflow. `app_calibration_session_t` owns the application-level capture
orchestration between that workflow and Phase 05. The Bringup console attaches to the
service/session and reports cached state/events.

Normal `lab cal status`, `lab cal dump`, automatic measurement, and calibration
acquisition commands must not rescan or reinitialize W25Q storage. A deliberate
`lab cal rescan` command may refresh the cached product state, but it must be rejected
while the store or calibration workflow is busy.

OPEN/SHORT/LOAD acquisition evidence is transient in Stage 2B.1. It uses Phase 05
fixed-condition measurement transactions after the hardware has returned SAFE, then
extracts compact raw phasor/statistical evidence. It does not replace the active
persisted calibration set and does not copy the 3072-byte raw ADC DMA buffer into
application or calibration contexts.

Calibration evidence must preserve these raw observed quantities when available:
`VEXC_1`, `VEXC_2`, `RET_1X`, physical `RET_HG_raw`, reconstructed `RET_HG`, `VMID_ADC1`,
`VMID_ADC2`, per-path clipping/usability, and observed complex HG transfer
`RET_HG_raw / RET_1X`. The HG-side provisional impedance uses `VEXC_2` with reconstructed
HG return; it must not reuse `VEXC_1`.

The Stage 2B.1 workflow distinguishes raw observed evidence, provisional repeatability
measurements, and future corrected output. Active persisted output corrections are not
fed back into OSL evidence capture. OPEN captures may be singular in the final
impedance equation, so OPEN stability uses normalized admittance-like evidence
`(Vs - Vx) / Vx` rather than requiring a finite final `Zx`. SHORT and LOAD use
per-path impedance evidence. Missing path evidence is never counted as stable.

Calibration capture temperature is valid only when the auxiliary NTC snapshot is valid.
Firmware must not substitute a fixed placeholder ambient temperature in calibration
records or Bringup acquisition requests.

## Decision-change rule

Any agent or contributor proposing to reverse a consolidated decision must document:

1. new evidence or requirement;
2. hardware impact;
3. firmware impact;
4. calibration/qualification impact;
5. migration cost;
6. affected documentation/plans.
