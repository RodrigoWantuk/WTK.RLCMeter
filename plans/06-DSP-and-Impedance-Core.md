# 06 — DSP and Impedance Core

STATUS: NOT_STARTED

## Goal

Implement the host-testable metrology core that converts synchronized ADC blocks into calibrated complex channel phasors and then into complex DUT impedance and derived electrical quantities.

This phase should be mathematically trustworthy before autorange, cross-frequency classification confidence, and calibration workflow complexity are added.

## Prerequisites

- Phase 05 acquisition metadata/buffer format stable;
- actual ADC timing/skew documented;
- raw captures available for later comparison;
- host-test harness available from Phase 01.

## In scope

- complex-number primitives as needed;
- synchronous I/Q or single-bin DFT;
- DC offset/reference handling;
- channel scaling;
- RET_1X and RET_HG reconstruction;
- central impedance equation;
- R/X/|Z|/phase;
- series-equivalent C/L;
- ESR/Q/D where valid;
- single-condition model interpretation primitives;
- numerical guardrails;
- synthetic known-vector tests;
- diagnostic measurement-result structure.

## Out of scope

- automatic range search;
- final multi-frequency component classification/confidence policy;
- final OPEN/SHORT/LOAD calibration workflow;
- final qualification map;
- polished product UI.

## Task 1 — Freeze pure measurement types

Create narrow C types for:

```text
complex value
phasor set
measurement configuration metadata
measurement result
quality/statistics
error/status codes
```

Use UI-independent units and representations. Prefer SI base units internally.

Do not encode strings or screen-formatting fields into metrology structures.

## Task 2 — Complex arithmetic

Use either:

- carefully implemented project complex helpers; or
- standard C complex support if toolchain/host compatibility is proven and the API remains clear.

Required operations include:

- add/subtract;
- multiply/divide;
- magnitude;
- phase;
- finite/near-zero checks.

Host-test edge cases thoroughly.

## Task 3 — Synchronous phasor extraction

Implement the chosen single-frequency extraction method.

Baseline concept:

```text
I = Σ x[n] cos(ωn)
Q = Σ x[n] sin(ωn)
```

Requirements:

- account for sample count and scaling;
- handle integer-cycle windows cleanly;
- document sine/cosine sign/phase convention;
- no hidden dependence on UI or HAL;
- support deterministic channel phase corrections later.

Compare against analytically generated sine waves with known amplitude and phase.

## Task 4 — DC/VMID handling

Clarify whether phasor extraction naturally rejects DC and how VMID is represented.

Because the physical channels are biased around VMID, the implementation must correctly derive complex quantities referenced to VMID without assuming raw ADC codes are centered at zero.

Test with synthetic offsets far larger than the AC amplitude.

## Task 5 — ADC scaling

Convert raw codes to voltage using explicit scale/offset metadata.

Avoid hard-coding 3.3 V as an exact ADC reference if the architecture later allows calibrated ADC scale.

Keep channel gain/phase correction separate from raw ADC code conversion.

## Task 6 — RET_HG reconstruction

Implement the high-gain channel through a complex transfer representation.

Conceptually:

```text
RET = VMID + (RET_HG - VMID) / H_HG
```

Initial host tests may use ideal `H_HG = 15.47 + j0`, but the interface must accept calibrated complex response.

Do not spread the nominal 15.47 constant across the codebase.

## Task 7 — Channel selection primitive

Provide a pure decision helper that can evaluate whether RET_1X or RET_HG data is usable based on supplied quality metadata such as:

- clipping;
- amplitude;
- SNR estimate;
- calibration availability.

Final autorange policy belongs to Phase 07, but Phase 06 should make both channels mathematically usable.

## Task 8 — Impedance equation

Implement:

```text
Vs = VEXC - VMID
Vx = RET  - VMID
Zx = ZREF * Vx / (Vs - Vx)
```

Guard against:

- denominator near zero;
- non-finite values;
- extreme OPEN-like behavior;
- extreme SHORT-like behavior;
- invalid/missing phasors;
- invalid ZREF calibration.

Return explicit status rather than NaN propagation as the only signal.

## Task 9 — Derived quantities

From:

```text
Z = R + jX
```

calculate:

- resistance `R`;
- reactance `X`;
- magnitude `|Z|`;
- phase;
- series-equivalent inductance when `X > 0` and confidence/model permits;
- series-equivalent capacitance when `X < 0` and confidence/model permits;
- ESR/Q/D where definitions and sign conventions are valid.

