# Pinout and Interfaces — Rev.1

## STM32F103C8T6 pin allocation

| MCU pin | Net / function | Notes |
|---|---|---|
| PA0 | ADC_VEXC | ADC |
| PA1 | ADC_VMID | ADC |
| PA2 | ADC_RET_1X | ADC |
| PA3 | ADC_RET_HG | ADC |
| PA4 | ADC_OV_HI | residual-voltage ADC |
| PA5 | ADC_OV_LO | residual-voltage ADC |
| PA6 | ADC_BAT | battery ADC |
| PA7 | ADC_NTC | temperature ADC |
| PA8 | PWM_EXC | TIM1_CH1 |
| PA9 | DEBUG_TX | USART1 TX |
| PA10 | DEBUG_RX | USART1 RX |
| PA11 | K2_CMD | conflicts with native USB D- |
| PA12 | FLASH_CS | conflicts with native USB D+ |
| PA13 | SWDIO | reserved for SWD |
| PA14 | SWCLK | reserved for SWD |
| PA15 | CHG_DETIO / USB_CONNECTED | requires freeing JTAG pins |
| PB0 | TFT_BL | backlight PWM, TIM3_CH3 |
| PB1 | IO_BUZZ | piezo control output |
| PB3 | SW_UP | requires freeing JTAG pins |
| PB4 | SW_DOWN | requires freeing JTAG pins |
| PB5 | RANGE_A0 | GPIO |
| PB6 | RANGE_A1 | GPIO |
| PB7 | RANGE_A2 | GPIO |
| PB8 | RANGE_EN | GPIO |
| PB9 | K1_CMD | GPIO |
| PB10 | TFT_RST | GPIO |
| PB11 | TFT_DC | GPIO |
| PB12 | TFT_CS | SPI slave select |
| PB13 | TFT_SCK | SPI2 SCK through 33 Ω series resistor |
| PB14 | TFT_MISO | SPI2 MISO |
| PB15 | TFT_MOSI | SPI2 MOSI through 33 Ω series resistor |
| PC13 | SW_OK | button |
| PC14 | NC | reserve / LSE-related pin |
| PC15 | NC | reserve / LSE-related pin |

## JTAG/SWD remap

PA15/PB3/PB4 are JTAG pins after reset. Firmware must disable JTAG while retaining SWD so these pins become available as GPIO. PA13/PA14 remain dedicated to SWD.

## Native USB limitation

The Blue Pill Micro-USB connector uses PA11/PA12 for USB D-/D+.

In Rev.1 those pins are already assigned to board functions. Therefore:

- native USB device mode is **not available**;
- firmware must not initialize the STM32 USB device peripheral as a product interface;
- the Blue Pill Micro-USB connector must not be treated as a console/communications port for Rev.1;
- bring-up and diagnostics use UART and/or SWD;
- restoring native USB requires a future pinout/hardware revision.

This is separate from `CHG_VBUS`: the external 1S charge/boost module has its own USB/5 V input and reports charger presence through J_PWR.

## Connectors

### J_PWR — 1×4

```text
1 VBAT_PROT
2 +5V_SYS
3 GND
4 CHG_VBUS
```

### J_TEST — 2 positions

```text
1 TEST_HI
2 TEST_LO
```

### J_BTN — 1×4

```text
1 GND
2 SW_UP
3 SW_OK
4 SW_DOWN
```

Buttons are external to the main PCB. Firmware uses pull-ups and interprets closure to GND.

### J_UART — 1×3

```text
1 GND
2 DEBUG_TX (PA9, STM32 output)
3 DEBUG_RX (PA10, STM32 input)
```

`DEBUG_RX/TX` names are from the STM32 point of view. Adapter RX connects to
pin 2 and adapter TX to pin 3. Use 3.3 V logic and GND only, without an adapter
power lead. The pre-A05 documentation reversed pins 2/3.

### BUZZER1 — on-board two-pin footprint

```text
1 +5V_SYS
2 switched low side (QBUZZ collector)
```

There is no J_BUZZ connector in the live source/fabrication BOM. QBUZZ is a BC817
low-side switch; RPBUZZ is 4.7 kΩ across the load. **HOLD BUZZER1**: the actual
BOM specifies a 9–15 V internally driven magnetic indicator, incompatible with
the 5 V rail and intended passive tone generation. Resolve the component type,
drive and pin pitch before assembly; see [A05](review/a05/README.md).

### J_TFT — 1×9, 2.54 mm pitch

```text
1 +3V3
2 GND
3 TFT_CS
4 TFT_RST
5 TFT_DC
6 TFT_MOSI
7 TFT_SCK
8 LED connection through R_TFT_LED (0 Ω) to PB0
9 TFT_MISO
```

This is the extracted source and fabrication pin order, replacing the historical
2×5 description. It does not establish compatibility with an unspecified ILI9341
module. **HOLD R_TFT_LED and display connection** until the module's pinout,
mechanical fit and LED input type/current are verified. The carrier has no
backlight power driver or current-limiting resistor. Raw LED current must not
be sourced from PB0. Keep the SPI cable short with a GND return.

## Pinout as a firmware contract

Firmware agents must treat this document and the current PCB/schematic as the hardware contract. Pin reassignment is not a software convenience change; it requires hardware review and documentation updates.
