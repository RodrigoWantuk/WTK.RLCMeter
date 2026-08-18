# 03 — SPI, Display, Flash, and User Input

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

## Goal

Implement and validate the non-metrology user-interface peripherals while preserving shared-SPI correctness, future quiet-mode requirements, and the fixed button/resource architecture used by later product UI work.

## Prerequisites

- Phase 01 complete;
- Phase 02 BSP/clock/GPIO/UART foundation implemented;
- SPI2 pinout confirmed against `docs/05-Pinout-and-Interfaces.md`.

## In scope

- SPI2 BSP/driver boundary;
- shared-bus ownership policy;
- W25Q identification/read/program/erase;
- ILI9341 init/status/basic drawing;
- button debounce/events;
- backlight PWM;
- buzzer tone generation;
- minimal external resource/font-streaming primitive;
- peripheral diagnostics.

## Out of scope

- final product screens/navigation implementation;
- final asset-pack format if not needed yet;
- calibration storage;
- ADC/DMA metrology;
- measurement graphs driven by real results.

## Task 1 — SPI2 configuration

Configure PB13/PB14/PB15 for SPI2 and define:

- SPI mode required by ILI9341 and W25Q;
- initial conservative clock;
- maximum validated clocks per device;
- transaction boundaries;
- CS timing;
- blocking vs later DMA strategy.

The initial implementation may use blocking transfers because SPI is not in the metrology critical path, but APIs must permit chunked operation and quiet-mode suspension.

## Task 2 — Shared-bus ownership

Create an explicit rule or helper layer so:

- TFT_CS and FLASH_CS are HIGH when idle;
- only one device is selected at a time;
- device-specific SPI configuration changes are applied safely if needed;
- failed transactions release CS/bus ownership;
- later quiet mode can prevent new large transfers.

Do not rely on call-site discipline scattered across UI/storage code.

The bus abstraction must support the later resource-rendering sequence:

```text
select W25Q
read bounded resource chunk
release W25Q
select TFT
transmit/render chunk
release TFT
```

## Task 3 — W25Q driver

Implement:

- JEDEC ID;
- capacity decoding for compatible W25Q16/32/64/128;
- status-register reads;
- write enable;
- read / fast read;
- page program with page-boundary handling;
- sector erase;
- timeout-based wait-ready;
- bounds checking using detected capacity.

Do not erase/program arbitrary addresses during bring-up. Reserve a test sector through a clearly documented temporary policy.

## Task 4 — W25Q diagnostics

UART diagnostics should report:

```text
JEDEC manufacturer/device
capacity
status
read/write test result
```

Do not treat unknown JEDEC IDs as compatible without an explicit device table/rule.

## Task 5 — ILI9341 driver

Implement:

- hardware reset;
- initialization sequence;
- display rotation;
- address window;
- RGB565 pixel streaming;
- solid fill;
- basic procedural drawing primitives required for bring-up;
- minimal bitmap/glyph primitive;
- optional ID/status readback where the module exposes usable MISO behavior.

Avoid a full framebuffer.

## Task 6 — TFT fallback path

Keep a minimal built-in font/diagnostic rendering path in MCU Flash. The instrument should display a basic error even if W25Q resources are unavailable.

This fallback is intentionally small. Large/custom fonts belong to the W25Q resource architecture and must not be copied wholesale into internal MCU Flash merely for convenience.

## Task 7 — Buttons

Implement debounce for UP/OK/DOWN and emit events:

```text
PRESS
RELEASE
LONG_PRESS
REPEAT
```

Requirements:

- no blocking debounce delays;
- timing based on BSP monotonic time;
- driver does not own screen navigation;
- simultaneous-button state is deterministic enough for controlled future service shortcuts;
- ordinary product operation must not depend on button combinations, double-clicks, or triple-clicks.

Later UI code will consume these events according to the fixed product contract:

```text
normal/result context:
  OK short     measure
  UP/DOWN      browse result pages
  OK long      open menu

menu context:
  UP/DOWN      navigate/change
  OK short     select/confirm
  OK long      back
```

