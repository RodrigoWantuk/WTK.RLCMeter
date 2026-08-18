# UI, UX, and Diagnostics

## Display

Controller: ILI9341, 240×320, SPI.

Primary pixel format: RGB565.

A full RGB565 frame requires:

```text
240 * 320 * 2 = 153600 bytes
```

This exceeds the available RAM on the STM32F103C8T6, so the UI uses incremental rendering and streaming rather than a full framebuffer.

## Assets

Images, icons, and fonts may reside in the W25Q Flash.

```text
W25Q -> small RAM buffer -> ILI9341
```

An initial practical buffer size is approximately 512–2048 bytes, subject to profiling.

### Asset pack

Planned simple format:

```text
header
asset table
asset data...
```

Each entry contains at least:

```text
id
offset
length
width
height
format
crc32
```

No FAT/LittleFS is planned for the first implementation.

## Planned screens

### Startup

- WTK.RLCMeter branding;
- firmware version;
- self-test progress/status.

### Home / Measure

- primary R/L/C/Z value;
- unit/prefix;
- frequency;
- selected range;
- AUTO/MANUAL state where applicable;
- battery state;
- charger/external-power indication;
- confidence class.

### Details

- `|Z|`;
- phase;
- R and X;
- ESR/Q/D when meaningful;
- comparison across frequencies;
- simple response/phase visualizations.

### Component visualizations

The UI may provide educational or diagnostic views derived from the measured result without changing the metrology core:

- voltage/current vector diagram showing phase lead/lag;
- predominantly resistive/capacitive/inductive classification;
- comparison of `|Z|` and phase at 100 Hz, 1 kHz, and 10 kHz;
- phase visualization for inductors/coils;
- calculated capacitor charge/discharge curves based on measured capacitance and user-selected parameters.

A capacitor charge curve is initially a **derived visualization**, not a claim that the instrument captured a transient waveform. A future time-domain acquisition mode would require separate implementation and qualification.

### Calibration

Wizard-driven OPEN/SHORT/LOAD procedures with explicit instructions and validation.

### Diagnostics

Display engineering values useful during bring-up:

```text
VMID
VEXC
RET_1X
RET_HG
raw ADC values
ADC_OV_HI/LO
VBAT
NTC
selected range
K1/K2 state
CHG_DETIO
W25Q JEDEC ID/status
TFT status
clipping/SNR/confidence
```

### Event console

Compact ring-buffer example:

```text
[0001.203] BOOT
[0001.215] FLASH EF4017
[0001.244] TFT OK
[0001.310] SAFE residual=0.08V
[0001.315] READY
```

## Controls

Three buttons:

- UP;
- DOWN;
- OK.

Long-press and repeat are handled in software.

Suggested semantics:

- UP/DOWN: navigate or modify;
- OK: confirm / request measurement;
- long OK: menu/back-to-home behavior;
- UP+DOWN: optional diagnostic/calibration shortcut.

## Backlight

PB0 controls brightness through PWM.

Planned behavior:

- configurable brightness;
- auto-dimming after inactivity;
- backlight off in sleep where appropriate;
- stable/fixed duty during critical acquisition if required to reduce noise coupling.

## Buzzer

The passive piezo is mounted externally in the enclosure. PB1 drives a BC817 low-side switch. A 4.7 kΩ resistor across the piezo discharges its capacitance.

Suggested patterns:

| Event | Feedback |
|---|---|
| button | optional very short click/tone |
| successful result | two rising tones |
| error | three short tones |
| residual-voltage warning | distinct alert pattern |
| low battery | descending tones |
| calibration completed | positive completion sequence |

The buzzer is always muted during metrology acquisition.

## Asset failure fallback

A minimal bitmap font and a basic diagnostic/error screen should remain in MCU internal Flash so the instrument can report fundamental failures even when the W25Q is missing or corrupted.
