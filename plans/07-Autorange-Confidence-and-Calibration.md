# 07 — Autorange, Confidence, Classification, and Calibration

STATUS: IN_PROGRESS

Stage 1 — Automatic measurement engine / autorange / confidence / classification:
STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

Stage 2 — Calibration model / OPEN-SHORT-LOAD workflows / persistence:
STATUS: NOT_STARTED

## Goal

Turn the fixed-condition impedance engine into a robust instrument measurement engine that chooses appropriate ranges/channels/amplitudes/frequencies, evaluates result quality, automatically interprets the dominant component/model, applies calibration, and persists calibration/settings safely.

## Prerequisites

- Phase 04 safety/range services validated enough for controlled switching;
- Phase 05 acquisition stable;
- Phase 06 fixed-condition DSP produces meaningful raw complex impedance and single-condition model evidence;
- W25Q driver from Phase 03 available.

## In scope

- autorange search/state machine;
- 1X/HG selection;
- excitation amplitude policy;
- frequency-selection/refinement policy;
- partial-result publication between attempts;
- automatic R/C/L/mixed classification with explicit evidence/confidence;
- settling/retry rules;
- confidence metrics/classes;
- calibration record/set format;
- calibration-validity evaluation;
- OPEN/SHORT/LOAD workflow;
- complex calibration application;
- redundant/power-loss-tolerant persistence;
- qualification-map structure;
- calibration diagnostics/tools.

## Out of scope

- final polished UI artwork/navigation implementation;
- claiming qualification before bench evidence;
- future 4-wire/high-voltage modes;
- requiring normal users to preselect R/C/L before measurement.

## Stage 1 implementation boundary

Phase 07 Stage 1 adds a pure, host-testable automatic measurement session engine in
`Firmware/src/measurement/measurement_engine.c/.h`.

It consumes completed Phase 05 + Phase 06 attempt results and decides:

```text
next attempt
partial result
final result
terminal OPEN/SHORT/no-valid-condition outcome
```

It does not directly touch GPIO, relays, range pins, ADC/DMA registers, TFT, W25Q, or
Lab UART output during critical acquisition. Phase 05 remains the only owner of the
fixed-condition hardware measurement transaction. Phase 06 remains the owner of the
single-condition DSP and impedance equation.

Implemented Stage 1 policy constants:

```text
MEASUREMENT_AUTO_MAX_ATTEMPTS = 6
MEASUREMENT_AUTO_MAX_RANGE_TRANSITIONS = 4
MEASUREMENT_AUTO_MAX_FREQUENCY_REFINEMENTS = 1
MEASUREMENT_AUTO_MAX_REPEATED_CONDITIONS = 1

initial no-hint probe:
    RREF = 1 kOhm
    frequency = 1 kHz
    amplitude = 100 mVrms

ratio too small:
    |Z| / |ZREF| <= 0.20

ratio too large:
    |Z| / |ZREF| >= 5.00

OPEN-like upper edge:
    |Z| / |ZREF| >= 100 on 1 MOhm range

weak return signal:
    selected return peak < 2 mV

good return signal:
    selected return peak >= 10 mV

resistive dominance:
    |X| / |R| <= 0.10

reactive dominance:
    |X| / |R| >= 0.25
```

These thresholds are conservative software policy defaults and remain
`REQUIRES_BENCH_VALIDATION`.

Stage 1 preserves the Phase 05 amplitude safety rule: the automatic engine never emits
`10 Ohm + 500 mVrms`, and the lower excitation service still rejects it.

Click and Live modes both use the same session engine. Click runs one complete session.
Live may start another complete session using a previous final result as a starting hint,
but that hint only carries range/frequency/amplitude/channel preference. It carries no
safety authorization, no permit, and no active hardware ownership across sessions.

## Task 1 — Define measurement attempt model

Represent one attempt explicitly:

```text
RREF
frequency
amplitude
RET channel strategy
settling policy
acquisition metadata
calibration key
```

