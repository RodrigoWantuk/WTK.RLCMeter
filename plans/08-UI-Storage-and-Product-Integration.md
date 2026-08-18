# 08 — UI, Storage, and Product Integration

STATUS: NOT_STARTED

## Goal

Turn the validated measurement engine and peripheral foundations into a coherent Rev.1 product experience without allowing UI/storage activity to compromise acquisition determinism or safety.

## Prerequisites

- Phase 03 display/Flash/input peripherals validated;
- Phase 04 safety/range services integrated;
- Phase 05 acquisition stable;
- Phase 06 measurement result API stable;
- Phase 07 autorange/confidence/calibration interfaces available.

## In scope

- full navigation model;
- startup/self-test presentation;
- main measurement screen;
- detailed impedance screen;
- derived graphs/visualizations;
- settings;
- calibration wizard UI;
- diagnostics screen/console;
- final asset-pack format/tooling;
- settings persistence;
- power/idle/backlight policy;
- buzzer patterns;
- integration state-machine hardening;
- memory/performance optimization.

## Out of scope

- changing metrology equations for UI convenience;
- adding Rev.2 hardware features;
- declaring final metrology qualification before Phase 09.

## Task 1 — Freeze UI state model

Define screens and navigation independently from raw button GPIO.

Baseline screens:

```text
STARTUP
MEASURE
DETAILS
GRAPH
SETTINGS
CALIBRATION
DIAGNOSTICS
ABOUT/INFO
```

Navigation consumes button events and application state.

Safety warnings must be able to interrupt/override normal navigation.

## Task 2 — Main measurement screen

Prioritize:

- dominant R/L/C/Z value;
- SI prefix/unit;
- frequency;
- confidence (`NOMINAL`, `EXTENDED`, etc.);
- relevant AUTO/manual status;
- battery/charger state;
- safety/fault indication.

Do not overload the primary screen with engineering diagnostics.

## Task 3 — Detail screen

Expose:

- `|Z|`;
- phase;
- R and X;
- ESR/Q/D when valid;
- selected RREF;
- frequency/amplitude;
- 1X/HG channel information in Lab/diagnostic builds;
- confidence reason when useful.

Derived values must display “not applicable” rather than misleading infinities when the equivalent model is invalid.

## Task 4 — Graph/visualization screen

Implement derived visualizations such as:

- phase vector diagram;
- resistive/capacitive/inductive tendency;
- comparison across 100 Hz / 1 kHz / 10 kHz when measurements exist;
- inductor/coils phase visualization;
- calculated capacitor charge/discharge curve based on measured C and selected external parameters.

Clearly separate calculated/derived visualizations from actual captured time-domain waveforms.

## Task 5 — Rendering engine

Implement:

- RGB565 primitives;
- text/glyph rendering;
- dirty-region or explicit-region updates;
- no full-screen framebuffer;
- chunked image streaming;
- clipping/bounds checks;
- predictable memory usage.

Profile large screen transitions and keep them outside quiet mode/acquisition.

## Task 6 — Final asset pack

Freeze an asset-pack format with:

```text
header/version
asset count
entries
blob payloads
CRC
```

Each entry should contain stable ID, offset, length, dimensions, format, flags, and integrity information as needed.

Host tooling must:

- convert source assets;
- generate the pack deterministically;
- validate bounds/CRC;
- optionally generate a C header of stable asset IDs.

The firmware must degrade to internal fallback assets if the pack is absent/corrupt.

## Task 7 — Settings schema

Define persistent settings such as:

- backlight brightness;
- auto-dim timeout;
- buzzer enabled;
- preferred measurement display;
- auto/manual preference where applicable;
- diagnostic log level for Lab builds.

Safety interlocks are not user-configurable.

Use versioned CRC-protected records through the storage layer.

## Task 8 — Calibration wizard UI

Guide OPEN/SHORT/LOAD operations explicitly.

Requirements:

- show required fixture/action;
- refuse progress if charger/residual/safety conditions are invalid;
- show stability/progress;
- identify current frequency/range/amplitude condition;
- confirm persistence success;
- allow cancellation that returns to SAFE without corrupting existing calibration.

## Task 9 — Diagnostics screen

Display engineering data sufficient for bench work:

```text
firmware/hardware revision
reset reason/uptime
application state
VMID/VEXC/RET summaries
raw auxiliary ADC values
battery/NTC/charger
RREF/K1/K2
W25Q status
TFT status
clipping/SNR/confidence
last fault
```

Lab builds may expose more detail than Release.

## Task 10 — Event console

Implement bounded ring-buffer logging for TFT display and UART output.

Requirements:

- fixed memory usage;
- no heap;
- timestamps from monotonic time;
- filtering by log level;
- quiet-mode suppression of high-volume output;
- no logging from critical ISR paths beyond compact counters/flags.

## Task 11 — Backlight/power policy

Implement:

- user brightness;
- inactivity dimming;
- optional sleep/off behavior supported by hardware;
- stable acquisition behavior;
- battery-low adjustments if desired.

Do not create a power-saving mode that changes analog conditions silently during an active measurement.

## Task 12 — Buzzer patterns

Create non-blocking patterns for:

- button confirmation;
- valid measurement;
- invalid action;
- residual-voltage warning;
- low battery;
- calibration completion;
- critical fault.

Quiet mode and safety policy can mute/cancel patterns immediately.

## Task 13 — Application integration

Harden full state flow:

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
RETRY/RERANGE or RESULT
FAULT
```

Add tests for user actions during transitions, cancellations, faults, charger insertion, low battery, missing Flash, and TFT failure.

## Task 14 — Memory/size review

Use link map/size reports to record:

- Flash usage;
- RAM usage;
- acquisition buffers;
- UI buffers;
- log ring buffer;
- stack margin strategy if measurable.

Do not allow UI feature growth to consume metrology buffer margin silently.

## Task 15 — Responsiveness review

Measure or instrument worst-case `*_step()` latency.

Ensure:

- button handling remains responsive outside acquisition;
- large rendering/storage tasks are chunked;
- watchdog service remains reliable;
- acquisition start cannot be delayed unpredictably by a UI operation once scheduled.

## Automated acceptance criteria

- host tests for formatting/navigation/state transitions pass;
- asset pack generator produces deterministic validated output;
- settings corruption/recovery tests pass;
- no full framebuffer exists;
- no screen code drives hardware safety GPIOs;
- embedded builds fit within documented Flash/RAM budgets;
- previous DSP/safety tests remain green.

## Bench acceptance criteria

1. boot with valid W25Q assets;
2. boot with missing/corrupt asset pack and verify fallback diagnostics;
3. navigate all screens/buttons;
4. perform repeated measurements while observing UI responsiveness;
5. verify quiet mode suppresses disruptive TFT/Flash/buzzer activity;
6. power-cycle settings/calibration persistence;
7. test charger insertion/residual faults during UI operation;
8. validate backlight/buzzer behavior and rail stability;
9. review diagnostic screen against DMM/scope/UART values.

## Handoff

Report:

- final screen/navigation structure;
- asset format/tool version;
- settings schema version;
- measured Flash/RAM usage;
- worst observed UI/update latencies;
- fallback behavior;
- known UX limitations;
- readiness for formal Phase 09 qualification.
