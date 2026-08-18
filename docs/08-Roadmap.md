# Roadmap

The roadmap is intentionally ordered so that safety and observability exist before metrology features depend on them. Detailed AI-agent execution plans live under [`../plans`](../plans/).

## M0 — Hardware Rev.1

- fabricate PCB;
- assemble by functional blocks;
- validate shorts and supply rails;
- freeze the actual assembled BOM;
- record DNP population;
- capture the exact hardware revision used for calibration.

## M1 — Firmware bootstrap

- C17 project foundation;
- CMake toolchain and presets;
- VS Code workspace integration;
- host-test harness;
- startup/clock;
- safe GPIO defaults;
- JTAG remap while preserving SWD;
- UART;
- watchdog;
- base diagnostics.

## M2 — UI and storage peripherals

- SPI2 bus abstraction;
- W25Q JEDEC/read/write/erase;
- ILI9341 initialization and incremental rendering primitives;
- buttons;
- backlight;
- buzzer;
- initial asset-pack tooling.

## M3 — Safety and range control

- residual-voltage ADC channels;
- battery and NTC sensing;
- charger detection;
- K1/K2 services;
- 74HC238 one-hot range selection;
- SAFE/READY/MEASURE state-machine enforcement.

## M4 — Excitation and acquisition

- TIM1 PWM carrier;
- excitation amplitude generation/policy;
- ADC1/ADC2 configuration;
- deterministic trigger timer;
- DMA buffers;
- raw-capture diagnostics through UART;
- quiet mode.

## M5 — DSP and impedance

- synchronous I/Q / single-bin DFT;
- channel reconstruction;
- complex Z calculation;
- R/X/phase;
- series-equivalent L/C;
- clipping/SNR/confidence metrics;
- host-side known-vector tests.

## M6 — Autorange

- range search policy;
- amplitude policy;
- 1×/HG channel selection;
- settling policy;
- retry/fallback rules;
- explicit rejection states.

## M7 — Calibration

- OPEN/SHORT/LOAD workflow;
- persistent calibration records;
- complex correction;
- schema/versioning;
- calibration import/export tooling where useful;
- qualification-map generation.

## M8 — Rev.1 product experience

- complete measurement UI;
- graphs/derived visualizations;
- battery UX;
- diagnostic console;
- power/idle policy;
- asset pack;
- qualification matrix integrated into confidence behavior.

## M9 — Rev.1 qualification and Rev.2 decision

Rev.2 should be driven by measured Rev.1 limitations rather than speculation.

Possible items:

- lower high-Z leakage / OFF capacitance;
- Kelvin/4-wire support;
- MCU/pinout with native USB available;
- more integrated power control;
- qualified TVS protection;
- smaller PCB;
- separate voltage-measurement front-end.

## Outside Rev.1

Potential direct voltage measurement around 400 Vrms AC and 600–800 VDC has been discussed as a future feature. It requires a separate front-end, connectors, clearance/creepage strategy, protection analysis, and safety validation. It must not be implemented as an improvised extension of the current RLC input.