Represent attempt outcome separately:

```text
raw result
clipping
SNR
stability
headroom
single-condition model evidence
status
retry/rerange reason
```

This makes autorange deterministic and testable.

## Task 2 — Define autorange objectives

The algorithm should seek a condition where:

- DUT and RREF are in a useful ratio;
- ADC channels have adequate signal;
- no relevant channel clips;
- excitation current/headroom is safe;
- the combination is calibrated/qualified when possible;
- settling time remains acceptable;
- the selected condition provides useful information for the primary displayed value and, when needed, component/model classification.

Do not encode autorange as a single “nearest RREF to |Z|” rule.

## Task 3 — Initial probing/range strategy

Choose a deterministic first-attempt strategy.

Possible inputs:

- previous successful measurement history when still relevant;
- safe middle range when no history exists;
- low-energy probing strategy;
- known hard safety/qualification limits.

The normal product flow does **not** require a user-provided R/C/L hint.

The strategy must not expose the DUT/AFE to aggressive excitation before its impedance is reasonably known.

## Task 4 — Range transition state machine

Integrate hardware-service sequence:

```text
measurement safe
RANGE_EN=0
select address
wait dead-time
RANGE_EN=1
wait analog settling
configure excitation
recheck safety as required
enter measurement
acquire
return SAFE
process
```

No range GPIO shortcuts.

## Task 5 — 1X/HG selection

Define thresholds/logic for:

- RET_1X preferred when signal is large or HG clips;
- RET_HG preferred when signal is small and HG remains linear;
- overlap region used for cross-check/diagnostics;
- missing HG calibration causing fallback or confidence downgrade.

Host-test channel-selection boundaries.

## Task 6 — Amplitude policy

Baseline amplitudes:

```text
100 mVrms
500 mVrms
```

Rules:

- never 500 mVrms with RREF 10 Ω;
- use higher amplitude only when it improves SNR without clipping/current/headroom problems;
- calibration/qualification key includes amplitude;
- amplitude changes trigger settling.

## Task 7 — Frequency and refinement policy

Baseline frequencies:

```text
100 Hz
1 kHz
10 kHz
```

Define a deterministic primary/secondary measurement strategy rather than blindly measuring every DUT at every frequency.

The engine may request another frequency when it improves:

- observability;
- confidence;
- distinction between resistive/capacitive/inductive behavior;
- validity of a derived parameter;
- qualification coverage.

Cross-frequency behavior is particularly useful for automatic classification, but real component non-idealities mean it must be treated as evidence rather than an ideal-law assertion.

Every automatic frequency change must remain visible in session metadata so the UI can distinguish the **primary condition** used for the displayed value from additional conditions used for refinement/classification.

## Task 8 — Partial-result publication

Allow the measurement engine to publish a valid partial result after a completed attempt when useful.

A partial result may include:

```text
current primary value/model evidence
completed attempt condition
reason for continuing
next refinement purpose
```

Examples of continuation reasons:

```text
RERANGE
CHECK_ESR
CHECK_SECOND_FREQUENCY
IMPROVE_SNR
VERIFY_CLASSIFICATION
```

Requirements:

- only completed/validated attempt results are published;
- never expose individual raw ADC samples as a user result;
- publication occurs after the critical acquisition window;
- the UI may render progress only between acquisitions;
- partial values are clearly marked internally as non-final.

Host-test sequencing and cancellation/fault behavior.

## Task 9 — Confidence metrics

Create explicit metrics where feasible:

- clipping flags;
- AC amplitude;
- SNR/noise estimate;
- block-to-block variation;
- phase consistency;
- denominator conditioning in impedance equation;
- OPEN/SHORT proximity;
- calibration presence/validity;
- cross-frequency consistency where used;
- qualification-map status.

Avoid a single unexplained magic “quality score” as the only output.

## Task 10 — Confidence classes

Map metrics into:

```text
NOMINAL
EXTENDED
LOW_CONFIDENCE
REJECTED
```

The mapping must be testable and diagnostic reasons must be available.

`NOMINAL` requires qualification evidence, not merely mathematically clean samples.

## Task 11 — Automatic component/model classification

Implement the product classifier that consumes one or more completed impedance results plus quality metadata.

Normal output should support at least:

```text
RESISTOR / RESISTIVE
CAPACITOR / CAPACITIVE
INDUCTOR / INDUCTIVE
MIXED_OR_UNKNOWN
```

The exact user-facing labels belong to Phase 08 localization/UI, but the engine must expose a stable semantic enum plus evidence/confidence/reason fields.

Evidence may include:

- R/X dominance;
- reactance sign;
- phase;
- derived parameter validity;
- consistency across multiple frequencies;
- whether `|X|` changes with frequency in a manner consistent with the candidate model;
- ESR/series-resistance dominance;
- measurement confidence at each condition.

Rules:

- calculate impedance first; classification never chooses a different fundamental impedance equation;
- do not force R/L/C when evidence is ambiguous;
- low-confidence or mixed results remain reportable as complex impedance;
- a poor/non-ideal capacitor or inductor may still classify correctly using cross-frequency evidence when qualified;
- manual R/C/L selection is not required in normal Rev.1 operation.

Host tests must include ideal and non-ideal R/C/L plus mixed R+C/R+L cases and deliberately ambiguous vectors.

## Task 12 — Calibration record schema

Define a portable versioned record format including at least:

```text
magic
schema_version
hardware_revision
model_version
frequency
RREF
amplitude
RET channel
calibration temperature
sequence/timestamp surrogate
payload length
CRC32
payload
```

Do not persist raw compiler-dependent structs as the only format.

## Task 13 — Calibration-set validity model

Define what constitutes a **valid required calibration set for normal boot**.

Validation must cover at least:

- recognized record/type magic;
- schema/model compatibility;
- hardware revision compatibility;
- record bounds/length;
- CRC/integrity;
- required OPEN/SHORT/LOAD or model-specific record completeness;
- required condition/key coverage according to the current calibration strategy.

Expose a pure/testable result such as:

```text
VALID
MISSING
CORRUPT
INCOMPATIBLE_SCHEMA
INCOMPATIBLE_HARDWARE
INCOMPLETE
```

plus diagnostic reasons.

Phase 08 uses this result as the mandatory boot gate; Phase 07 owns the validity semantics.

## Task 14 — Calibration storage

Implement redundant slots or small journal behavior:

1. identify current valid record/set;
2. write new candidate to inactive location;
3. verify readback/CRC/completeness;
4. commit/sequence it as newest valid;
5. preserve older valid state until the new one is proven.

Power loss at any point must leave at least one recoverable valid state where practical.

This is particularly important for manually requested recalibration: starting a new calibration must not erase the current valid calibration first.

Calibration persistence uses the external W25Q storage layer. Do not call it STM32 internal EEPROM.

## Task 15 — OPEN workflow

OPEN characterization should capture residual admittance/leakage/parasitics for the selected key.

Requirements:

- enforce SAFE/fixture instructions through app/UI later;
- reject unstable captures;
- record raw diagnostic data in Lab mode;
- do not extrapolate OPEN calibration far outside the measured condition without model justification.

## Task 16 — SHORT workflow

SHORT characterizes residual series impedance/contact/switch/relay path.

Use a repeatable fixture and validate stability before accepting a record.

## Task 17 — LOAD workflow

LOAD uses a known standard within a useful region.

The user/tool must provide the reference value/model explicitly. Record enough metadata to reproduce the correction.

## Task 18 — Correction model

Start with the simplest complex model supported by evidence.

Potential progression:

1. complex scale/offset corrections;
2. OPEN/SHORT correction;
3. bilinear/Möbius model if real data shows clear benefit.

Do not implement a sophisticated model solely because it is theoretically available.

Model version is part of persistent compatibility.

