# 07 — Autorange, Confidence, Classification, and Calibration

STATUS: IN_PROGRESS

Stage 1A — Automatic measurement policy / autorange / confidence / classification:
STATUS: IMPLEMENTED

Stage 1B — End-to-end automatic session orchestration:
STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

Stage 2A — Initial calibration data model / correction architecture / persistence substrate:
STATUS: IMPLEMENTED

Stage 2A.1 — Calibration substrate hardening / async W25Q integration / correction plumbing:
STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

Stage 2A.2 — Condition-domain / compatibility / record-integrity hardening:
STATUS: IMPLEMENTED

Stage 2A.3 — SRAM/stack hardening / calibration runtime ownership:
STATUS: IMPLEMENTED

Stage 2B — OPEN-SHORT-LOAD workflows / calibration acquisition:
STATUS: IN_PROGRESS

Stage 2B.1 — Product calibration service / OSL evidence acquisition:
STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

Stage 2B.2 — OSL coefficient solving / calibration-set replacement:
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

## Stage 1A/1B implementation boundary

Phase 07 Stage 1A adds a pure, host-testable automatic measurement policy engine in
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

Phase 07 Stage 1B adds the application orchestration layer in
`Firmware/src/app/app_measurement_session.c/.h`. The controller connects:

```text
Phase 07 policy requested attempt
    -> Phase 05 fixed-condition transaction
    -> Phase 06 DSP after SAFE teardown
    -> Phase 07 policy submit
    -> partial / next attempt / final result
```

The application controller owns session lifecycle, cancellation, partial/final result
publication, and Lab diagnostic events. It does not directly energize K1, switch range
GPIO, issue bypass permits, configure ADC/DMA, or keep hardware active between attempts.
Every attempt is a fresh Phase 05 safety transaction.

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

The session result has explicit primary-result semantics:

```text
primary_attempt_index:
    zero-based index into completed attempt history, or MEASUREMENT_AUTO_INDEX_NONE

primary_attempt:
    the policy-designated attempt that supports the displayed value

supporting/refinement attempts:
    retained in history and classification evidence but do not silently replace
    the primary value
```

The primary attempt is selected by explicit quality ordering, not by impedance
magnitude. Frequency-refinement attempts provide supporting classification/confidence
evidence unless a future policy explicitly promotes a new primary condition.

Stage 1B Lab diagnostics expose `lab auto measure`, which runs a Click-style automatic
session and emits structured post-critical-window events:

```text
AUTO_BEGIN
ATTEMPT_BEGIN
PARTIAL_RESULT
FINAL_RESULT
AUTO_END
```

Click and Live modes both use the same session engine. Click runs one complete session.
Live may start another complete session using a previous final result as a starting hint,
but that hint only carries range/frequency/amplitude/channel preference. It carries no
safety authorization, no permit, and no active hardware ownership across sessions.

Current Rev.1 qualification behavior is intentionally conservative:

```text
measurement quality:
    GOOD / DEGRADED / INVALID

qualification:
    UNQUALIFIED / NOMINAL / EXTENDED / DISABLED

publication confidence:
    NOMINAL / EXTENDED / LOW_CONFIDENCE / REJECTED
```

A mathematically clean but physically unqualified Rev.1 result remains
`qualification=UNQUALIFIED` and `publication confidence=LOW_CONFIDENCE`. `NOMINAL`
requires explicit qualification evidence from later Phase 09 work.

RET channel ownership is resolved as follows: Phase 05 captures both RET paths and
Phase 06 selects the mathematically usable channel for a fixed condition. Phase 07
records RET_1X/RET_HG usability, selected channel, and overlap evidence for confidence
and future calibration/qualification, but does not command a different hardware RET
capture path.

Multi-frequency classification records explicit evidence flags for reactance dominance,
resistive dominance, capacitive/inductive qualitative frequency trend support, and
trend inconsistency. Trend evidence is tolerant and qualitative; it is not an ideal
component-law fit, and ambiguous/non-ideal networks may remain `MIXED_OR_UNKNOWN`.

## Stage 2A / 2A.1 implementation boundary

Phase 07 Stage 2A implements the calibration substrate without starting any
OPEN/SHORT/LOAD user workflow.

Implemented files:

```text
Firmware/src/measurement/measurement_calibration.c/.h
Firmware/src/measurement/measurement_calibration_store.c/.h
Firmware/src/storage/storage_crc32.c/.h
Firmware/src/storage/storage_layout.c/.h
Firmware/tools/inspect_calibration_record.py
```

The active persistent schema is:

```text
MEASUREMENT_CAL_SCHEMA_VERSION = 2
MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V2 = 2
MEASUREMENT_CAL_HARDWARE_REV1 = 0x00010001
```

`schema_version` describes the portable byte framing. `model_version` describes the
mathematical correction model. They are intentionally separate compatibility axes.

The Stage 2A.1 correction model is the conservative direct model:

```text
global per-channel ADC scale/offset:
    volts = raw * code_to_volts + offset_volts

complex high-gain transfer:
    RET = VMID + (RET_HG - VMID) / H_HG

complex ZREF:
    ZREF(frequency, range, amplitude)

optional complex output correction per selected return path:
    Z_corrected = Z_raw * output_scale[RET_1X or RET_HG] + output_offset[RET_1X or RET_HG]
```

This does not implement OPEN/SHORT/LOAD correction capture. The record types reserve
values for future OPEN, SHORT, and LOAD evidence so Stage 2B can add workflows without
changing the frame contract gratuitously.

Calibration keys intentionally include only the physical pre-DSP condition:

```text
hardware_revision
model_version
range_id
frequency
amplitude
```

RET channel and RET strategy are not key dimensions. Phase 05 captures both return
paths and Phase 06 selects the usable channel after DSP; therefore the calibration
record carries both RET_1X and RET_HG output-correction terms together. This removes
the circular dependency where persistent lookup previously needed a RET choice before
the DSP made one.

Resolution is exact-condition only in Stage 2A.1. Missing exact calibration may fall
back to the ideal DSP defaults only when the caller explicitly allows that behavior;
the provenance is then reported as `source=IDEAL`, `status=MISSING`, and
`uncalibrated=true`. This is acceptable for Lab/debug bring-up but is not a product
qualification claim.

Resolution statuses are explicit:

```text
FOUND
MISSING
INCOMPATIBLE
CORRUPT
UNQUALIFIED
INVALID_ARG
```

Calibration-set validity is also explicit, not a boolean:

```text
VALID
MISSING
CORRUPT
INCOMPATIBLE_SCHEMA
INCOMPATIBLE_HARDWARE
INCOMPATIBLE_MODEL
INCOMPLETE
```

Validity includes diagnostic flags for missing/corrupt/schema/hardware/model/incomplete
and unqualified records. Required coverage is centralized through a bounded
`measurement_cal_requirements_t` key list so later product boot policy can define the
exact required condition set without rewriting the record validator.

Portable frame contract:

```text
little-endian fields only
header size = 64 bytes
max frame size = 3072 bytes
payload = set header with global ADC correction + fixed-size condition records
CRC32 = IEEE reflected CRC32 over header fields excluding CRC/commit plus payload
commit marker = 0x54494D43, programmed last
temperature = signed integer millidegrees C
floating coefficients = IEEE754 binary32, serialized field-by-field
```

Raw compiler-dependent structs are not persisted.

W25Q logical layout now reserves the mutable tail of any supported W25Q part:

```text
resource pack:     0 .. mutable_tail_start - 1
calibration slot A: 1 sector / 4096 bytes
calibration slot B: 1 sector / 4096 bytes
settings:          1 sector / 4096 bytes
diagnostics:       4 sectors / 16384 bytes
bring-up test:     final sector / 4096 bytes
```

The final sector remains reserved for the existing W25Q bring-up self-test. Resource
assets stay separate from calibration/settings/diagnostics.

The Stage 2A store is a two-slot power-loss-safe substrate:

1. read both slots;
2. choose the newest valid committed sequence, with rollover-aware comparison;
3. serialize a candidate into the inactive slot;
4. erase only the inactive slot;
5. program header and payload with the commit marker still erased;
6. program the commit marker last;
7. verify by decoding/CRC.

If power is lost before commit, the previous valid slot remains active. If power is lost
after commit, the newly committed CRC-valid sequence may become active. The host NOR
emulator enforces erase-to-`0xFF` and 1->0-only programming.

The store API is asynchronous. `measurement_cal_store_step(store, now_ms)` starts one
W25Q erase/program operation, waits through the driver `poll()` contract, and never
starts a second mutation while the previous one is active. The real W25Q adapter maps
the portable store IO onto `w25q_device_read()`,
`w25q_device_sector_erase_start()`, `w25q_device_page_program_start()`, and
`w25q_device_poll()`; no duplicate SPI path is introduced.

The newest CRC-valid slot is not automatically selected for product use. Active
selection requires a usable compatible complete set. Diagnostics still expose newer
rejected slots as missing/corrupt/incompatible/incomplete.

