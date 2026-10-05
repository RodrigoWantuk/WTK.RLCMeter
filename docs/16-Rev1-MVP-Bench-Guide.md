# Rev.1 MVP Bench Guide

This guide is for the first assembled Rev.1 board. It prepares evidence; it does not declare accuracy, safety category, or bench validation complete.

## Setup

1. Build and flash BRINGUP:

```text
cd Firmware
cmake --preset stm32-bringup
cmake --build --preset stm32-bringup
```

Flash `build/stm32-bringup/WTK.RLCMeter.hex` or `.bin` using the chosen SWD tool.

2. Connect USART1 at 115200 8N1.

PASS: boot banner appears and SWD remains usable.
FAIL: no UART or repeated reset. Stop and debug power, clock, reset, and SWD before connecting a DUT.

## Safe Boot

Command:

```text
lab mvp status
```

External checks:

- K1 coil command remains SAFE/LOW.
- RANGE_EN is disabled.
- PA8 excitation is inactive.

PASS: firmware reports no latched safety fault, K1 SAFE, range disabled/not ready for measurement unless explicitly prepared, and excitation off.
FAIL: any relay/range/excitation activity at boot. Do not proceed.

## Basic Peripherals

Commands:

```text
lab flash info
lab range status
lab charger status
lab sensors status
lab adc status
lab fault status
```

Expected behavior:

- Flash either reports a JEDEC device or is clearly not detected.
- Charger state follows PA15/CHG_VBUS.
- VMID is plausible before residual testing.
- Battery and NTC telemetry are present or explicitly unknown.

Proceeding after failure:

- Flash/resource failure is acceptable for BRINGUP measurement debugging, but calibration persistence and PRODUCT use require W25Q.
- Sensor/safety faults block measurement and must be understood before DUT testing.

## Range Bank

Commands:

```text
lab range 10r
lab range 100r
lab range 1k
lab range 10k
lab range 100k
lab range 1m
lab range off
```

External checks:

- RANGE_EN goes low before address changes.
- Address bits match the requested range.
- RANGE_EN goes high only after the configured dead time.

FAIL: address changes while enabled, invalid one-hot behavior, or unexpected range output. Stop range testing.

## Excitation and Raw Capture

Commands:

```text
lab metrology capture 100hz 100mv 1k
lab metrology capture 1khz 100mv 1k
lab metrology capture 10khz 100mv 1k
```

External checks:

- PA8 carrier is present only during capture.
- Filtered excitation is centered near VMID.
- Frequencies are grossly correct at 100 Hz, 1 kHz, and 10 kHz.
- UART raw dump starts only after capture stops.

Do not tune amplitude or phase from these first observations. Record them as unqualified bench data.

## First DUT Measurements

Start with isolated passive components only.

1. Known mid-range resistor:

```text
lab measure auto
```

PASS: output reaches `AUTO_RESULT`, reports a resistive or plausible impedance result, and explicitly reports either `calibration=PERSISTED` or `calibration=IDEAL_UNQUALIFIED`.
FAIL: safety abort, ADC/DMA failure, range failure, or generic no-valid-condition. Read `lab mvp status`, fix the reported blocker, and retry.

2. Repeat with:

- second resistor in another range;
- known capacitor;
- known inductor;
- OPEN;
- SHORT.

The first uncalibrated results may be approximate. They are only evidence that the chain is alive.

## Calibration

After gross measurement behavior is sane, run the existing 33-condition OSL campaign with the BRINGUP calibration commands:

```text
lab cal campaign begin <freq> <amp> <range>
lab cal acquire open <freq> <amp> <range>
lab cal acquire short <freq> <amp> <range>
lab cal acquire load <freq> <amp> <range> <load_mohm>
lab cal campaign solve
lab cal campaign commit
lab cal rescan
lab cal status
```

Repeat for all Rev.1-supported conditions. The domain is six ranges times three frequencies times two amplitudes, minus the forbidden 10 ohm plus 500 mVrms conditions, for 33 conditions.

PASS: active calibration becomes valid and survives reload/rescan.
FAIL: do not proceed to PRODUCT measurement until persistence and active validation are understood.

## PRODUCT Measurement

Build and flash PRODUCT Release:

```text
cd Firmware
cmake --preset stm32-release
cmake --build --preset stm32-release
```

Expected behavior:

- Missing active calibration routes to the calibration-required/product wizard path.
- Valid active calibration allows READY.
- Short OK starts one click measurement session.
- Product never uses ideal calibration fallback.

External resources may be missing or corrupt; PRODUCT must remain usable with internal fallback text, but calibration remains a hard measurement gate.