## Task 19 — Qualification map

Create a representation of which combinations are:

```text
UNQUALIFIED
NOMINAL
EXTENDED
DISABLED
```

Key dimensions may include:

```text
hardware revision
frequency
RREF
amplitude
RET channel / strategy
```

The map may begin compiled for Lab builds and later become generated from qualification data.

## Task 20 — Autorange/classification termination

Define hard limits:

- maximum attempts;
- maximum classification/refinement frequency probes;
- no oscillation between two ranges;
- clear OPEN/SHORT exits;
- safety fault exits;
- timeout exits;
- explicit REJECTED result when no valid condition exists.

Do not keep acquiring indefinitely merely to force a component label. A valid `MIXED_OR_UNKNOWN`/low-confidence result is preferable to an infinite refinement loop or false certainty.

Host-test pathological decision loops.

## Task 21 — Diagnostics

Every measurement session should be able to report:

```text
attempt sequence
RREF/frequency/amplitude per attempt
primary condition
1X/HG decision
clipping/SNR/stability
calibration record used
partial-result publications
classification evidence/reason
confidence reason
retry/rerange/refinement reason
final result
```

## Stage 1 automated acceptance criteria

- automatic session engine has deterministic host tests;
- initial probe is conservative and deterministic;
- previous-result hinting cannot bypass safety ownership;
- forbidden `10 Ohm / 500 mVrms` is impossible through the public policy API;
- range movement is one-directional per decision and bounded by attempted-condition
  tracking;
- repeated-condition loops terminate explicitly;
- amplitude escalation is only used for weak-signal improvement and never on 10 Ohm;
- frequency refinement is bounded and visible in session metadata;
- partial-result publication only follows completed attempts;
- confidence mapping has explicit reason flags and does not produce `NOMINAL` without
  qualification evidence;
- session classification handles resistive, capacitive, inductive, non-ideal, and
  ambiguous vectors;
- OPEN-like, SHORT-like, no-valid-condition, safety-abort, and cancellation paths are
  terminal;
- Phase 06 math tests remain green.

## Later automated acceptance criteria

- autorange policy has deterministic host tests;
- no infinite retry/refinement loops;
- forbidden 10 Ω / 500 mVrms combination is impossible through the public API;
- confidence mapping has reason codes;
- automatic classification has ideal/non-ideal/mixed/ambiguous host tests;
- complex impedance remains available when R/C/L classification is ambiguous;
- calibration records survive corruption/power-loss simulation tests at the record-store level;
- incompatible hardware/schema/incomplete calibration sets are rejected;
- a new candidate calibration cannot invalidate the previous active valid set before commit;
- Phase 06 math tests remain green.

## Bench acceptance criteria

1. manually validate autorange across resistors spanning all RREF regions;
2. verify 1X/HG switching in overlap regions;
3. validate automatic simple R/C/L classification using known parts;
4. verify cross-frequency refinement improves or appropriately downgrades ambiguous/non-ideal cases;
5. run OPEN/SHORT/LOAD at baseline frequencies/ranges;
6. power-cycle and verify calibration persistence/validity evaluation;
7. interrupt a manual recalibration and verify previous valid calibration recovery where practical;
8. compare raw vs corrected results;
9. test repeated measurement stability;
10. verify confidence downgrades near range edges/clipping/OPEN/SHORT;
11. verify no range/amplitude transition violates safety policy.

`NOMINAL` qualification remains incomplete until Phase 09 performs the formal matrix.

## Handoff

Report:

- autorange algorithm/state diagram;
- attempt/refinement limits and fallback behavior;
- component/model classification algorithm, evidence, and ambiguity behavior;
- partial-result publication API;
- confidence metrics/thresholds and which remain provisional;
- calibration schema/model version;
- definition of a valid required calibration set;
- Flash layout usage;
- persistence recovery behavior;
- measured calibration improvement;
- qualification-map format;
- readiness for Phase 08.