The Phase 05/06 processing integration applies the resolved output correction to the
raw complex impedance and recomputes derived quantities. Result metadata preserves
calibration provenance (`IDEAL`, `PERSISTED`, missing/unqualified/found status,
sequence, model, and condition ID) for later UI/qualification policy.

Lab diagnostics expose `lab cal status` for active/slot validity and `lab cal dump` for
a read-only compact listing of the active calibration records.

Stage 2A.1 does not yet add the Phase 08 mandatory calibration boot gate. Missing
calibration remains diagnosable and explicit, and product-ready measurement blocking
will be wired when the UI/boot calibration flow is implemented.

## Stage 2A.2 implementation boundary

Stage 2A.2 unifies the Rev.1 condition domain before real calibration data is
generated. Firmware now distinguishes three concepts:

```text
hardware-supported condition:
    the Rev.1 firmware/electronics transport can execute it

calibratable condition:
    the current calibration model can represent coefficients for it

qualified/product-enabled condition:
    bench evidence permits ordinary product use
```

For Rev.1 Stage 2A.2, the hardware-supported and calibratable domains are identical.
There is no qualification map yet, so every condition remains unqualified until later
bench evidence says otherwise.

The authoritative physical support rule is centralized in
`measurement_condition.c/.h`. The only hard Stage 2A.2 Rev.1 prohibition is:

```text
RREF = 10 Ohm
amplitude = 500 mVrms
```

All three supported frequencies remain representable for the 100 kOhm and 1 Mohm
ranges. They may later become `UNQUALIFIED`, `EXTENDED`, or `DISABLED` based on Phase
09 evidence, but they are not silently removed from calibration capability merely
because high-Z/high-frequency behavior is expected to be difficult.

The resulting Rev.1 calibration matrix is:

```text
6 ranges x 3 frequencies x 2 amplitudes = 36 nominal combinations
minus 3 combinations of 10 Ohm + 500 mVrms
= 33 hardware-supported/calibratable conditions
```

`MEASUREMENT_CAL_MAX_RECORDS`, `MEASUREMENT_CAL_MAX_REQUIRED_KEYS`, and the full-matrix
test are sized from that product condition domain, not the reverse. With 33 fixed-size
condition records, the serialized frame remains below the 3072-byte frame limit and
inside one 4096-byte W25Q calibration slot.

Calibration set construction has explicit identity semantics:

```text
ADD      succeeds only for a new complete key
ADD      fails if the key already exists
REPLACE  succeeds only for an existing key
REPLACE  fails if the key is missing
```

Decoded or manually assembled sets containing duplicate complete keys are invalid. The
diagnostic `condition_id` is a CRC32 of the complete key and is not authoritative for
lookup or equality; all matching uses the full key.

Slot diagnostics now separate frame integrity from compatibility:

```text
MISSING
CORRUPT
INCOMPATIBLE_SCHEMA
INCOMPATIBLE_HARDWARE
INCOMPATIBLE_MODEL
INCOMPLETE
VALID
```

A structurally intact slot with an old schema, wrong hardware revision, or wrong model
version preserves header metadata such as sequence, schema, hardware revision, and model
version for `lab cal status` instead of being reported as generic corruption. Active
selection still chooses the newest compatible, complete, usable set and may fall back to
an older valid slot.

Post-write verification decodes the target slot, verifies sequence/hardware/model/count,
and checks the expected complete condition keys. A CRC-valid but semantically different
candidate is not accepted as a successful write.

Stage 2B should treat raw OPEN/SHORT/LOAD captures as acquisition evidence used to
derive compact condition coefficients. The active production calibration set should
remain coefficient-focused and small in SRAM. If raw evidence is needed for audit or
debug, it should be persisted separately from the active coefficient set rather than
inflating `measurement_cal_set_t`.

## Stage 2A.3 implementation boundary

Stage 2A.3 hardens the calibration substrate for the STM32F103C8T6 SRAM and stack
budget before OPEN/SHORT/LOAD workflow work begins. It does not implement calibration
capture workflows, calibration wizards, or persistent raw evidence.

Calibration store memory ownership is:

```text
measurement_cal_store_t
    owns one serialized frame image while an async write is active
    owns one decoded scan scratch set for slot scanning and verification
    stores expected post-write identity as sequence/hardware/model/count/key-mask

app_calibration_service_t
    owns the product calibration runtime
    owns the single calibration store scratch context
    owns the active OPEN/SHORT/LOAD acquisition workflow
    is available in Debug, Release, and Lab builds

app_calibration_runtime_t
    owns the active decoded calibration set and active-slot provenance
    owns compact slot diagnostics
    does not own W25Q or raw metrology buffers

app_lab_console_t
    owns Lab command state only
    attaches to app_calibration_service_t through its public API
    does not own the active calibration set
    does not own calibration store scratch storage
```

