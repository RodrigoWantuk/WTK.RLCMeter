# Measurement Model and DSP

## Electrical model

During measurement:

```text
VEXC -- ZREF -- RET -- ZDUT -- VMID
```

Complex voltages are referenced to VMID:

```text
Vs = VEXC - VMID
Vx = RET  - VMID
I  = (Vs - Vx) / ZREF
Zx = ZREF * Vx / (Vs - Vx)
```

This is the central impedance equation of the instrument.

After OPEN/SHORT/LOAD calibration the runtime calibration model operates on the
normalized transfer:

```text
t = Vx / Vs
Z = K * (t - t_short) / (t - t_open)
```

The Rev.1 OSL/Mobius model stores `t_short`, `t_open`, and `K` per exact
range/frequency/amplitude condition. This form is intentionally projective: the SHORT
standard maps to 0 ohm, the LOAD standard maps to the known complex load, and the OPEN
standard maps to a singularity. Runtime processing must therefore treat `t` values near
`t_open` as OPEN-like rather than allowing NaN/Inf propagation.

## Complex processing

Amplitude alone is not sufficient to separate resistance from reactance. Each measured channel is converted into a complex phasor through synchronous detection or an equivalent single-bin DFT:

```text
V = I_component + j * Q_component
```

A full FFT is not required for the normal measurement path.

## Baseline frequencies

Initial qualification targets:

- 100 Hz;
- 1 kHz;
- 10 kHz.

The final valid range × frequency × amplitude matrix will be established empirically. Not every combination should automatically be considered valid.

## Acquisition flow

1. A timer generates deterministic sample triggers.
2. ADC1/ADC2 capture channels according to the selected acquisition schedule.
3. DMA transfers samples into bounded RAM buffers.
4. DMA ISR only marks blocks ready.
5. DSP executes outside interrupt context.
6. Windows should contain an integer number of excitation cycles whenever practical.

Any deterministic skew between ADC1/ADC2 or rank sequences must be measured and either removed by configuration or compensated in calibration/DSP.

## RET 1× and high-gain channels

`RET_1X` preserves headroom.

`RET_HG` improves SNR for small return signals. Conceptually:

```text
RET = VMID + (RET_HG - VMID) / H_HG(f)
```

where `H_HG(f)` is a calibrated complex transfer response. The current nominal DC gain is approximately 15.47×, but firmware must not treat it as exact or frequency independent.

The Stage 2B.2 solver records an observed complex `H_HG` from RET_HG/RET_1X overlap
evidence when available. The present Rev.1 assumption stores one transfer in the exact
condition record; whether temperature/frequency/amplitude/range need denser HG
modeling remains `REQUIRES_BENCH_VALIDATION`.

The measurement engine chooses the channel with the best useful SNR while rejecting clipping and invalid calibration regions.

## Derived quantities

Given:

```text
Z = R + jX
```

then:

```text
|Z|   = sqrt(R² + X²)
phase = atan2(X, R)
```

Series-equivalent interpretation:

```text
R = Re(Z)
L = X / (2*pi*f)            when X > 0
C = -1 / (2*pi*f*X)         when X < 0
```

ESR, Q, and D may also be derived when confidence and the selected equivalent model make those values meaningful.

## Autorange

The goal is to place `|ZREF|` in the same broad order of magnitude as the DUT while preserving:

- adequate ADC signal;
- safe analog current;
- headroom;
- low distortion;
- a qualified range/frequency/amplitude combination.

Safe range transition:

```text
RANGE_EN=0 -> set A0/A1/A2 -> dead-time -> RANGE_EN=1 -> settling -> acquire
```

## Confidence gates

A reading may be rejected or marked `EXTENDED` / `LOW_CONFIDENCE` due to:

- clipping;
- insufficient SNR;
- excitation outside the expected window;
- instability between blocks;
- inconsistent phase;
- behavior too close to OPEN/SHORT for the selected range;
- an unqualified range/frequency/amplitude combination;
- residual voltage;
- analog saturation or excessive current;
- missing/incompatible calibration.

The firmware should prefer an explicit low-confidence or rejected result over presenting accuracy that has not been established.
