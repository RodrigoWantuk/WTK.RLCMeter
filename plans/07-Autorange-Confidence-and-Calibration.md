# 07 — Autorange, Confidence, and Calibration

STATUS: NOT_STARTED

## Goal

Turn the fixed-condition impedance engine into a robust instrument measurement engine that chooses appropriate ranges/channels/amplitudes, evaluates result quality, applies calibration, and persists calibration/settings safely.

## Prerequisites

- Phase 04 safety/range services validated enough for controlled switching;
- Phase 05 acquisition stable;
- Phase 06 fixed-condition DSP produces meaningful raw complex impedance;
- W25Q driver from Phase 03 available.

## In scope

- autorange search/state machine;
- 1X/HG selection;
- excitation amplitude policy;
- optional frequency-selection policy;
- settling/retry rules;
- confidence metrics/classes;
- calibration record format;
- OPEN/SHORT/LOAD workflow;
- complex calibration application;
- redundant/power-loss-tolerant persistence;
- qualification-map structure;
- calibration diagnostics/tools.

## Out of scope

- final polished UI artwork;
- claiming qualification before bench evidence;
- future 4-wire/high-voltage modes.

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
- settling time remains acceptable.

Do not encode autorange as a single “nearest RREF to |Z|” rule.

## Task 3 — Initial range strategy

Choose a deterministic first-attempt strategy.

Possible inputs:

- previous successful measurement;
- user manual hint;
- safe middle range when no history exists;
- low-energy probing strategy.

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

## Task 7 — Frequency policy

Initial product may default to a user-selected or standard frequency. Automatic frequency changes should occur only under explicit policy, for example when the DUT is poorly observable at the current frequency.

Any automatic cross-frequency classification must preserve transparency in the result metadata/UI.

## Task 8 — Confidence metrics

Create explicit metrics where feasible:

- clipping flags;
- AC amplitude;
- SNR/noise estimate;
- block-to-block variation;
- phase consistency;
- denominator conditioning in impedance equation;
- OPEN/SHORT proximity;
- calibration presence/validity;
- qualification-map status.

Avoid a single unexplained magic “quality score” as the only output.

## Task 9 — Confidence classes

Map metrics into:

```text
NOMINAL
EXTENDED
LOW_CONFIDENCE
REJECTED
```

The mapping must be testable and diagnostic reasons must be available.

`NOMINAL` requires qualification evidence, not merely mathematically clean samples.

## Task 10 — Calibration record schema

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

## Task 11 — Calibration storage

Implement redundant slots or small journal behavior:

1. identify current valid record;
2. write new candidate to inactive location;
3. verify readback/CRC;
4. commit/sequence it as newest valid;
5. preserve older valid record until new one is proven.

Power loss at any point must leave at least one recoverable valid state where practical.

## Task 12 — OPEN workflow

OPEN characterization should capture residual admittance/leakage/parasitics for the selected key.

Requirements:

- enforce SAFE/fixture instructions through app/UI later;
- reject unstable captures;
- record raw diagnostic data in Lab mode;
- do not extrapolate OPEN calibration far outside the measured condition without model justification.

## Task 13 — SHORT workflow

SHORT characterizes residual series impedance/contact/switch/relay path.

Use a repeatable fixture and validate stability before accepting a record.

## Task 14 — LOAD workflow

LOAD uses a known standard within a useful region.

The user/tool must provide the reference value/model explicitly. Record enough metadata to reproduce the correction.

## Task 15 — Correction model

Start with the simplest complex model supported by evidence.

Potential progression:

1. complex scale/offset corrections;
2. OPEN/SHORT correction;
3. bilinear/Möbius model if real data shows clear benefit.

Do not implement a sophisticated model solely because it is theoretically available.

Model version is part of persistent compatibility.

## Task 16 — Qualification map

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

## Task 17 — Autorange termination

Define hard limits:

- maximum attempts;
- no oscillation between two ranges;
- clear OPEN/SHORT exits;
- safety fault exits;
- timeout exits;
- explicit REJECTED result when no valid condition exists.

Host-test pathological decision loops.

## Task 18 — Diagnostics

Every measurement session should be able to report:

```text
attempt sequence
RREF/frequency/amplitude per attempt
1X/HG decision
clipping/SNR/stability
calibration record used
confidence reason
retry/rerange reason
final result
```

## Automated acceptance criteria

- autorange policy has deterministic host tests;
- no infinite retry loops;
- forbidden 10 Ω / 500 mVrms combination is impossible through the public API;
- confidence mapping has reason codes;
- calibration records survive corruption/power-loss simulation tests at the record-store level;
- incompatible hardware/schema records are rejected;
- Phase 06 math tests remain green.

## Bench acceptance criteria

1. manually validate autorange across resistors spanning all RREF regions;
2. verify 1X/HG switching in overlap regions;
3. run OPEN/SHORT/LOAD at baseline frequencies/ranges;
4. power-cycle and verify calibration persistence;
5. compare raw vs corrected results;
6. test repeated measurement stability;
7. verify confidence downgrades near range edges/clipping/OPEN/SHORT;
8. verify no range/amplitude transition violates safety policy.

`NOMINAL` qualification remains incomplete until Phase 09 performs the formal matrix.

## Handoff

Report:

- autorange algorithm/state diagram;
- attempt limits and fallback behavior;
- confidence metrics/thresholds and which remain provisional;
- calibration schema/model version;
- Flash layout usage;
- persistence recovery behavior;
- measured calibration improvement;
- qualification-map format;
- readiness for Phase 08.
