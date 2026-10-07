# Rev.1 MVP Bench Guide

This guide is for the first assembled Rev.1 board. It prepares evidence; it does not declare accuracy, safety category, or bench validation complete. BRINGUP UART commands are optional engineering diagnostics. Normal PRODUCT navigation, calibration, and measurement use the TFT and buttons; the COM port is needed only to provision or recover the external Resource Pack.

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

- Flash/resource failure may be investigated with BRINGUP diagnostics, but calibration persistence and PRODUCT use require a working W25Q. PRODUCT does not enter normal navigation with a missing, corrupt, or incompatible required Resource Pack.
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

## Optional BRINGUP DUT Diagnostics

This UART path is for early bench diagnosis, not the normal product workflow. Start with isolated passive components only.

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

## Optional BRINGUP Calibration Diagnostics

The following UART commands are available for engineering diagnosis. PRODUCT calibration is performed through the on-device wizard described below; users do not need these commands.

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

## PRODUCT Resource Provisioning

Build and flash PRODUCT Release:

```text
cd Firmware
cmake --preset stm32-release
cmake --build --preset stm32-release
```

On a blank W25Q, or when the required pack is corrupt or incompatible, PRODUCT must show its internal bilingual PC-link recovery screen. This is the only accessible product UI until a valid pack mounts. K1 remains SAFE; the recovery screen does not grant measurement permission. An optional image failure alone should fall back to a plain background rather than invalidate otherwise complete required catalogs/fonts.

Build and inspect the current Resource Pack from the repository root. The outer wire format is v2 and the PRODUCT resource API is v4; older API v3 packs must be rebuilt:

```text
python Firmware/tools/resource_pack_tool.py bundle Firmware/assets/resource_manifest.json -o Firmware/build/resources/product.wrp2 --summary Firmware/build/resources/product.json --stream Firmware/build/resources/product.wpc
python Firmware/tools/resource_pack_tool.py inspect Firmware/build/resources/product.wrp2
```

Connect the USART1 serial adapter, close any terminal holding the port, and replace `COM5` with the actual port:

```text
python -m pip install pyserial
python Firmware/tools/resource_pack_tool.py upload Firmware/build/resources/product.wrp2 --port COM5
```

The uploader uses 115200 baud and PC-link acknowledgments. The firmware remounts the pack after a completed update. Do not place the pack in MCU internal Flash. See [`../Firmware/assets/README.md`](../Firmware/assets/README.md) for source regeneration and pack inspection.

PASS: upload completes, the required pack mounts, and PRODUCT leaves the recovery screen for its normal boot/calibration flow. Power-cycle and verify that EN and PT-BR text, fonts, icons, and the optional READY art render correctly without proportional SRAM growth. FAIL: failed upload, persistent recovery screen, shared SPI-bus errors, or missing required text/fonts. Do not proceed to normal PRODUCT operation; inspect the pack, W25Q, UART wiring, and PC-link diagnostics. Resource failure must not bypass safety or calibration gates.

## PRODUCT Calibration Through Menus

After resource admission, a missing/invalid active calibration enters the mandatory calibration-required flow; it must not enter READY. Use short OK to start the on-device wizard. For manual recalibration with a valid active calibration, long OK opens the main menu; select Calibration with UP/DOWN and short OK. The Calibration status screen uses UP/DOWN to select a persisted LOAD preset, then short OK to start the wizard. Follow each displayed OPEN, SHORT, and LOAD fixture prompt and confirm with short OK. Long OK requests cancellation; allow any active hazardous transaction to drain safely before changing fixtures.

The wizard covers the 33 supported Rev.1 conditions: six ranges times three frequencies times two amplitudes, excluding 10 ohm with 500 mVrms. LOAD is a preset value, not a measured value entered numerically. The selected preset must match the actual known load used for each prompted range; otherwise the saved coefficients cannot be treated as accurate. Verify load values with appropriate external equipment and record them with the bench data. Do not claim absolute accuracy from nominal presets alone.

| Range | NOMINAL | E12 LOW | E12 HIGH |
| --- | ---: | ---: | ---: |
| 10 ohm | 10 ohm | 12 ohm | 47 ohm |
| 100 ohm | 100 ohm | 120 ohm | 470 ohm |
| 1 kohm | 1 kohm | 1.2 kohm | 4.7 kohm |
| 10 kohm | 10 kohm | 12 kohm | 47 kohm |
| 100 kohm | 100 kohm | 120 kohm | 470 kohm |
| 1 Mohm | 1 Mohm | 820 kohm | 1 Mohm |

PASS: the wizard completes, commits, enters READY, and active calibration remains valid after a power cycle. A cancelled or failed manual recalibration must not silently replace a previously valid active calibration. FAIL: unavailable W25Q storage, incomplete campaign, failed commit, unexpected relay/range activity, or measurement access without valid calibration. Stop and inspect the safety/calibration diagnostics.

## PRODUCT Measurement

With a valid required Resource Pack and active calibration:

- READY is available only after the normal self-test and safety prerequisites.
- Short OK starts one click measurement session; use isolated passive DUTs only.
- UP/DOWN navigate available result pages; long OK opens the menu.
- Product measurement never uses ideal calibration fallback.

Start with known resistors in at least two ranges, then known capacitors and inductors, OPEN, and SHORT. Verify each attempt returns K1 to SAFE, stops excitation, and leaves no unexpected range activity. Compare readings with independent references; record uncertainty rather than tuning to a single component. Exercise both languages, button navigation, backlight, and repeated measurements. A missing or corrupt required Resource Pack must return to PC-link recovery, not to normal measurement. These observations are bring-up evidence, not accuracy or electrical-safety qualification.
