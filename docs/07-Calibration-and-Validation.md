# Calibration and Validation

## Purpose

The first PCB is not considered a qualified measurement instrument simply because it powers up. Rev.1 exists to measure and model:

- offset;
- gain;
- phase;
- switch RON;
- trace/relay/contact resistance;
- switch OFF capacitance;
- leakage;
- MCP6002 frequency response;
- PWM-related noise;
- thermal repeatability.

## Complex calibration

Each relevant calibration condition should be identified by a key similar to:

```text
hardware_revision
calibration_model_version
range
frequency
excitation_amplitude
```

`return_channel` is not part of the persistent Rev.1 condition key because Phase 05
captures RET_1X and RET_HG together for the same physical condition. A calibration
record may carry separate RET_1X/RET_HG correction terms and overlap evidence, but the
lookup key remains the shared hardware/model/range/frequency/amplitude condition.

Calibration temperature should also be recorded with an explicit validity flag. If no
valid NTC temperature is available for a capture, firmware must not substitute a
synthetic ambient value.

Stage 2B.2 treats this temperature as calibration provenance for the OSL acquisition.
It is not six independent thermal samples and it is not an input to runtime
temperature compensation.

## OPEN / SHORT / LOAD

### OPEN

Fixture with no DUT. Characterizes leakage and residual admittance/parasitics.
Evidence should be taken from synchronized raw phasors. A true OPEN may make the final
impedance equation singular, so the calibration workflow must preserve normalized
OPEN observables such as `(Vs - Vx) / Vx` instead of rejecting the capture only because
`Zx` cannot be computed.

### SHORT

Repeatable short at the fixture. Characterizes residual series impedance, contacts, traces, switches, and relay path.
SHORT stability is evaluated from residual series impedance observables on each usable
return path.

### LOAD

Known standard within the useful region of the selected range. Characterizes scale and phase tracking.
LOAD stability is evaluated from measured impedance relative to the known complex
standard. Both VEXC paths and both return paths must remain observable so later
coefficient solving can distinguish 1X, raw HG, reconstructed HG, and overlap behavior.

The implemented Rev.1 Stage 2B.2 calibration model uses the normalized transfer:

```text
t = Vx / Vs
K = ZL * (tL - tO) / (tL - tS)
Zcorr = K * (t - tS) / (t - tO)
```

where `tS`, `tO`, and `tL` are the measured SHORT, OPEN, and LOAD transfers for one
exact range/frequency/amplitude condition, and `ZL` is the known complex LOAD standard.
This maps SHORT to 0 ohm, LOAD to `ZL`, and OPEN to a singularity. Coefficients are
computed from stable OSL evidence and stored as a projective/Möbius condition record.

The solver and model are software-implemented and synthetically host-tested. Physical
accuracy, model residuals, leakage behavior, and temperature drift remain
`REQUIRES_BENCH_VALIDATION`.

The Stage 2B.2 host comparison deliberately injects complex gain/phase error, residual
series impedance, and shunt leakage/admittance, then validates impedances that were not
used as OPEN/SHORT/LOAD fit standards. In that deterministic model the previous
two-complex affine correction fitted at SHORT/LOAD leaves substantially larger
intermediate residuals, especially near high impedance and with complex reactance,
while the OSL/Mobius transfer maps the synthetic DUTs back to the true impedance within
the configured float tolerances. This justifies replacing the mathematical
`model_version`; it does not qualify the real PCB.

## Range validation

For each range and frequency:

1. measure standards near 0.1× RREF;
2. measure near 1× RREF;
3. measure near 10× RREF;
4. repeat at each allowed excitation amplitude;
5. measure repeatability and drift;
6. record magnitude and phase error;
7. document clipping/SNR/headroom boundaries.

## Qualification components

Recommended references include:

- precision resistors;
- C0G/NP0 capacitors for smaller values;
- film capacitors where appropriate;
- known inductors, ideally cross-checked with a reference instrument;
- repeatable OPEN/SHORT fixtures.

## Metrics

Record at least:

- absolute/relative error;
- standard deviation over repeated measurements;
- SNR;
- excitation THD when measurable;
- ADC headroom;
- temperature;
- residual voltage after SAFE/discharge;
- actual excitation frequency;
- actual sampling frequency;
- selected range/channel/amplitude.

## NOMINAL and EXTENDED

Firmware marks a combination `NOMINAL` only after enough measured evidence exists.

Initial engineering objective, not a guaranteed specification:

- central qualified region: roughly 1–2% class where achievable;
- extremes / `EXTENDED`: larger error may be acceptable if explicitly reported.

High-Z ranges, especially 1 MΩ, must not enter unrestricted automatic use before leakage and switch OFF capacitance are characterized.

## Regression policy

Changes to any of the following may invalidate calibration:

- op-amp;
- MOSFET;
- relay;
- RREF part/value;
- analog-path PCB layout;
- acquisition timing/sampling firmware;
- relevant filter values;
- calibration algorithm/model.

Persistent calibration records therefore carry `hardware_revision` and model/schema version information.
