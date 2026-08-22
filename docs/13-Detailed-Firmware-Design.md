# Detailed Firmware Design

This document defines the planned firmware decomposition, module contracts, data flow, persistence model, and implementation order.

## Architectural objective

The firmware should behave like measurement-instrument firmware:

- electrical safety takes priority over UI behavior;
- acquisition timing is deterministic;
- DSP remains testable outside the STM32 target;
- device-specific code is confined to BSP/drivers;
- UI and storage never block the metrology path;
- software faults tend toward SAFE;
- calibration/settings formats are explicit and versioned;
- the canonical implementation language is C17;
- the canonical build system is CMake;
- runtime orchestration uses a cooperative superloop with modular finite-state machines rather than an RTOS.

## Runtime execution model

Rev.1 uses no RTOS. The baseline runtime model is an event-driven cooperative superloop with small state machines by responsibility.

Representative shape:

```c
for (;;)
{
    safety_step();
    input_step();
    app_step();
    measurement_step();
    ui_step();
    storage_step();
    diagnostics_step();
    watchdog_service();
}
```

The exact call order is implementation-defined and may evolve, but the following rules are normative:

- `*_step()` functions must return promptly and must not hide long blocking delays;
- long operations are decomposed into explicit states/timeouts/events;
- time-based waits use the monotonic BSP timebase rather than busy waits;
- ISR code is short and normally publishes flags/events or transfers bounded data;
- heavy DSP, UI rendering, storage writes, and logging do not run inside critical ISRs;
- one giant application `switch` must not absorb every subsystem responsibility.

Expected state-machine domains include:

```text
application / boot policy
measurement cycle
autorange / attempt control
calibration workflow
UI/navigation
buttons/debounce
storage operations
power/safety policy
```

Some domains may be implemented as small explicit state objects rather than separate source modules, but their responsibilities and transitions must remain testable and isolated.

## Layers

```text
┌─────────────────────────────────────────┐
│ app                                     │
│ state machines / orchestration / policy │
├──────────────┬──────────────┬───────────┤
│ measurement  │ storage      │ ui        │
├──────────────┴──────┬───────┴───────────┤
│ hardware services  │ drivers            │
├─────────────────────┴───────────────────┤
│ bsp / CMSIS / HAL / LL                  │
└─────────────────────────────────────────┘
```

## `src/app`

Owns global policy, not GPIO details.

Planned files:

```text
app_state_machine.c/.h
app_events.c/.h
app_context.c/.h
app_scheduler.c/.h
app_faults.c/.h
app_version.c/.h
```

### Application-level states

The application state machine coordinates high-level product policy and subordinate state machines. It should not duplicate all measurement substates.

Representative application states:

```text
BOOT
SELF_TEST
CALIBRATION_CHECK
CALIBRATION_REQUIRED
READY
MEASURING
RESULT
MENU
FAULT
```

The measurement state machine owns detailed transitions such as:

```text
IDLE
SAFE_CHECK
PREPARE_RANGE
PRE_EXCITATION
K1_MEASURE
SETTLING
ACQUIRE
K1_SAFE
PARTIAL_PROCESS
UI_UPDATE_POINT
RETRY_OR_REFINE
FINAL_PROCESS
DONE
ABORT
```

The exact names may change, but the separation of application policy from detailed measurement sequencing is intentional.

### Application events

Examples:

```text
APP_EVENT_BUTTON_UP
APP_EVENT_BUTTON_OK
APP_EVENT_BUTTON_DOWN
APP_EVENT_BUTTON_OK_LONG
APP_EVENT_MEASURE_REQUEST
APP_EVENT_DMA_BLOCK_READY
APP_EVENT_PARTIAL_RESULT
APP_EVENT_MEASUREMENT_DONE
APP_EVENT_CALIBRATION_INVALID
APP_EVENT_CALIBRATION_VALID
APP_EVENT_RESIDUAL_VOLTAGE
APP_EVENT_CHARGER_CONNECTED
APP_EVENT_LOW_BATTERY
APP_EVENT_FAULT
```

The main loop dispatches events. ISR code only produces minimal flags/events and moves data.

## `src/bsp`

The only layer expected to depend strongly on STM32F1 implementation details.

Planned files:

```text
bsp_clock.c/.h
bsp_gpio.c/.h
bsp_adc.c/.h
bsp_dma.c/.h
bsp_timer.c/.h
bsp_spi.c/.h
bsp_uart.c/.h
bsp_watchdog.c/.h
bsp_time.c/.h
bsp_reset.c/.h
```

