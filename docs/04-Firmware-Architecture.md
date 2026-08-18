# Firmware Architecture

## Principles

The firmware is organized as instrument firmware, not as a microcontroller demo:

- SAFE is the default state;
- acquisition is deterministic;
- UI and storage do not block measurement;
- DSP is isolated from device-specific peripherals;
- failure of any required precondition returns K1 to SAFE;
- settings and calibration are versioned and CRC-protected;
- command-line CMake builds are canonical;
- Visual Studio Code is the primary supported editor workflow;
- the implementation language is C17;
- Arduino/INO and C++ are outside the current baseline.

## Main state machine

```text
BOOT
  |
  v
SELF_TEST
  |
  v
SAFE_CHECK <-----------------------------+
  |                                      |
  +-- residual/charger/fault --> WAIT ---+
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

The DUT should remain connected to the measurement AFE only for the time required to settle and acquire.

## Boot sequence

1. clock and watchdog;
2. safe GPIO defaults;
3. disable JTAG while keeping SWD to free PA15/PB3/PB4;
4. diagnostic UART;
5. SPI bus;
6. W25Q external Flash;
7. ILI9341 display;
8. ADC/DMA/timers;
9. persisted settings/calibration validation;
10. self-test;
11. `SAFE_CHECK`.

A display or external-Flash failure must not bypass safety policy.

## BSP

`bsp` encapsulates:

- clock tree;
- GPIO;
- SPI2;
- USART1;
- ADC1/ADC2;
- DMA;
- TIM1/TIM2/TIM3/TIM4;
- watchdog;
- monotonic time;
- reset reason.

Higher layers must not write STM32 registers or arbitrary GPIOs directly.

## Drivers

### ILI9341

Minimum API responsibilities:

```text
init
reset
read_id/status
set_rotation
set_window
fill
write_pixels_rgb565
draw_bitmap primitive
draw_glyph primitive
```

There is no 240×320×16-bit framebuffer in MCU RAM.

### W25Q

Generic API responsibilities:

```text
init / read_jedec_id
read
fast_read
write_enable
page_program
sector_erase
read_status
wait_ready
```

The driver should recognize compatible 3.3 V W25Q16/32/64/128 densities rather than hard-code W25Q64 behavior across the firmware.

### Buttons

Debounce produces events:

```text
PRESS
RELEASE
LONG_PRESS
REPEAT
```

The button driver does not own UI navigation policy.

## Hardware services

Higher layers use semantic services rather than GPIOs:

```text
relay_set_safe()
relay_set_measure()
range_disable()
range_select(range)
excitation_configure(freq, amplitude)
excitation_stop()
charger_connected()
battery_read()
temperature_read()
buzzer_play(pattern)
backlight_set(percent)
```

## Acquisition

The acquisition module delivers synchronized raw sample blocks. It does not compute impedance.

DSP consumes buffers plus metadata such as:

```text
frequency
sample_rate
range
excitation amplitude
channel timing/skew
RET channel
calibration key
```

and returns complex phasors, statistics, and quality information.

## Quiet mode

During critical acquisition windows:

- buzzer is always off;
- large TFT transfers are suspended;
- external Flash is not accessed unless explicitly required;
- backlight PWM remains stable or may be frozen to a qualified condition if bench tests show coupling;
- high-volume UART logging is suspended.

Results are preferably rendered after K1 returns to SAFE.

## TIM3 and buzzer

PB0 and PB1 map to TIM3 channels, therefore two hardware-PWM outputs on those pins would share the same timer base frequency.

Recommended baseline:

- TIM3_CH3: continuous backlight PWM on PB0;
- TIM4: buzzer timebase;
- PB1: GPIO toggled according to the requested tone.

This keeps backlight and audible-tone frequencies independent. The piezo remains silent during acquisition.

## Persistence

External Flash is logically divided into:

- asset pack;
- calibration records;
- settings;
- optional diagnostic/event data.

No filesystem is planned initially.

Persistent records contain fields equivalent to:

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

Settings/calibration must use redundant slots or a small journal strategy to survive interrupted writes.

## Diagnostics

A compact ring buffer may be displayed on TFT and streamed through UART.

Log levels:

```text
ERROR
WARN
INFO
DEBUG
TRACE   # laboratory builds only
```

The firmware should remain useful for bring-up without relying on breakpoints.

## Build and editor policy

The repository must eventually support:

```bash
cmake --preset <preset>
cmake --build --preset <preset>
ctest --preset <preset>
```

or equivalent documented CMake commands.

Visual Studio Code integrates through the checked-in workspace and CMake Tools, but no VS Code-specific metadata may become required for non-editor builds.

Detailed module contracts are documented in [`13-Detailed-Firmware-Design.md`](13-Detailed-Firmware-Design.md), while the execution sequence is defined in [`../plans/`](../plans/).
