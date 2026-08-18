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
- the canonical build system is CMake.

## Layers

```text
┌─────────────────────────────────────────┐
│ app                                     │
│ state machine / orchestration / policy  │
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

### Main states

```text
BOOT
SELF_TEST
SAFE_CHECK
WAIT_SAFE
READY
PREPARE_RANGE
PRE_EXCITATION
K1_MEASURE
SETTLING
ACQUIRE
K1_SAFE
PROCESS
RESULT
FAULT
```

### Application events

Examples:

```text
APP_EVENT_BUTTON_UP
APP_EVENT_BUTTON_OK
APP_EVENT_BUTTON_DOWN
APP_EVENT_MEASURE_REQUEST
APP_EVENT_DMA_BLOCK_READY
APP_EVENT_MEASUREMENT_DONE
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

### Excitation contract

```c
typedef struct
{
    uint32_t frequency_hz;
    uint32_t carrier_hz;
    uint16_t amplitude_mv_rms;
} excitation_config_t;
```

The API rejects forbidden combinations, including 500 mVrms with the 10 Ω RREF.

## `src/measurement`

The metrology core should be predominantly pure/testable C.

Planned files:

```text
measurement_types.h
acquisition.c/.h
phasor.c/.h
complex_math.c/.h
impedance.c/.h
autorange.c/.h
confidence.c/.h
calibration_apply.c/.h
measurement_engine.c/.h
```

### Core data types

Representative types:

```c
typedef struct
{
    float re;
    float im;
} complexf_t;

typedef struct
{
    complexf_t vexc;
    complexf_t vmid;
    complexf_t ret;
} phasor_set_t;

typedef struct
{
    complexf_t z;
    float magnitude;
    float phase_rad;
    float resistance;
    float reactance;
    float capacitance_f;
    float inductance_h;
    float esr;
    float q;
    float d;
} measurement_result_t;
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

Baseline approach: synchronous detection / single-bin DFT.

For each channel:

```text
I = Σ x[n] cos(ωn)
Q = Σ x[n] sin(ωn)
V = scale * (I + jQ)
```

Windows should contain an integer number of cycles where practical.

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

### Autorange output

The decision engine may return:

- accept;
- change RREF;
- change 1X/HG channel;
- change frequency;
- change amplitude;
- retry;
- reject.

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

A conceptual layout may reserve separate sectors for superblock, redundant settings, calibration, and asset pack. Exact addresses must not be frozen until the minimum supported W25Q density is decided.

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

Planned files:

```text
ui_core.c/.h
ui_theme.c/.h
ui_format.c/.h
ui_navigation.c/.h
ui_widgets.c/.h
screen_startup.c/.h
screen_measure.c/.h
screen_details.c/.h
screen_graph.c/.h
screen_settings.c/.h
screen_calibration.c/.h
screen_diagnostics.c/.h
```

Rules:

- no blocking screen code;
- no `delay()`-style animation;
- render changed regions where practical;
- stream large bitmaps from W25Q;
- keep SI-prefix/unit formatting separate from calculation;
- measurement logic does not depend on the active screen.

## Asset pack

Suggested simple structure:

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
crc32
```

Candidate formats:

- raw RGB565;
- simple RLE over RGB565;
- 1/4/8-bit alpha or glyph masks.

Host tooling converts source PNG/font assets to the firmware format.

## Settings

Compiled defaults must exist even if external Flash is invalid.

Examples:

```text
backlight brightness
auto-dim timeout
buzzer enabled
preferred display mode
measurement auto/manual policy
log level
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

## Host-side tests

Required focus areas:

- complex math;
- DFT/phasors;
- impedance equation;
- OPEN/SHORT/LOAD correction;
- autorange;
- confidence gates;
- CRC/record parsing;
- asset manifest;
- pure application state machine.

Synthetic vectors should cover ideal R/C/L components plus noise, clipping, offsets, and timing/phase errors.

## Observability

A measurement session should be able to produce compact diagnostic metadata:

```text
session id
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
result
confidence
retry/rerange reason
```

Release builds may reduce this data; Lab builds should expose it through UART/TFT.

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