Responsibilities:

- clock tree;
- JTAG/SWD configuration;
- safe GPIO defaults;
- ADC1/ADC2;
- DMA;
- TIM1/TIM2/TIM3/TIM4;
- SPI2;
- USART1;
- watchdog;
- monotonic clock;
- reset reason.

## `src/drivers`

Reusable device drivers without instrument-level policy.

Planned files:

```text
ili9341.c/.h
w25q.c/.h
buttons.c/.h
spi_bus.c/.h        # if required for shared-bus ownership
crc32.c/.h          # location may change if a common utility module is added
```

### ILI9341 contract

Representative API:

```c
bool ili9341_init(void);
void ili9341_set_rotation(uint8_t rotation);
void ili9341_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void ili9341_fill(uint16_t rgb565);
void ili9341_write_pixels(const uint16_t *pixels, size_t count);
bool ili9341_read_id(uint32_t *id);
```

The driver does not know measurement screens, units, or RLC results.

### W25Q contract

Representative API:

```c
bool w25q_init(void);
bool w25q_read_jedec_id(uint32_t *id);
bool w25q_read(uint32_t address, void *dst, size_t size);
bool w25q_fast_read(uint32_t address, void *dst, size_t size);
bool w25q_page_program(uint32_t address, const void *src, size_t size);
bool w25q_sector_erase(uint32_t address);
bool w25q_wait_ready(uint32_t timeout_ms);
```

The driver should support compatible W25Q16/32/64/128 devices rather than expose a W25Q64-specific API.

## `src/hardware`

Encapsulates instrument-specific hardware and safe command sequences.

Planned files:

```text
hw_safety.c/.h
hw_relays.c/.h
hw_range.c/.h
hw_excitation.c/.h
hw_power.c/.h
hw_battery.c/.h
hw_temperature.c/.h
hw_backlight.c/.h
hw_buzzer.c/.h
```

Representative safety contract:

```c
typedef struct
{
    bool charger_connected;
    bool residual_present;
    bool supply_ok;
    bool adc_ok;
    bool range_ok;
} safety_status_t;

bool safety_measure_allowed(const safety_status_t *status);
void safety_force_safe(void);
```

No external caller should energize K1 by directly manipulating a GPIO.

### Range contract

```c
typedef enum
{
    RREF_10R,
    RREF_100R,
    RREF_1K,
    RREF_10K,
    RREF_100K,
    RREF_1M,
} rref_range_t;

bool range_select(rref_range_t range);
void range_disable(void);
```

`range_select()` must enforce:

```text
RANGE_EN=0
A0/A1/A2=new range
wait dead-time
RANGE_EN=1
```

### Excitation contract (Stage 1 implemented)

Stage 1 modules: `hw_excitation`, `bsp_excitation`, `hw_metrology_session`, `bsp_metrology_adc`, `hw_metrology_raw`.

States:

```text
OFF     TIM1 stopped, DMA off, PA8 GPIO LOW (boot/fault)
NEUTRAL TIM1 50% duty (CCR1=80), DMA off
SINE    TIM1 + circular DMA1 Ch5 feeding 45-entry CCR table
```

Configuration changes occur only while SINE is stopped. Sequence: OFF → configure → fill CCR → NEUTRAL → 1 ms settle → reset phase → enable DMA → SINE → sine settle → ADC capture → OFF.

Amplitude policy rejects 500 mVrms on 10 Ω RREF with explicit `BSP_STATUS_NOT_SUPPORTED`.

### ADC ownership (Stage 1)

Before metrology capture: `hw_aux_sensors_pause()` (residual evidence invalidated). After capture: stop metrology, restore ADC1 aux configuration via `bsp_adc_init()`, `hw_aux_sensors_resume()` — eight fresh SAFE residual evaluations required. Restore failure latches `APP_SAFETY_FAULT_ADC_RUNTIME`.

### DMA buffer format

One canonical `uint32_t raw_words[768]` (3072 B). Per sample index `n`:

```text
word[3n+0]: ADC1 VEXC rank1 | ADC2 RET_1X rank1
word[3n+1]: ADC1 VEXC rank2 | ADC2 RET_HG rank2
word[3n+2]: ADC1 VMID rank3 | ADC2 VMID rank3
```

Mask each channel to 12 bits (`0x0FFF`). Clipping scan uses residual rails (≤16, ≥4079).

### ISR ownership

