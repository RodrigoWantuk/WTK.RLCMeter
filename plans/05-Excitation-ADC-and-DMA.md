# 05 — Excitation, ADC, and DMA

STATUS: NOT_STARTED

## Goal

Create and validate the deterministic metrology transport: filtered PWM excitation, timer-driven ADC sampling, DMA buffers, raw-capture observability, and quiet-mode coordination.

This phase deliberately stops before final impedance DSP.

## Prerequisites

- Phase 02 complete;
- Phase 04 safety/range services implemented;
- actual analog rails/VMID validated enough to power the AFE safely;
- oscilloscope available for bench acceptance.

## In scope

- TIM1 PWM carrier on PA8;
- excitation frequency/amplitude generation policy;
- ADC1/ADC2 metrology configuration;
- deterministic sample trigger timer;
- DMA buffering;
- sample metadata;
- ADC calibration/offset startup steps supported by STM32F1;
- raw sample export/logging;
- clipping/headroom primitives;
- quiet mode.

## Out of scope

- final phasor extraction;
- impedance calculation;
- autorange intelligence;
- final calibration correction.

## Task 1 — Freeze excitation carrier strategy

Verify the planned ~450 kHz carrier against:

- Phase 02 timer clocks;
- TIM1 resolution;
- 3-stage RC filter response;
- desired 100 Hz / 1 kHz / 10 kHz output frequencies;
- amplitude-control method;
- spectral leakage/ripple expectations.

Document actual carrier frequency and timer parameters rather than relying on the nominal plan.

## Task 2 — Generate AC excitation

Implement excitation generation on TIM1_CH1 / PA8.

Requirements:

- safe inactive/neutral state during boot and faults;
- supported measurement frequencies: 100 Hz, 1 kHz, 10 kHz;
- 100 mVrms and 500 mVrms policy targets where hardware permits;
- prohibit 500 mVrms on 10 Ω RREF;
- configuration API returns failure for impossible combinations;
- changing excitation does not accidentally leave uncontrolled output during transitions.

The exact modulation strategy must be documented and measured.

## Task 3 — Excitation settling

Define a settling policy based on:

- RC filter time constants;
- DUT/RREF behavior;
- frequency;
- amplitude changes;
- range changes.

Use a non-blocking state/timestamp policy rather than arbitrary application-level `delay()` calls.

Initial constants may be conservative and later tuned from bench data.

## Task 4 — ADC channel schedule

Design the exact ADC1/ADC2 acquisition schedule for:

```text
VEXC
VMID
RET_1X
RET_HG
```

The design must explicitly answer:

- which channels are simultaneous vs sequential;
- ADC sample time per channel;
- conversion order;
- deterministic skew;
- sample rate;
- number of samples/cycle;
- cycles/block;
- whether both RET channels are always captured or selected per session.

This is a consequential decision: document it in `docs/10-Consolidated-Design-Decisions.md` if the final topology was not already fixed.

## Task 5 — Sample-rate design

Choose sample rates for 100 Hz / 1 kHz / 10 kHz that support synchronous extraction efficiently.

Goals:

- integer or deliberately controlled samples per cycle where practical;
- sufficient points for phase/magnitude accuracy;
- manageable RAM and CPU load;
- deterministic relationship to excitation;
- no accidental aliasing with PWM carrier components.

Do not simply maximize ADC speed.

## Task 6 — Trigger timer

Use a hardware timer (candidate TIM2) as the metrology sampling trigger.

Requirements:

- sample cadence is independent of main-loop load;
- start/stop boundaries are deterministic;
- timestamp/metadata identifies the configured sample rate;
- SysTick is not used as sample timing.

## Task 7 — DMA buffers

Implement fixed-size buffers with explicit ownership.

Consider ping-pong/double buffering if continuous capture is required.

Rules:

- no heap;
- buffer dimensions derived/documented;
- ISR only marks half/full buffer readiness;
- overflow/overrun becomes an explicit fault/result condition;
- main-loop/DSP ownership cannot race DMA writes.

Document RAM use and inspect linker/map output.

## Task 8 — ADC startup/calibration

Implement STM32F1 ADC calibration procedure where applicable and define startup sequencing.

Expose diagnostics for:

- calibration success/failure;
- raw baseline values;
- impossible/saturated channels.

## Task 9 — Raw acquisition API

The acquisition layer should expose a block plus metadata, conceptually:

```text
session_id
frequency
sample_rate
sample_count
cycle_count
RREF
amplitude
channel schedule
raw sample buffers
flags/overrun status
```

Do not compute final impedance inside the DMA/BSP layer.

## Task 10 — Raw capture diagnostics

Provide a laboratory command or diagnostic mode that exports raw blocks through UART after acquisition.

Requirements:

- do not stream UART during the timing-critical capture if it perturbs acquisition;
- capture first, return to safe/quiet state, then transmit;
- output format should be parseable by host tooling.

## Task 11 — Clipping/headroom primitives

Add pure helpers that classify:

- near-zero rail;
- near-full-scale rail;
- usable amplitude range;
- saturated blocks.

Thresholds may be conservative and should be exposed as configuration/measurement metadata.

## Task 12 — Quiet mode

Implement a central quiet-mode state that, during capture:

- mutes buzzer;
- prevents large TFT writes;
- prevents unnecessary W25Q transactions;
- suppresses high-volume UART logging;
- keeps backlight PWM stable.

Entering/exiting quiet mode must be deterministic and recover after acquisition faults.

## Task 13 — Measurement session skeleton

Implement enough application flow to execute:

```text
SAFE_CHECK
PREPARE_RANGE
PRE_EXCITATION
K1_MEASURE
SETTLING
ACQUIRE
K1_SAFE
RAW_RESULT_READY
```

Final DSP states remain for Phase 06.

## Automated acceptance criteria

- embedded target builds;
- buffer sizes are static and documented;
- no acquisition ISR performs DSP/rendering/log formatting;
- clipping helpers have host tests;
- state-machine tests cover acquisition completion, timeout, overrun, and abort-to-SAFE;
- map/size report shows acceptable RAM margin.

## Bench acceptance criteria

Using an oscilloscope/reference equipment:

1. measure actual PWM carrier;
2. observe all RC filter stages and VEXC;
3. measure VEXC amplitude at each supported frequency/amplitude condition;
4. verify 10 Ω range never receives forbidden high amplitude;
5. inspect visible distortion/ripple;
6. capture raw VEXC/VMID/RET_1X/RET_HG blocks;
7. verify sample cadence and block length;
8. measure/estimate deterministic channel skew;
9. verify no ADC clipping under intended test conditions;
10. verify quiet mode prevents visible SPI/buzzer activity during acquisition;
11. confirm K1 returns SAFE before raw UART dump.

## Handoff

Report:

- actual carrier frequency;
- excitation-generation method;
- actual sample-rate table;
- ADC1/ADC2 schedule and skew;
- buffer dimensions/RAM use;
- raw-capture format/tooling;
- measured VEXC behavior;
- known acquisition artifacts;
- readiness for Phase 06.
