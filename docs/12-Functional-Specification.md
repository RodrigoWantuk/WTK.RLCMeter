# Functional Specification

This document consolidates expected WTK.RLCMeter behavior without confusing **engineering targets**, **implemented features**, and **qualified capabilities**.

## Maturity labels

- **PLANNED** — architecture is decided but implementation is incomplete.
- **IMPLEMENTED** — code/hardware exists but may not be qualified.
- **QUALIFIED** — behavior has been measured and accepted against defined criteria.
- **EXPERIMENTAL** — available for controlled evaluation without product-level guarantees.

At the current stage, many functions are between PLANNED and first bring-up.

## Rev.1 measurement scope

- complex impedance of de-energized passive components;
- two-wire operation;
- calculation of `R + jX`;
- derivation of resistance, capacitance, and inductance;
- magnitude `|Z|` and phase;
- ESR, Q, and D when mathematically and metrologically meaningful;
- baseline frequencies of 100 Hz, 1 kHz, and 10 kHz;
- baseline excitation amplitudes of 100 mVrms and 500 mVrms;
- automatic selection among six RREF values;
- automatic selection between `RET_1X` and `RET_HG`;
- retry/rerange when the first acquisition does not satisfy quality criteria.

## Engineering range targets

Initial targets for Rev.1 characterization:

| Quantity | Target span |
|---|---:|
| R | ~1 Ω to 10 MΩ |
| C | ~1 nF to 10 mF |
| L | ~10 µH to 10 H |
| Frequencies | 100 Hz / 1 kHz / 10 kHz |
| Excitation | 100 mVrms / 500 mVrms |

These are not accuracy guarantees. Qualification will define actual usable sub-ranges and allowed operating combinations.

## Excitation rules

- 500 mVrms is forbidden with the 10 Ω RREF;
- amplitude may be reduced by current/headroom policy;
- frequency may be changed automatically to improve observability;
- combinations not covered by qualification should not appear as ordinary high-confidence measurements.

## Confidence classes

A result may be degraded or rejected because of:

- ADC clipping;
- insufficient SNR;
- block-to-block instability;
- inconsistent phase;
- excitation outside the expected range;
- AFE saturation;
- behavior too close to OPEN/SHORT;
- an unqualified range/frequency/amplitude condition;
- residual voltage;
- charger connected;
- missing/incompatible calibration.

Planned classes:

- `NOMINAL` — main qualified region;
- `EXTENDED` — usable result outside the main qualified region;
- `LOW_CONFIDENCE` — diagnostic/indicative result only;
- `REJECTED` — not published as a valid measurement.

## Autorange

Reference bank:

```text
10 Ω
100 Ω
1 kΩ
10 kΩ
100 kΩ
1 MΩ
```

Selection considers headroom, current, SNR, 1×/HG choice, frequency, amplitude, and qualification—not only DUT order of magnitude.

Every reference transition follows:

```text
RANGE_EN=0 -> set A0/A1/A2 -> dead-time -> RANGE_EN=1 -> settling
```

## Safety requirements

Rev.1 remains SAFE by default.

MEASURE is permitted only when:

- required boot/self-test steps have completed;
- residual voltage is below the allowed threshold;
- `CHG_VBUS` is not active;
- the selected range is valid;
- no critical supply or application fault exists.

Failure/reset returns K1 to SAFE.

The residual-voltage network has an approximate ±100 V observation envelope, but this does **not** authorize energized measurements and does not represent a CAT rating.

## Battery and charger behavior

The carrier receives from the external 1S power module:

```text
VBAT_PROT
+5V_SYS
GND
CHG_VBUS
```

Planned firmware behavior:

- battery-level estimation;
- low-battery warning;
- charger indication;
- MEASURE lockout while charging;
- backlight dimming policy;
- safe behavior when supply conditions become unreliable.

## UI requirements

Planned screens:

1. startup/splash;
2. main measurement;
3. impedance details;
4. graphs/derived visualizations;
5. settings;
6. calibration;
7. diagnostics;
8. device/firmware information.

The main screen should prioritize readability of the dominant quantity, unit, frequency, confidence, and safety/status information.

## User controls

Three buttons:

- UP;
- OK;
- DOWN.

Driver-level events include press, release, long press, and repeat where applicable.

A rotary encoder is not part of Rev.1.

## Audible feedback

The passive piezo is external and driven through PB1/BC817.

Planned patterns include:

- short confirmation;
- invalid action;
- completed measurement;
- residual-voltage alert;
- low-battery warning;
- critical fault.

The buzzer stays silent during acquisition.

## Backlight

PB0 controls TFT backlight PWM.

Planned policies:

- configurable brightness;
- auto-dimming after inactivity;
- stable duty during acquisition;
- optional fixed qualified duty if bench tests show measurable coupling.

## TFT and assets

ILI9341 shares SPI with W25Q.

Requirements:

- no full-screen framebuffer;
- incremental rendering;
- large assets stored in external Flash;
- block streaming;
- no Flash erase/program during critical acquisition;
- explicit mutually exclusive chip-select control.

## Diagnostics

Laboratory builds should expose:

- firmware/hardware revision;
- uptime/reset reason;
- application state;
- raw ADC values;
- estimated VEXC/VMID/RET;
- active RREF;
- K1/K2 state;
- battery, NTC, and `CHG_VBUS`;
- W25Q JEDEC/status;
- clipping, SNR, and confidence;
- recent faults/events.

## Calibration

Planned workflows:

- OPEN;
- SHORT;
- LOAD when required by the final correction model;
- complex correction by frequency/range/amplitude/channel;
- records versioned by hardware revision;
- CRC-protected, power-loss-tolerant persistence.

Calibration corrects stable/repeatable error. It does not correct unstable contacts, moving cables, changing humidity, or unpredictable leakage.

## Explicitly outside Rev.1

- native USB device operation;
- CAT-rated measurements;
- direct mains measurement;
- ~400 Vrms AC direct measurement;
- ~600–800 VDC direct measurement;
- Kelvin/4-wire operation;
- external ADC;
- mandatory RTOS;
- full filesystem on external Flash;
- Arduino/INO firmware.

See [`14-Future-Extensions.md`](14-Future-Extensions.md) for future-revision concepts.