Do not hard-code those navigation semantics inside the button driver.

Host tests must cover press, release, long-press threshold boundaries, repeat behavior, bounce patterns, and event ordering.

## Task 8 — Backlight PWM

Configure PB0 / TIM3_CH3.

Define:

- PWM frequency high enough to avoid visible flicker;
- safe duty range;
- `backlight_set(percent)` API;
- stable behavior during quiet mode;
- default boot brightness policy.

Do not select a PWM frequency without checking timer clock from Phase 02.

Also provide the low-level support needed for Phase 08 to implement inactivity timeout and full-backlight-off behavior. The user-facing rule that the first button press after backlight-off only wakes the display belongs to Phase 08, not the BSP/driver.

## Task 9 — Buzzer

Use PB1 through the existing transistor/piezo circuit.

Because PB0 and PB1 are TIM3 channels and would share the timer base, prefer the documented architecture:

- TIM3 for backlight PWM;
- TIM4 timebase / software toggle for PB1 tones.

Implement a small non-blocking pattern player rather than `delay()`-based beeps.

The buzzer must have an unconditional mute path for quiet mode/safety faults and a normal enable/disable control that Phase 08 can bind to the persistent Sound setting.

Do not freeze the policy for safety-critical audible alerts versus user Sound=Off in this phase; that policy must be explicit before release.

## Task 10 — Minimal external resource/font streaming

Implement enough infrastructure to prove that the W25Q can act as external UI resource ROM rather than only as bitmap storage.

Prove both of these paths:

```text
W25Q -> small fixed RAM buffer -> ILI9341 bitmap/pixel stream
W25Q -> glyph/resource lookup -> small fixed RAM buffer -> ILI9341 text/glyph rendering
```

Requirements:

- do not allocate a full image or full font in RAM;
- total installed resource/font size must not cause proportional SRAM usage;
- target an initial scratch buffer in the hundreds of bytes where practical;
- if a larger scratch buffer is selected, document measured reason and RAM impact;
- support stable resource IDs/offsets rather than embedding pointer-like assumptions;
- keep the design compatible with offline-generated raster font packs containing metrics and glyph data;
- the MCU must not parse TTF/OTF or integrate FreeType.

For bring-up, use a deliberately non-trivial external resource that demonstrates that the bytes actually come from W25Q. A large numeric/font glyph set is preferred because it validates the intended product use better than a decorative full-screen bitmap.

Do not freeze final compression or localization formats prematurely.

## Task 11 — Quiet-mode hooks

Define a small interface that allows the later acquisition layer to prevent new TFT/W25Q/buzzer activity during a metrology-critical window.

Requirements:

- a transfer already in progress must have bounded completion time;
- no background UI/resource task starts an unbounded SPI transaction while quiet mode is requested;
- buzzer output can be stopped/muted deterministically;
- later Phase 08 can update the UI between acquisition attempts rather than during them.

Do not implement measurement policy here.

## Task 12 — Peripheral self-test

Add self-test results for:

- W25Q presence/capacity;
- TFT init/status;
- external resource read/glyph-stream sanity;
- button sanity;
- optional backlight/buzzer diagnostic actions.

A W25Q/TFT failure should degrade UI capability but must not bypass safety functionality in later phases.

## Automated acceptance criteria

- embedded build succeeds;
- host tests remain green;
- W25Q page/bounds logic has host tests where pure logic can be separated;
- button debounce/event state machine has host tests;
- no framebuffer-size static allocation appears;
- no large font/asset table is moved into MCU internal Flash as a shortcut;
- bus arbitration guarantees mutual CS exclusion by construction or centrally enforced API;
- resource streaming uses bounded fixed RAM;
- quiet-mode API has deterministic state behavior.

## Bench acceptance criteria

1. read expected W25Q JEDEC ID;
2. write/read/erase reserved test sector;
3. initialize TFT and render solid colors/basic procedural graphics/text;
4. stream an external bitmap/resource from W25Q to TFT in chunks;
5. render at least one externally stored custom/large glyph set from W25Q without loading the complete font into RAM;
6. verify buttons generate expected press/long/repeat events;
7. verify backlight brightness control;
8. verify buzzer tones near 500 Hz / 1 kHz / 2 kHz;
9. verify quiet-mode request blocks new disruptive peripheral activity;
10. verify no reset or obvious rail disturbance;
11. verify TFT MISO releases the bus with CS inactive.

