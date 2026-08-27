# `measurement`

Metrology core of WTK.RLCMeter.

## Goal

Keep acquisition, DSP, impedance calculation, autorange, confidence, and calibration application independent from UI and graphics/storage device details.

## Implemented files

```text
measurement_dsp.c/.h
measurement_condition.c/.h
measurement_engine.c/.h
measurement_calibration.c/.h
measurement_calibration_store.c/.h
```

`measurement_dsp` is the Phase 06 fixed-condition math core. `measurement_condition`
is the narrow Rev.1 physical condition-domain contract shared by automatic policy,
calibration requirements, Bringup validation, and future qualification maps.
`measurement_engine` is the Phase 07 Stage 1 automatic session policy layer. `measurement_calibration` and
`measurement_calibration_store` are the Phase 07 Stage 2A portable calibration model,
resolver, and redundant-slot substrate. OPEN/SHORT/LOAD acquisition workflows,
qualification maps, and product UI remain outside this implementation until later
phases.

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

## Calibration substrate

The calibration resolver converts exact condition keys into the existing Phase 06 DSP
inputs:

```text
measurement_adc_calibration_t
measurement_dsp_config_t
measurement_calibrated_result_t
```

The DSP does not parse Flash records. Missing exact calibration may use ideal Bringup/debug
defaults only with explicit `MISSING/uncalibrated` provenance.

The persisted schema v2 uses a pre-DSP physical condition key:

```text
hardware revision
model version
range
frequency
amplitude
```

RET channel and RET strategy are not persistent key dimensions. Phase 05 captures both
return paths and Phase 06 selects the usable return path, so one condition resolution
returns corrections for both RET_1X and RET_HG. The calibration wrapper applies the
selected-channel output correction after raw impedance calculation and then recomputes
derived values.

The persisted format is manually serialized little-endian data with CRC32 and a commit
marker written last. The store uses two W25Q slots, asynchronous erase/program
start/wait states, and preserves the previous usable valid set across interrupted or
incomplete candidate writes.

The Stage 2A.2 Rev.1 condition matrix is 33 calibratable conditions: all six ranges,
three frequencies, and two amplitudes except the hard unsupported `10 Ohm + 500 mVrms`
combination at each frequency. High-Z/high-frequency conditions remain representable
until real qualification evidence marks them otherwise. Calibration records reject
duplicate complete keys; `condition_id` is diagnostic metadata, not the authoritative
lookup key.

## Output

In addition to the measured value, the engine returns quality metadata such as clipping,
selected range/frequency/amplitude, 1X/HG channel, qualification state, confidence
reasons, classification evidence, partial/final status, and retry/rerange/refinement
reason. True SNR, stability, persistent calibration identifiers, and qualification-map
decisions remain later-phase inputs.