```text
DMA1_Channel1_IRQHandler  ADC capture TC/TE only
DMA1_Channel5_IRQHandler  excitation DMA TE only
```

No TIM2 per-sample IRQ, no ADC EOC per-rank IRQ, no DSP/UART/TFT in ISRs.

### Lab diagnostic (not product measurement)

Lab build only:

```text
lab metrology capture <100|1k|10k> <100m|500m> <10r|100r|1k|10k|100k|1m>
```

Non-blocking session FSM; K1 forced SAFE throughout; UART raw dump after excitation OFF and quiet released. Never consumes measurement permit.

Lab build only (Stage 2):

```text
lab metrology measure <100|1k|10k> <100m|500m> <10r|100r|1k|10k|100k|1m>
```

Stage 2 module: `hw_metrology_measure`. DUT measure FSM reuses the Stage 1 raw buffer and transport contract but energizes K1 only after single-use permit validate. `app_shell` skips global `hw_k1_force_safe()` while `hw_metrology_measure_k1_owned()`; dynamic blockers inside the measure FSM handle emergency abort during K1 MEASURE. Capture (`hw_metrology_session`) and measure are mutually exclusive in the lab console.

### K1 ownership and permit lifecycle (Stage 2)

```text
Global shell: hw_k1_force_safe() each safety tick unless measure module owns K1
Ownership window: from K1 request through operate guard, MEASURE, neutral post-capture, K1 SAFE, release guard
Permit issue: after range READY, excitation NEUTRAL, 1 ms settle, quiet ON — uses pre-permit sensor evidence
Permit validate: immediately before aux pause / K1 request; consumed single-use; TTL 5 ms
hw_k1_request_measure(): builds hw_safety_input_t from pre-permit issue evidence
Success shutdown: neutral excitation before K1 SAFE (not immediate OFF)
Abort during MEASURE: excitation OFF immediately; 8 ms release guard before aux resume if K1 reached MEASURE
```

### Session closure fixes (Stage 1, Stage 2)

Cleanup return statuses latch `APP_SAFETY_FAULT_K1_IO`, `RANGE_IO`, `ADC_RUNTIME`, or `METROLOGY_RUNTIME` as appropriate. Primary session/measure error is preserved during cleanup except when aux-restore is the secondary failure. First ADC restore failure sets `adc_restore_failed` and prevents dumpable results even if DMA succeeded or a later restore succeeds.

## `src/measurement`

The metrology core is predominantly pure/testable C. Phase 06 currently implements:

```text
measurement_dsp.c/.h
```

This module owns project complex helpers, raw ADC scaling, synchronous phasor
extraction, RET_HG reconstruction, guarded impedance calculation, derived quantities,
and a preliminary single-condition interpretation primitive.

Still deferred to later phases:

```text
autorange
final confidence scoring
persistent calibration application/storage
multi-frequency final classification
product UI publication
```

### Core data types

Representative implemented types:

```c
typedef struct
{
    float re;
    float im;
} measurement_complex_t;

typedef struct
{
    measurement_complex_t vexc_1;
    measurement_complex_t ret_1x;
    measurement_complex_t vexc_2;
    measurement_complex_t ret_hg;
    measurement_complex_t vmid;
    measurement_complex_t ret_hg_reconstructed;
} measurement_phasor_set_t;

typedef struct
{
    measurement_complex_t z_ohms;
    bool open_like;
    bool short_like;
} measurement_impedance_result_t;
```

Exact types may evolve, but UI formatting must not leak into metrology types.

### Acquisition metadata

Acquisition requires explicit metadata:

```text
frequency
sample_rate
cycle_count
samples_per_cycle
RREF
excitation amplitude
RET channel (1X/HG)
ADC timing/skew
calibration key
```

### Phasor extraction

Implemented approach: synchronous detection / single-bin DFT.

For each channel:

```text
x[n] = dc + Re{Vpeak * exp(j * theta[n])}
Vpeak = (2 / N) * sum(x[n] * (cos(theta[n]) - j*sin(theta[n])))
```

The returned phasor is peak volts. Positive phase means the waveform leads the cosine
reference. Phase is reported in radians. The current Phase 05 windows contain integer
cycles, so VMID/DC offset is rejected by the single-bin extraction rather than by
assuming raw ADC samples are centered around zero.

Reference generation uses fixed recurrence coefficients for 64-sample and
16-sample cycles. The target sample loop does not call runtime trigonometric functions.

### Impedance calculation