## Handoff

Report:

- validated SPI frequencies;
- detected Flash part/capacity;
- display module behavior/readback limitations;
- RAM used by resource/font streaming;
- tested glyph/resource format assumptions that remain provisional;
- button timing constants/event behavior;
- backlight PWM frequency;
- buzzer implementation/timing;
- quiet-mode behavior/latency;
- any concerns for later acquisition or final UI work.

## Phase 03 implementation status

Implemented on 2026-08-18 as the first SPI/display/Flash/input foundation. Bench validation remains required before this phase can be considered complete.

### Implemented

- Fixed the Phase 02 HSI fallback before starting Phase 03 so `bsp_clock_summary_t` matches hardware after HSE/PLL failure.
- Added SPI2 BSP APIs and a central shared-bus layer for W25Q/ILI9341 mutual CS exclusion.
- Added a quiet-mode request hook that prevents new shared-SPI acquisitions.
- Added W25Q JEDEC/capacity decoding for W25Q16/32/64/128, range checks, page-boundary span logic, bounded reads, and stateful page-program/sector-erase start/poll APIs.
- Added ILI9341 reset/init stepping, rotation, address-window setup, RGB565 pixel writes, and chunked solid-fill support without a framebuffer.
- Added non-blocking button debounce with `PRESS`, `RELEASE`, `LONG_PRESS`, and `REPEAT` events.
- Added TIM3_CH3 backlight PWM service on PB0 with a 1 kHz baseline.
- Added TIM4-driven buzzer tone service on PB1 with a deterministic mute path.
- Added resource-pack header/entry validation, a 256-byte UI resource streaming scratch buffer, replaceable font backend contracts, and a tiny fallback font.
- Added UART boot diagnostics for SPI2, W25Q probe result, W25Q JEDEC/capacity when present, backlight PWM, and buzzer init status.

### Provisional design choices

```text
Initial SPI mode:       mode 0
Initial SPI prescaler:  PCLK1 / 8
Resource scratch:       256 bytes
Backlight PWM:          1 kHz, 0-100%
Buzzer supported range: 100-4000 Hz, intended bring-up points 500 Hz / 1 kHz / 2 kHz
W25Q bring-up sector:   final sector of detected capacity, reserved for controlled bench testing only
```

The font/resource work intentionally does not select MCUFont or a custom final renderer. The backend remains callback-based until real Release Flash/RAM headroom and asset-pack needs are measured.

### Automated validation

Automated validation passed:

```bash
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug

cmake --preset host-release
cmake --build --preset host-release
ctest --preset host-release

cmake --preset stm32-debug
cmake --build --preset stm32-debug

cmake --preset stm32-release
cmake --build --preset stm32-release

cmake --preset stm32-lab
cmake --build --preset stm32-lab
```

Release memory report from the implemented firmware:

```text
FLASH: 9940 B / 64 KiB, 15.17%
RAM:   2296 B / 20 KiB, 11.21%
```

### REQUIRES_BENCH_VALIDATION

- SPI2 signal integrity and conservative/maximum validated clocks.
- W25Q JEDEC ID, capacity detection, status read, reserved-sector erase/program/read.
- ILI9341 reset/init, rotation, solid fills, pixel streaming, and MISO release with CS inactive.
- Shared-bus CS exclusion on the physical board.
- External resource/glyph streaming from actual W25Q contents to TFT.
- Button debounce/event behavior on the real panel harness.
- Backlight PWM brightness and noise behavior.
- Buzzer output near 500 Hz, 1 kHz, and 2 kHz.
- Quiet-mode request blocking new TFT/W25Q/buzzer activity during future acquisition windows.
- No watchdog starvation during pollable W25Q erase/program operations.
- No reset or obvious rail disturbance during TFT/backlight/buzzer activity.
