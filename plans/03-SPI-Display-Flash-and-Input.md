# 03 — SPI, Display, Flash, and User Input

STATUS: NOT_STARTED

## Goal

Implement and validate the non-metrology user-interface peripherals while preserving shared-SPI correctness and future quiet-mode requirements.

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
- minimal asset-streaming primitive;
- peripheral diagnostics.

## Out of scope

- final product screens;
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
- minimal bitmap/glyph primitive;
- optional ID/status readback where the module exposes usable MISO behavior.

Avoid a full framebuffer.

## Task 6 — TFT fallback path

Keep a minimal built-in font/diagnostic rendering path in MCU Flash. The instrument should display a basic error even if W25Q assets are unavailable.

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
- simultaneous-button state should be deterministic enough for future shortcuts.

## Task 8 — Backlight PWM

Configure PB0 / TIM3_CH3.

Define:

- PWM frequency high enough to avoid visible flicker;
- safe duty range;
- `backlight_set(percent)` API;
- stable behavior during quiet mode;
- default boot brightness policy.

Do not select a PWM frequency without checking timer clock from Phase 02.

## Task 9 — Buzzer

Use PB1 through the existing transistor/piezo circuit.

Because PB0 and PB1 are TIM3 channels and would share the timer base, prefer the documented architecture:

- TIM3 for backlight PWM;
- TIM4 timebase / software toggle for PB1 tones.

Implement a small non-blocking pattern player rather than `delay()`-based beeps.

The buzzer must have an unconditional mute path for quiet mode/safety faults.

## Task 10 — Minimal asset streaming

Implement enough infrastructure to prove:

```text
W25Q -> small RAM buffer -> ILI9341
```

Test with an intentionally larger bitmap that cannot reasonably be treated as a tiny inline icon. Verify that no full image buffer is allocated in RAM.

Do not freeze the final compression format prematurely.

## Task 11 — Peripheral self-test

Add self-test results for:

- W25Q presence/capacity;
- TFT init/status;
- button sanity;
- optional backlight/buzzer diagnostic actions.

A W25Q/TFT failure should degrade UI capability but must not bypass safety functionality in later phases.

## Automated acceptance criteria

- embedded build succeeds;
- host tests remain green;
- W25Q page/bounds logic has host tests where pure logic can be separated;
- button debounce/event state machine has host tests;
- no framebuffer-size static allocation appears;
- bus arbitration guarantees mutual CS exclusion by construction or centrally enforced API.

## Bench acceptance criteria

1. read expected W25Q JEDEC ID;
2. write/read/erase reserved test sector;
3. initialize TFT and render solid colors/text;
4. stream a bitmap from W25Q to TFT in chunks;
5. verify buttons generate expected events;
6. verify backlight brightness control;
7. verify buzzer tones near 500 Hz / 1 kHz / 2 kHz;
8. verify no reset or obvious rail disturbance;
9. verify TFT MISO releases the bus with CS inactive.

## Handoff

Report:

- validated SPI frequencies;
- detected Flash part/capacity;
- display module behavior/readback limitations;
- RAM used by asset streaming;
- button timing constants;
- backlight PWM frequency;
- buzzer implementation/timing;
- any quiet-mode concerns for later acquisition work.