```text
Vs = VEXC - VMID
Vx = RET  - VMID
Zx = ZREF * Vx / (Vs - Vx)
```

`ZREF` may itself be represented as a calibrated complex value/transfer correction.

### High-gain channel

Current hardware:

```text
G_HG_nominal = 1 + 68k / 4.7k ≈ 15.47
```

DSP uses a calibrated response such as `H_HG(f, range, amplitude)` rather than only nominal DC gain.

### Automatic component/model classification

The normal Rev.1 user flow does not require the user to select resistor, capacitor, or inductor before measuring.

Classification occurs **after** complex impedance is calculated. The fundamental measured quantity remains `Z = R + jX`; the R/L/C label is an interpretation layer, not an input to the impedance equation.

The classifier may use:

- sign and magnitude of reactance;
- phase;
- R/X dominance;
- derived C/L validity;
- frequency dependence across available 100 Hz / 1 kHz / 10 kHz measurements;
- stability/confidence information.

A component with large parasitics or an ambiguous network must be allowed to return a mixed/unknown/low-confidence interpretation rather than forcing an incorrect R/L/C label.

Frequency behavior may strengthen classification, for example capacitive reactance tending downward in magnitude with increasing frequency and inductive reactance tending upward, subject to real-component non-idealities and qualified evidence.

Manual component-type selection is not part of the ordinary product UI. A future Lab/Debug measurement tool may request a model/parameter focus, but must not change the underlying impedance computation.

### Autorange output

The decision engine may return:

- accept;
- change RREF;
- change 1X/HG channel;
- change frequency;
- change amplitude;
- retry;
- reject.

A measurement session may publish validated partial results between attempts so the UI can show progressive information. Partial UI publication must occur outside critical ADC/DMA acquisition windows.

## `src/storage`

Planned files:

```text
storage_layout.c/.h
asset_store.c/.h
settings_store.c/.h
calibration_store.c/.h
record_store.c/.h
```

### Logical layout

A conceptual layout may reserve separate sectors for superblock, redundant settings, calibration, and resource/asset pack. Exact addresses must not be frozen until the minimum supported W25Q density is decided.

### Record format

```text
magic
schema_version
hardware_revision
record_type
sequence
payload_length
crc32
payload
```

Every read validates identifier, version, bounds, and CRC.

## `src/ui`

Planned files may include:

```text
ui_core.c/.h
ui_theme.c/.h
ui_format.c/.h
ui_navigation.c/.h
ui_widgets.c/.h
ui_resources.c/.h
ui_localization.c/.h
screen_startup.c/.h
screen_measure.c/.h
screen_details.c/.h
screen_measurement_info.c/.h
screen_graph.c/.h
screen_menu.c/.h
screen_calibration.c/.h
screen_debug_console.c/.h
screen_about.c/.h
```

Rules:

- no blocking screen code;
- no `delay()`-style animation;
- render changed regions where practical;
- stream external font/glyph/asset resources from W25Q;
- keep SI-prefix/unit formatting separate from calculation;
- measurement logic does not depend on the active screen;
- no full framebuffer;
- UI emits intent and never directly drives relay/range/excitation GPIOs.

### Normal navigation contract

Outside the menu:

```text
OK short     request a measurement
UP/DOWN      browse pages of the last result
OK long      open the main menu
```

Result pages are conceptually:

```text
Primary result
Electrical details
Measurement information
Useful graphs/visualizations (zero or more)
Debug console (only when enabled)
```

The primary page uses large typography for detected dominant component/model and primary value. Small footer metadata identifies the excitation amplitude and frequency most directly associated with the displayed value.

During measurement, valid partial/refined results may be displayed with a clear waiting/progress message. Rendering occurs between critical acquisition windows, never as uncontrolled TFT traffic during the ADC/DMA acquisition window.

### Main menu contract

```text
Calibration
Display
Sound
Language
Debug
About
```

Menu controls:

```text
UP/DOWN      navigate/change
OK short     select/confirm
OK long      back
```

If the backlight is fully off because of inactivity timeout, the first button press wakes the display and is consumed without triggering its normal action.

## Asset pack

The W25Q asset pack is the external resource/data ROM for display resources, not only a bitmap image store. Suggested simple structure:

```text
header
asset_count
asset_table[]
blob data...
```

Each entry can contain:

```text
id
offset
size
width
height
format
flags
metrics_offset / metrics_size where applicable
crc32
```

Candidate formats:

- raw RGB565;
- simple RLE over RGB565;
- 1/4/8-bit alpha or glyph masks;
- compact rasterized font glyph data with metrics;
- localized string/resource tables where useful.

Host tooling converts source PNG/icon/font assets to the firmware format. Authoring fonts such as TTF/OTF are converted offline on the development PC; the STM32 firmware must not parse TTF/OTF or embed FreeType.

The resource pack may contain multiple font sizes, large numeric glyphs, measurement symbols such as Ω, µ, °, and ±, icons, glyph metrics, localized resources, and optional compression such as simple RLE.

Runtime resource access is chunked because W25Q and ILI9341 share SPI2 with independent chip selects. The intended sequence is:

```text
select W25Q CS
read resource chunk into fixed scratch buffer
release W25Q CS
select TFT CS
render/transmit chunk to ILI9341
release TFT CS
repeat
```

Installed resource size must not create proportional SRAM usage. The UI/storage boundary should use stable asset IDs plus a fixed small shared scratch buffer, ideally hundreds of bytes unless measurements justify more. Only renderer state and a tiny metadata cache should be resident in SRAM.

STM32 internal Flash keeps rendering/decoding code and a tiny emergency fallback font for essential diagnostic and safety messages when W25Q is absent or corrupted. Procedural graphics such as lines, boxes, scales, phase vectors, equivalent circuits, and graphs should be generated by code rather than stored as full-screen bitmaps.

## Settings

Compiled defaults must exist even if external Flash is invalid.

Examples:

```text
backlight brightness
backlight timeout
buzzer enabled
language
preferred display/result page where appropriate
log level / debug console enabled
```

No normal user setting may disable mandatory safety interlocks.

## Calibration key

Conceptual key:

```text
hardware_revision
frequency
RREF
excitation amplitude
RET channel
calibration type/model version
```

The persisted format must support schema evolution without directly serializing fragile C struct layouts.

## Calibration boot gate

Calibration validity is product state, not merely a menu preference.

On every boot the application must load and validate the required calibration set before entering normal READY operation. Validation includes record integrity, schema/model compatibility, hardware revision compatibility, required key coverage/completeness, and CRC/bounds checks.

If calibration is missing, corrupt, incomplete, or incompatible:

```text
BOOT
  ↓
CALIBRATION_CHECK
  ↓
CALIBRATION_REQUIRED
  ↓
Calibration wizard
  ↓
verified valid records
  ↓
READY
```

Normal measurement must remain unavailable while required calibration is invalid.

Manual recalibration after a valid boot uses candidate/previous-valid semantics: keep the active valid calibration until a new candidate has been fully written, read back, validated, and committed as active. A failed/cancelled/power-interrupted recalibration should preserve the previous valid state whenever possible.

Calibration is persisted through the W25Q storage layer. Do not refer to STM32F103C8T6 calibration persistence as internal EEPROM; the MCU has no native EEPROM.

## Host-side tests

Required focus areas:

- complex math;
- DFT/phasors;
- impedance equation;
- component/model classification;
- OPEN/SHORT/LOAD correction;
- autorange;
- confidence gates;
- CRC/record parsing;
- calibration boot-gate decisions;
- asset manifest/resource lookup;
- button semantics;
- pure application/measurement/UI state machines.

Synthetic vectors should cover ideal R/C/L components plus mixed impedances, noise, clipping, offsets, and timing/phase errors.

## Observability

A measurement session should be able to produce compact diagnostic metadata:

```text
session id
attempt sequence
range
frequency
amplitude
sample metadata
VEXC phasor
RET phasor
channel used
clipping
SNR
calibration id
partial/final result
classification
confidence
retry/rerange reason
```

Release builds may reduce this data; Lab builds should expose it through UART/TFT.

When the on-screen debug console is enabled, it becomes an additional page in the normal result-page sequence. The TFT console uses a bounded fixed-size ring buffer for recent events; UART remains the higher-volume stream.

## Fault policy

Critical faults include:

- residual voltage;
- charger connected during a measurement attempt;
- inconsistent ADC/DMA state;
- invalid range state;
- brownout/supply invalid;
- watchdog/reset recovery;
- impossible state transition.

Baseline response:

```text
stop excitation
RANGE_EN=0
K1_SAFE
K2 safe/default
buzzer off
record fault
show/report fault when UI/diagnostics are available
```

## Implementation sequence

The normative implementation sequence is maintained in [`../plans/`](../plans/). Agents must not treat the module list in this document as permission to implement later phases early.