Near-zero X must not produce absurd infinite L/C values without an explicit invalid/not-applicable result.

## Task 10 — Single-condition model interpretation

Provide a pure, UI-independent interpretation primitive for one completed measurement condition.

The normal product does not ask the user to select resistor, capacitor, or inductor before measurement. Therefore the result model must permit automatic interpretation after `Z` is known.

At this phase the primitive may classify tendencies such as:

```text
RESISTIVE
CAPACITIVE
INDUCTIVE
MIXED_OR_UNKNOWN
```

and expose reasons/metrics rather than only a label.

Possible evidence includes:

- reactance sign;
- `|X| / |R|` dominance;
- phase magnitude/sign;
- whether derived C/L values are numerically meaningful;
- quality/status of the source measurement.

This must **not** alter the impedance equation or cause different DSP math depending on a preselected component type.

Do not force a component label for ambiguous/mixed networks. Cross-frequency consistency and final confidence policy belong to Phase 07.

## Task 11 — Numerical precision review

Evaluate `float` performance/precision on Cortex-M3 without FPU versus possible selective `double` use.

Do not assume `double` is free: STM32F103 performs floating point in software.

Use host tests and error budgets to decide. The likely baseline is `float` for embedded DSP with carefully designed scaling, but record the decision and evidence.

## Task 12 — Synthetic vector suite

Create deterministic host vectors covering:

### Pure resistor

Multiple values around RREF ratios of 0.1×, 1×, 10×.

### Pure capacitor

Multiple C values at 100 Hz, 1 kHz, 10 kHz.

### Pure inductor

Multiple L values at the three frequencies.

### Mixed impedance

Series R+C and R+L cases with analytically known complex Z.

Verify that model interpretation is allowed to report mixed/unknown where a single R/L/C dominant label is not justified.

### Error cases

- DC offset;
- added white noise;
- phase offset;
- gain error;
- clipping;
- denominator approaching zero;
- very small return signal;
- synthetic ADC channel skew.

## Task 13 — Reference implementation comparison

Create a small host-side reference calculator/tool, potentially Python in `Firmware/tools`, that generates expected phasors/impedance independently of the embedded implementation.

Use it to generate fixtures or validate C outputs. Keep the production firmware independent of Python.

## Task 14 — Real raw-capture replay

If Phase 05 bench captures exist, add a host tool/test mode that replays captured blocks through the same DSP core.

This is valuable before putting DSP execution on the MCU because PC-side inspection can distinguish math errors from acquisition errors.

## Task 15 — Embedded integration

Integrate DSP after DMA acquisition without moving heavy work into ISR.

Sequence:

```text
DMA block ready
K1 -> SAFE when acquisition complete
measurement processing scheduled
phasors calculated
impedance calculated
derived values calculated
single-condition interpretation produced
result stored in app/measurement context
diagnostics/UI notified
```

The result API should be suitable for later Phase 07 publication of validated partial results between attempts. No TFT rendering belongs in this phase.

## Automated acceptance criteria

- all pure measurement modules compile on host and STM32;
- synthetic ideal vectors meet tight mathematical tolerances defined by tests;
- noise/error vectors behave predictably;
- singular/invalid cases return explicit statuses;
- single-condition R/C/L tendency interpretation is host-tested and never required as an input to impedance computation;
- ambiguous mixed impedances can remain unclassified rather than being forced to R/L/C;
- no HAL/TFT/W25Q dependency appears in pure DSP modules;
- host reference and C implementation agree within documented tolerances.

## Bench acceptance criteria

Using fixed known DUTs and manually selected RREF:

1. compare embedded/raw-replayed results for known resistors;
2. verify phase sign convention using known C and L;
3. compare RET_1X and RET_HG reconstructed values in overlap regions;
4. identify systematic gain/phase error before calibration;
5. verify results remain stable across repeated acquisitions;
6. confirm basic single-condition model tendency agrees with known simple R/C/L DUTs where physically unambiguous.

Accuracy at this phase is **raw/unqualified** and should not be represented as final product accuracy.

## Handoff

Report:

- phasor sign/scaling convention;
- numeric type decision;
- synthetic test tolerance/results;
- result/status API;
- single-condition interpretation API/reason fields;
- observed raw hardware systematic errors;
- known limits near OPEN/SHORT;
- RET_HG reconstruction behavior;
- readiness for Phase 07.
