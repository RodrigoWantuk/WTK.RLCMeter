# `measurement`

Metrology core of WTK.RLCMeter.

## Goal

Keep acquisition, DSP, impedance calculation, autorange, confidence, and calibration application independent from UI and graphics/storage device details.

## Implemented Phase 06 files

```text
measurement_dsp.c/.h
```

The current implementation deliberately keeps autorange, final confidence, persistent
calibration, and product UI outside this directory until later phases.

## Flow

```text
raw ADC samples
   -> channel scaling
   -> synchronous I/Q / single-bin DFT
   -> calibrated complex phasors
   -> impedance equation
   -> derived R/C/L/ESR/Q/D
   -> confidence gates
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

`phasor`, `complex_math`, `impedance`, `autorange`, `confidence`, and calibration application should compile in host tests without HAL/CMSIS dependencies.

## Output

In addition to the measured value, the engine should return quality metadata such as clipping, SNR, stability, selected range/frequency/amplitude, 1X/HG channel, calibration identifier, and retry/rerange reason.