The store no longer places multiple complete `measurement_cal_set_t` objects on the
normal embedded write path. `measurement_cal_store_write_start()` serializes the caller's
const candidate with an explicit current schema/model/sequence header, decodes into the
store scan scratch for preflight verification, then retains only the serialized image and
compact expected identity for the asynchronous erase/program/verify state machine.
`measurement_cal_store_load_newest_usable()` now copies a decoded candidate directly into
the caller output when it becomes the newest valid choice instead of keeping a second
full `best` set on the stack.

Expected-key post-write verification is compact but remains semantic. It derives a
bit-mask from the complete Rev.1 calibration key fields:

```text
hardware_revision
model_version
range
frequency
amplitude
```

The diagnostic `condition_id` remains non-authoritative and is not used as the write
verification identity. Sequence, hardware revision, model version, record count, CRC,
commit marker, duplicate-key rejection, and exact key-mask agreement must all pass.

Current SRAM/stack policy:

```text
guaranteed SRAM:             20480 bytes
minimum stack headroom:       2048 bytes
maximum calibration frame:    3072 bytes
W25Q calibration slot:        4096 bytes
metrology raw DMA buffer:     BSP-owned; never copied by calibration runtime
```

STM32 builds enable GCC `-fstack-usage` so relevant stack frames can be audited from
generated `.su` files. A compile-time guard keeps `measurement_cal_store_t` within its
documented scratch budget. Lab-only diagnostics may retain extra command/dump state, but
Release must not carry Lab-only console buffers or raw calibration workflow fixtures.

Stage 2B must keep raw OPEN/SHORT/LOAD captures as transient acquisition evidence. It
must reuse the existing Phase 05 raw block ownership, derive compact coefficients, and
avoid copying complete raw ADC blocks into calibration/session contexts.

## Stage 2B.1 implementation boundary

Stage 2B.1 adds a product-owned calibration service and the first OPEN/SHORT/LOAD
evidence acquisition workflow. It does not solve or commit new calibration
coefficients.

On boot, after W25Q probing, the application initializes the product calibration
service. If W25Q is present, the service initializes the calibration store, scans both
slots, selects the newest usable persisted set, and publishes cached runtime validity.
If W25Q is absent or rejected, the service records `STORAGE_UNAVAILABLE`. Lab status and
dump commands read this cached state; they do not rescan or reinitialize storage. A
manual Lab rescan is rejected while a store transaction or OSL workflow is active.

The OSL workflow captures one exact condition at a time:

```text
Lab/UI intent
    -> app_calibration_service_t
    -> app_calibration_workflow_t
    -> Phase 05 fixed-condition DUT measurement
    -> Phase 06 baseline DSP evidence extraction after SAFE teardown
    -> compact per-condition OPEN/SHORT/LOAD evidence
```

Each capture is a separate Phase 05 safety transaction. The workflow never energizes
K1 directly, never keeps K1 energized between repeats, never bypasses measurement
permits, and never owns range GPIO.

Initial Stage 2B.1 repetition policy:

```text
accepted captures required: 6
maximum total attempts:     10
stability limit:            20000 ppm of complex-Z magnitude
minimum source magnitude:   5000 uV peak
minimum denominator:        1000 uV peak for SHORT/LOAD evidence
```

These acquisition thresholds are provisional and remain `REQUIRES_BENCH_VALIDATION`.
The workflow uses online complex statistics and stores only compact evidence. It does
not copy the 3072-byte Phase 05 raw DMA buffer.

Stage 2B.1 standard semantics:

- `OPEN` preserves phasor evidence and residual apparent impedance/noise when meaningful;
- `SHORT` preserves compact complex residual impedance evidence;
- `LOAD` records a known complex standard impedance, with the initial Lab command
  accepting pure resistance in ohms;
- both RET_1X and RET_HG paths remain visible in evidence.

Rejected captures are explicit. Causes include Phase 05 failure, safety abort,
clipping, invalid/non-finite DSP evidence, source too small, no usable RET channel, and
severe denominator conditioning where applicable. Cancellation during an active Phase 05
transaction requests the existing safe abort path, waits for the hardware-safe end
state, then discards incomplete evidence. Active persisted calibration remains
unchanged throughout Stage 2B.1.

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
