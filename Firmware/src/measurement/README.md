# `measurement`

Metrology core of WTK.RLCMeter.

## Goal

Keep acquisition, DSP, impedance calculation, autorange, confidence, and calibration application independent from UI and graphics/storage device details.

## Planned files

```text
measurement_types.h
acquisition.c/.h
phasor.c/.h
complex_math.c/.h
impedance.c/.h
autorange.c/.h
confidence.c/.h
calibration_apply.c/.h
measurement_engine.c/.h
```

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

## Testability

`phasor`, `complex_math`, `impedance`, `autorange`, `confidence`, and calibration application should compile in host tests without HAL/CMSIS dependencies.

## Output

In addition to the measured value, the engine should return quality metadata such as clipping, SNR, stability, selected range/frequency/amplitude, 1X/HG channel, calibration identifier, and retry/rerange reason.
