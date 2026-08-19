# 05 — Excitation, ADC, and DMA

STATUS: IN_PROGRESS

## Stage 1 status

```text
Stage 1 — Deterministic excitation + dual-ADC/DMA raw-capture foundation
STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION
```

Stage 1 delivers host-tested policy/contract modules, STM32 BSP for TIM1 excitation and dual-ADC DMA capture, Lab-only `lab metrology capture`, and fail-safe abort behavior. It does **not** energize K1, implement DSP, or connect production MEASURE.

## Stage 2 status

```text
Stage 2 — Lab DUT measurement with K1 ownership and permit-gated energize
STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION
```

Stage 2 adds `hw_metrology_measure` (host-testable FSM), Lab-only `lab metrology measure`, K1 ownership during the authorized measure window, single-use measurement permit issue/validate (5 ms TTL), and Stage 1 session closure fixes (cleanup fault latching, transient ADC restore handling). It does **not** implement DSP, autorange, product short-OK, or K2 changes.

### Stage 2 DUT measure contract

```text
Mode: DUT_MEASURE only (raw capture remains hw_metrology_session / lab metrology capture)
K1_OPERATE_GUARD_MS = 10
K1_RELEASE_GUARD_MS = 8
Permit: hw_measure_permit_issue/validate, single-use, TTL 5 ms, issued after neutral settle
K1 request: hw_k1_request_measure only after successful permit validate in same attempt
Shell K1 policy: hw_k1_force_safe skipped while hw_metrology_measure_k1_owned()
Success tail: DMA stop → excitation NEUTRAL → 1 ms → K1 SAFE → 8 ms release guard → excitation OFF
Emergency abort during K1 MEASURE: stop ADC/TIM2, excitation OFF immediately, K1 SAFE, range disable,
  8 ms release guard if K1 reached MEASURE before aux resume, quiet off after hazardous cleanup
Dynamic blockers during K1 MEASURE: fault mask, charger != ABSENT, range not READY/wrong id, exc/ADC DMA error
No abort for residual UNKNOWN/stale while aux is paused
Cleanup faults: K1_IO, RANGE_IO, ADC_RUNTIME, METROLOGY_RUNTIME latched per return status; primary session error preserved
Transient ADC restore: first failure latches ADC_RUNTIME permanently; later success may resume aux but block.valid=false
```

Lab command:

```text
lab metrology measure <100|1k|10k> <100m|500m> <10r|100r|1k|10k|100k|1m>
```

Capture and measure are mutually exclusive. Raw dump header includes `mode=DUT_MEASURE` and permit/guard metadata when applicable.

## Authoritative Rev.1 Stage 1 Metrology Contract

The following values are frozen for Rev.1 Stage 1. Future stages must not silently change them.

### Metrology clock prerequisite

```text
source          = HSE_PLL
hse_ready       = true
SYSCLK/HCLK     = 72 MHz
PCLK1           = 36 MHz
PCLK2           = 72 MHz
TIM APB1/APB2   = 72 MHz
ADC clock       = 12 MHz
```

Anything else: metrology not ready. Boot latches `APP_SAFETY_FAULT_CLOCK` when the contract fails.

### Excitation (TIM1 / PA8)

```text
PSC=0, ARR=159  => carrier 450 kHz exact
LUT             = 45-point signed Q15 sine (const Flash, no libm)
states          = OFF | NEUTRAL (CCR1=80) | SINE (DMA circular CCR table)
RCR             = 99 @100 Hz, 9 @1 kHz, 0 @10 kHz
amplitudes      = 100 mVrms (PEAK_Q8=1755), 500 mVrms (PEAK_Q8=8777)
forbidden       = 500 mVrms on 10 Ω RREF
neutral settle  = 1 ms   (REQUIRES_BENCH_VALIDATION)
sine settle     = max(8 cycles, 5 ms) => 80/8/5 ms @100/1k/10k (REQUIRES_BENCH_VALIDATION)
DMA             = DMA1 Channel 5, mem->TIM1_CCR1, 16-bit circular x45, priority HIGH, TE IRQ only
```

### Dual ADC capture

```text
ADC1+ADC2       = dual regular simultaneous, 12-bit, scan, 7.5-cycle sample time
ADC1 sequence   = VEXC, VEXC, VMID
ADC2 sequence   = RET_1X, RET_HG, VMID
trigger         = TIM2_CC2 internal only; PA1 remains ADC_VMID (never TIM2 GPIO out)
DMA             = DMA1 Channel 1, ADC1->DR packed 32-bit, VERY HIGH, non-circular
buffer          = uint32_t raw_words[768] (256 instants x 3 words = 3072 B SRAM)
clip rails      = raw <= 16 or >= 4079 invalid (reuse residual policy)
```

### Sample-rate table

| Excitation | SPS    | TIM2 ARR | TIM2 CCR2 | spc | cycles | block samples |
|------------|--------|----------|-----------|-----|--------|---------------|
| 100 Hz     | 6400   | 11249    | 5625      | 64  | 4      | 256           |
| 1 kHz      | 64000  | 1124     | 562       | 64  | 4      | 256           |
| 10 kHz     | 160000 | 449      | 225       | 16  | 16     | 256           |

### Stage 1 scope limits

- K1 remains SAFE; no production `hw_k1_request_measure()` call sites.
- Lab command: `lab metrology capture <100|1k|10k> <100m|500m> <10r|100r|1k|10k|100k|1m>`
- No DSP (Vs/Vx/Z/DFT/R/L/C) in Stage 1.
- Aux ADC1 ownership: pause before metrology, restore after capture; eight fresh SAFE residuals required after resume.

Tasks 1–5 below that ask agents to *choose* carrier, sample rates, ADC schedule, or buffer topology are **superseded** by this contract.

## Responsibility boundary

Firmware implementation for this phase must not independently choose excitation topology, PWM strategy, ADC1/ADC2 assignment, sample-rate table, DMA layout, channel schedule, or analog timing. Those items require an authoritative electronics/metrology contract before Phase 05 implementation begins.

Task language below that asks the implementation agent to verify, choose, or design consequential metrology parameters is a planning placeholder, not delegated authority. Do not start Phase 05 implementation until the missing contract is provided.

Phase 04 hands off these future K1 guard rules:

```text
K1_OPERATE_GUARD_MS = 10
K1_RELEASE_GUARD_MS = 8
```

Both guard times remain `REQUIRES_BENCH_VALIDATION`. After K1 returns LOW and the 8 ms release guard completes, old residual evidence is invalid; auxiliary ADC must reacquire eight fresh SAFE residual evaluations before any new measurement permit can be issued.

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
