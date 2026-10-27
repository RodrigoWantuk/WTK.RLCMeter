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

- 10 Hz;
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

Stage 2B.2.1 defines persisted `H_HG` as an effective normalized path gain, not a
standalone physical op-amp gain:

```text
H_HG_effective = (RET_HG_raw / VEXC_2) / (RET_1X / VEXC_1)
```

This aligns the HG transfer plane with the 1X transfer plane and absorbs systematic
VEXC1/VEXC2 path mismatch visible in calibration evidence. The current OSL model
normalizes raw HG evidence with the same effective transfer during fitting and runtime.
A persisted condition without observed HG overlap keeps HG unavailable for calibrated
runtime channel selection; Lab/debug ideal fallback remains explicitly uncalibrated.

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

For a capacitive **series** model below self-resonance, `ESR = Re(Z)` and
`D = ESR / |X|`; for an inductive series model, winding-loss estimate is `Re(Z)`
and `Q = |X| / Re(Z)` when the denominator is resolvable. These are conditional
AC-equivalent quantities at the measured frequency, not DC resistance or a universal
component datasheet value. DCR and low-voltage leakage require separate guarded DC
measurement modes. Three supported AC frequencies do not support a general quantified
ESL or SRF claim; high-voltage insulation resistance is outside Rev.1.

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

### Bounded low-impedance exploration

Automatic range selection first follows the existing DUT/reference ratio,
clipping and weak-signal criteria. It does not sweep every RREF: a larger RREF
can further reduce a small DUT's return signal. After these quality steps, a
finite apparently resistive result at 1 kHz on RREF 10 or 100 ohm also requests
one 10-kHz attempt at the same amplitude/RREF, provided the selected return path is usable and unclipped. Reactance can be zero or
have either sign at this stage; the check investigates possible hidden
reactance rather than presuming a component type. A finite negative estimated
R may also be explored; unchanged publication guards still reject unsupported
negative resistance/loss.

The existing six-attempt/four-range/one-frequency limits and no-repeat rule
still apply. Actual SHORT/OPEN, invalid data, safety abort and cancellation
retain their priorities. No 10-ohm/500-mVrms condition is allowed. Higher-RREF
and already-10-kHz resistive results do not trigger this additional check.
Every requested condition uses the guarded hardware FSM. PC-provisioned PRODUCT
may continue within an explicitly bounded continuous K1 authorization; fixed
captures retain fresh individual permits (see plan 24).

The second frequency is supporting evidence. Existing primary selection,
classification thresholds and conservative disagreement handling remain
unchanged: a resistive/reactive disagreement can finish MIXED/UNKNOWN, and
an extra attempt is not proof of sub-microhenry capability or accuracy.
See [plan 23](../plans/23-Bounded-Low-Impedance-Search.md).

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

## Classification evidence admission

A mathematically finite candidate does not automatically qualify as classification
or frequency-trend evidence. The selected return must be usable, unclipped and
finite, with peak at least the existing 2-mV weak-return policy threshold; failed
or clipped captures do not vote. Raw history remains complete. The same predicate
is used for frequency-trend pairs, so weak rerange attempts cannot poison that
trend either. Primary quality ordering and conservative handling of genuinely
contradictory usable captures remain unchanged. This is an evidence policy,
not a new calibrated accuracy bound. See plan 24.

## Seven-sample estimate after autorange

PC-provisioned PRODUCT uses the primary calibrated impedance plus six further
valid coherent captures at exactly the same RREF/frequency/amplitude/RET path
and calibration set/model/condition. Each capture still runs the original
phasor extraction and complex calibration. R and signed X are sorted separately;
the two highest and two lowest of each coordinate are discarded, and the middle
three coordinates are averaged. Different R/X retained-index masks are allowed
and stored with all seven raw complex points in session RAM. This is a
coordinate-wise trimmed estimate, not three retained whole captures.

Magnitude, wrapped phase, conditional C/L and Q/D are recomputed from that one
impedance. Wrapped angles and independently derived C/L/Q/D are never averaged.
The original up-to-six-condition search history remains unchanged; final primary
metadata explicitly marks the aggregate. Selected/source/denominator peak metadata
uses minima across admitted points. Confidence cannot improve merely from
averaging, and a contrary aggregate cannot override unresolved classification
or assert an incompatible reactive sign. Existing loss guards still apply.

At most eight additional windows allow two rejected captures. Clipped, weak
(selected RET below 2 mV), nonfinite, failed, wrong-path or mismatched-calibration
captures cannot enter the seven points. All seven are required: failure is
explicit, with SAFE cleanup and no silent single-reading fallback. This rejects
spikes, not systematic error; no new accuracy or sub-microhenry claim is made.
Safety/interlock/residual/battery monitoring is not delayed or statistically
filtered by this metrology estimator. Fixed calibration and diagnostic captures
retain their original behavior. See [plan 25](../plans/25-Trimmed-Measurement-Estimate.md).

## Three-frequency classification in factory PRODUCT

Factory PRODUCT surveys the existing 1-kHz, 10-kHz and 10-Hz profiles before
normal final interpretation, including resistive and near-OPEN/SHORT probes.
No new frequency or OSL model is introduced. Acquired DSP rejection is distinct
from acquisition failure; with a present source and weak return, lower RREF
before increasing amplitude, then try other frequencies under explicit bounds.
A failed/unchanged processing output cannot reuse preceding DSP data.

Usable low-frequency resistance dominance does not contradict inductive evidence
at higher frequency, and usable high-frequency resistance dominance does not
contradict capacitive evidence at lower frequency. Preserve sign/trend/quality
checks and reject opposite reactive signs. Only usable unambiguous evidence at
all three frequencies can confirm a resistance-dominated result. Below the
~1-ohm engineering lower target, such a result remains unknown/impedance to avoid
asserting that an unresolved small choke is physically a resistor.

Primary selection uses one supported, usable condition after the combined
classification, then the existing seven-sample coordinate estimator applies at
that exact condition and frequency. At 10 Hz C/L derivation uses 10 Hz, not a
1-kHz default. No averaging across frequencies, raw-history replacement,
calibration changes, qualification upgrades or synthetic accuracy claims occur.
Synthetic 4,700/10,000-uF cases demonstrate algorithmic low-frequency recovery;
actual large electrolytics, excitation/polarity, settling and accuracy remain
REQUIRES_BENCH_VALIDATION.
