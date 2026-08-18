# 09 — Bring-up, Qualification, and Rev.1 Release

STATUS: NOT_STARTED

## Goal

Convert an implemented firmware/hardware prototype into a defensible Rev.1 instrument baseline by collecting evidence, qualifying measurement regions, validating safety/fault behavior, and freezing release artifacts/known limitations.

This phase is intentionally bench-heavy. An AI agent can prepare scripts, matrices, analysis, and documentation, but cannot replace physical measurement evidence.

## Prerequisites

- Phases 01–08 implemented to the level required for a complete measurement session;
- assembled Rev.1 hardware with exact BOM/DNP state recorded;
- reference DMM/oscilloscope and, ideally, a trusted LCR/reference setup available;
- repeatable DUT fixtures and calibration standards available.

## In scope

- staged hardware bring-up evidence;
- safety validation;
- excitation characterization;
- ADC timing/raw capture validation;
- reference-component matrix;
- OPEN/SHORT/LOAD calibration evidence;
- range/frequency/amplitude/channel qualification;
- accuracy/repeatability/temperature observations;
- `NOMINAL` / `EXTENDED` / disabled map;
- regression test set;
- release versioning/artifacts;
- known limitations;
- Rev.2 evidence package.

## Task 1 — Freeze test-unit identity

Record:

```text
PCB revision
schematic revision
assembled BOM export
DNP list
rework/modifications
Blue Pill identification
firmware commit/version
calibration schema/model version
asset/settings schema versions
```

No qualification data is valid without knowing which physical configuration produced it.

## Task 2 — Complete staged bring-up

Follow `docs/09-Rev1-Bringup.md` and record pass/fail evidence for:

1. supply rails;
2. VMID/AFE idle state;
3. safe boot GPIOs;
4. UART/reset/watchdog;
5. TFT/W25Q/buttons/backlight/buzzer;
6. residual sensing/charger lockout;
7. K1/K2/range bank;
8. excitation waveform;
9. ADC/DMA raw capture;
10. first fixed known DUTs.

Any hardware rework made during this sequence must be added to the test-unit identity.

## Task 3 — Safety behavior matrix

Test at least:

- power-on reset;
- software reset;
- watchdog reset;
- charger present at boot;
- charger inserted while READY;
- charger insertion attempt around measurement transitions;
- residual voltage below/near/above provisional threshold;
- invalid/saturated residual ADC;
- range invalid/fault injection where practical;
- low/unstable supply conditions that can be safely tested.

Expected invariant: K1 does not remain in MEASURE after a safety-critical condition.

## Task 4 — Residual-voltage threshold qualification

Using controlled low-current sources and appropriate safety practice:

- compare ADC estimate to reference measurement;
- characterize offset/noise around zero;
- characterize polarity;
- determine threshold margin that is reliably distinguishable from noise/offset;
- verify fail-closed behavior when ADC saturates/is implausible;
- never treat the ±100 V observation envelope as the desired MEASURE threshold.

Freeze a conservative Rev.1 threshold only after data supports it.

## Task 5 — Excitation characterization

For each baseline frequency and allowed amplitude/range condition, record:

- actual frequency;
- RMS/amplitude;
- DC offset around VMID;
- visible residual PWM ripple;
- distortion/THD when measurable;
- settling time after frequency/amplitude/range change;
- load sensitivity;
- supply coupling.

Special attention: 10 Ω RREF current/headroom at reduced amplitude.

## Task 6 — ADC/acquisition characterization

Record:

- actual sample frequency;
- trigger jitter if measurable;
- ADC1/ADC2/rank skew;
- noise floor;
- raw-code headroom;
- channel-to-channel phase bias;
- repeatability;
- effect of TFT/backlight/buzzer/Flash activity to validate quiet mode.

Any deterministic phase bias becomes calibration/model input.

## Task 7 — Reference resistor matrix

For each RREF and frequency, measure standards approximately around:

```text
0.1 × RREF
1 × RREF
10 × RREF
```

subject to practical overlap and total project target ranges.

At each point record:

- raw complex result;
- corrected result;
- error;
- repeatability;
- selected 1X/HG channel;
- amplitude;
- confidence;
- autorange attempts.

Use more dense points near qualification boundaries.

## Task 8 — Capacitor matrix

Use stable reference capacitors where possible.

Cover values across the intended range and frequencies, paying particular attention to:

- small capacitances/high-Z leakage/parasitics;
- high capacitance at low frequency and settling time;
- ESR significance;
- fixture/cable contribution;
- OPEN subtraction behavior.

Do not overstate accuracy at extremes.

## Task 9 — Inductor matrix

Use characterized inductors and compare against a reference method/instrument where possible.

Observe:

