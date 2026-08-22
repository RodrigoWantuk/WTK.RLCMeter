# `measurement`

Metrology core of WTK.RLCMeter.

## Goal

Keep acquisition, DSP, impedance calculation, autorange, confidence, and calibration application independent from UI and graphics/storage device details.

## Implemented files

```text
measurement_dsp.c/.h
measurement_engine.c/.h
```

`measurement_dsp` is the Phase 06 fixed-condition math core. `measurement_engine` is the
Phase 07 Stage 1 automatic session policy layer. Persistent calibration, qualification
maps, and product UI remain outside this implementation until later phases.

## Flow

```text
raw ADC samples
   -> channel scaling
   -> synchronous I/Q / single-bin DFT
   -> calibrated complex phasors
   -> impedance equation
   -> derived R/C/L/ESR/Q/D
   -> automatic session policy
   -> confidence gates / partial or final result
   -> accept / retry / rerange / reject
```

## Central equation

```text
Vs = VEXC - VMID
Vx = RET  - VMID
Zx = ZREF * Vx / (Vs - Vx)
```

## High-gain channel

The current hardware has nominal gain around 15.47× on `RET_HG`, but code must use a calibrated complex response by frequency/range/amplitude where required.

Phase 06 represents this as:

```text
RET = VMID + (RET_HG - VMID) / H_HG
```

where `H_HG` is a complex transfer. The ideal synthetic default is
`15.468085 + j0`.

## Phasor convention

The implemented synchronous detector returns peak-voltage phasors:

```text
x[n] = dc + Re{Vpeak * exp(j * theta[n])}
Vpeak = (2 / N) * sum(x[n] * (cos(theta[n]) - j*sin(theta[n])))
```

Positive phase means the waveform leads the cosine reference. Integer-cycle windows make
the large VMID/DC offset reject naturally in the single-bin extraction.

## Numeric strategy

The STM32 path uses `float` and project-owned complex helpers. It does not call runtime
`sin()`/`cos()` in the sample loop and currently links without `libm`; host Python tools
provide independent double-precision reference calculations.

## Testability

`phasor`, `complex_math`, `impedance`, `autorange`, `confidence`, classification, and
calibration application should compile in host tests without HAL/CMSIS dependencies.

## Automatic session engine

The Phase 07 Stage 1 engine consumes completed fixed-condition attempt results. It does
not start ADC/DMA, switch GPIOs, energize K1, issue permits, render UI, or touch W25Q.

The no-history initial probe is:

```text
RREF = 1 kOhm
frequency = 1 kHz
amplitude = 100 mVrms
```

The engine uses bounded attempt history:

```text
maximum attempts = 6
maximum range transitions = 4
maximum frequency refinements = 1
```

Each emitted attempt has structured metadata: range, frequency, amplitude, RET strategy,
attempt number, and reason. A previous successful result may seed the next Live session
as a performance hint, but it carries no safety authorization or active hardware state.

The confidence output is semantic (`NOMINAL`, `EXTENDED`, `LOW_CONFIDENCE`,
`REJECTED`) plus reason flags. `NOMINAL` requires explicit qualification evidence; the
current unqualified software default cannot claim nominal accuracy.

## Output

In addition to the measured value, the engine returns quality metadata such as clipping,
selected range/frequency/amplitude, 1X/HG channel, qualification state, confidence
reasons, classification evidence, partial/final status, and retry/rerange/refinement
reason. True SNR, stability, persistent calibration identifiers, and qualification-map
decisions remain later-phase inputs.
