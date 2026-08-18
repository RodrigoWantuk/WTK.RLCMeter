# Rev.1 Bring-up

The objective is to discover faults while energizing the smallest possible portion of the circuit at each step.

## 0. Before soldering

- photograph the bare PCB;
- verify orientation of critical footprints;
- confirm no short between +5V_SYS, +5V_A, +3V3, and GND;
- verify continuity from J_TEST through K1/SAFE paths;
- confirm intended DNP population, especially `D_TVS` / `R_TVS_LINK` and K2 if applicable;
- record PCB revision and BOM version.

## 1. Power section

Assemble passive power components first.

Apply +5V_SYS from a current-limited bench supply.

Verify:

- +5V_SYS;
- +5V_A after RA;
- no unexpected heating;
- no abnormal current consumption.

## 2. VMID and AFE

Assemble the MCP6002 devices and VMID network.

Expected baseline:

```text
VMID_RAW ~1.65 V
VMID     ~1.65 V
```

With PWM disabled, verify VEXC and return outputs are not saturated or oscillating.

## 3. Blue Pill

Before inserting/powering the module:

- verify physical orientation;
- verify 5 V / 3.3 V / GND pins;
- use a minimal firmware image that starts in SAFE.

**Do not rely on the Blue Pill Micro-USB connector as a Rev.1 USB interface**, because PA11/PA12 are reassigned.

Initial programming may use ST-Link through the module SWD pads/header or the STM32 ROM USART1 bootloader where appropriate.

## 4. UART and GPIO

The first firmware should report at least:

```text
boot
firmware version
hardware revision
reset cause
charger state
battery ADC
critical raw ADC values
```

Validate UP/DOWN/OK and PA15 input. Validate K1/K2 command signals initially without energizing relay coils if practical.

## 5. TFT and Flash

- keep both CS lines HIGH before SPI initialization;
- read the W25Q JEDEC ID;
- test write/read/erase in a reserved test sector;
- initialize ILI9341;
- verify solid colors and minimal text;
- verify TFT MISO releases the shared bus while CS is HIGH;
- verify W25Q access does not accidentally select the TFT and vice versa.

## 6. Buzzer and backlight

Outside measurement acquisition:

- test backlight PWM over a conservative range;
- test buzzer around 500 Hz, 1 kHz, and 2 kHz;
- confirm no reset or excessive supply ripple;
- later verify quiet-mode behavior.

## 7. Safety detector

Keep K1 in SAFE.

Test progressively:

- 0 V at terminals;
- small positive/negative DC voltages from a controlled current-limited source;
- validate `ADC_OV_HI/LO`, polarity, conversion, and thresholds;
- increase test voltage only after low-voltage behavior is understood;
- do not begin validation by applying ~100 V directly.

Verify that `CHG_VBUS` prevents K1 from entering MEASURE at the hardware level.

## 8. Range bank

With `RANGE_EN=0`, verify all range branches are off.

Select each range individually with no DUT and very low excitation.

Verify:

- one-hot decoding;
- no two branches enabled simultaneously;
- expected path resistance;
- K2/R0_BANK baseline behavior;
- no unexpected leakage or switching transients.

## 9. Excitation

Begin with a light load / high RREF.

Observe with an oscilloscope:

- PWM carrier;
- filter stages;
- VEXC;
- waveform shape;
- amplitude;
- visible distortion/ripple.

Only after this should testing proceed to the 10 Ω reference range with reduced excitation.

## 10. Acquisition

Capture raw buffers and inspect them on the PC through UART before trusting embedded DSP.

Compare:

- VEXC;
- VMID;
- RET_1X;
- RET_HG;
- relative phase;
- ADC clipping/headroom;
- deterministic sample timing.

## 11. First DUTs

Recommended order:

1. known mid-value resistor;
2. resistors spanning each intended range;
3. known capacitor;
4. known inductor;
5. repeatable OPEN/SHORT fixtures.

Only after raw measurement behavior is understood should automatic autorange and calibration workflows be enabled.

## 12. Bring-up evidence

For every significant bring-up step, record:

- hardware revision and BOM population;
- firmware commit;
- supply voltage/current limit;
- instrument setup;
- measured values/screenshots/logs;
- pass/fail notes;
- any temporary rework.

This evidence becomes the input for qualification and future hardware-revision decisions.