- series resistance;
- frequency dependence;
- core losses;
- phase sign/convention;
- low-inductance fixture sensitivity;
- high-inductance behavior at low frequency.

Remember that real inductors are not ideal and may require reporting impedance plus equivalent-model values rather than treating L as frequency independent.

## Task 10 — OPEN/SHORT/LOAD repeatability

Repeat calibration fixtures multiple times, including reconnect cycles, to distinguish:

- electronic repeatability;
- connector/fixture repeatability;
- user-placement variation.

Reject a calibration workflow that appears numerically precise but is dominated by fixture instability.

## Task 11 — Temperature observations

At minimum record ambient/NTC temperature during qualification.

If practical, repeat representative points after instrument warm-up or modest temperature changes to estimate drift.

Do not implement aggressive temperature compensation unless data shows a repeatable relationship.

## Task 12 — Define qualification map

For each relevant condition, classify:

```text
NOMINAL
EXTENDED
DISABLED/UNQUALIFIED
```

Use measured criteria such as:

- magnitude error;
- phase error;
- repeatability;
- SNR;
- clipping/headroom;
- stability;
- calibration validity.

The initial 1–2% central-region goal is an objective, not an entitlement. If the data does not support it, classify honestly.

## Task 13 — Autorange qualification

Run DUTs spanning transitions and edge cases:

- near every RREF boundary;
- near 1X/HG transition;
- near amplitude transition;
- near OPEN/SHORT;
- noisy/unstable DUT contact;
- repeated same DUT to test hysteresis/history behavior.

Verify:

- no oscillating range loop;
- bounded attempt count;
- no unsafe amplitude/range pair;
- reasonable final condition;
- clear failure when no qualified condition exists.

## Task 14 — UI/quiet-mode regression

Repeat representative measurements while:

- changing screens before acquisition;
- backlight at different duties;
- asset-heavy screens active;
- logging enabled/disabled;
- buzzer patterns queued before acquisition.

Verify critical acquisition remains equivalent and quiet mode works.

## Task 15 — Power/battery behavior

Using the actual 1S charge/boost module, observe:

- battery ADC calibration;
- low-battery behavior;
- current consumption by operating state;
- TFT/backlight contribution;
- relay contribution;
- behavior near boost/low-battery limits;
- charger lockout behavior.

Freeze user-facing battery thresholds only after measuring the real power module.

## Task 16 — Regression dataset

Create a durable machine-readable qualification dataset containing:

```text
hardware id
firmware id
test id
DUT/reference value/reference source
frequency
RREF
amplitude
channel
raw result
corrected result
confidence
reference error
notes/temperature
```

Host tooling should be able to summarize and compare later firmware/hardware revisions.

## Task 17 — Release criteria

Rev.1 firmware release requires:

- all automated tests green;
- reproducible embedded build;
- documented Flash/RAM usage;
- safety matrix passed for tested conditions;
- qualification map generated;
- calibration persistence verified;
- no known issue that can leave K1 unsafe after a handled fault;
- user-facing claims aligned with measured capability;
- known limitations documented.

## Task 18 — Release artifacts

Archive:

- `.elf`;
- `.bin`;
- `.hex` where supported;
- map/size report;
- source commit/tag;
- hardware compatibility/revision;
- calibration schema/model version;
- asset/settings schema versions;
- qualification matrix/dataset;
- release notes.

## Task 19 — Rev.2 evidence package

After qualification, summarize measured limitations and rank them by impact.

Possible evidence-driven Rev.2 questions:

- Is high-Z leakage/OFF capacitance limiting useful range?
- Is K2 isolation needed?
- Would active guard measurably help?
- Does MCP6002 response limit 10 kHz HG performance?
- Is ADC resolution/noise the limiting factor?
- Is two-wire low-R error large enough to justify Kelvin?
- Is native USB worth a pinout/MCU revision?
- Is TVS protection worth its metrology cost?

Do not design Rev.2 before answering these with Rev.1 data where possible.

## Automated acceptance criteria

- all host tests pass;
- STM32 Release and Lab builds succeed;
- regression dataset passes schema validation;
- qualification-report tooling runs reproducibly;
- no `NOMINAL` classification exists without corresponding evidence records.

## Bench acceptance criteria

This entire phase is bench acceptance. Status remains `IN_PROGRESS` or `IMPLEMENTED_REQUIRES_BENCH_VALIDATION` until the qualification evidence is complete enough for the intended Rev.1 claims.

## Final handoff

Produce:

- Rev.1 capability summary;
- qualified ranges by quantity/frequency;
- accuracy/repeatability summary;
- safety validation summary;
- known limitations;
- release artifact list;
- unresolved risks;
- prioritized Rev.2 recommendations based on measured evidence.
