# Pinout e interfaces — Rev.1

## STM32F103C8T6

| Pino MCU | Net / função | Observação |
|---|---|---|
| PA0 | ADC_VEXC | ADC |
| PA1 | ADC_VMID | ADC |
| PA2 | ADC_RET_1X | ADC |
| PA3 | ADC_RET_HG | ADC |
| PA4 | ADC_OV_HI | ADC residual |
| PA5 | ADC_OV_LO | ADC residual |
| PA6 | ADC_BAT | bateria |
| PA7 | ADC_NTC | temperatura |
| PA8 | PWM_EXC | TIM1_CH1 |
| PA9 | DEBUG_TX | USART1 TX |
| PA10 | DEBUG_RX | USART1 RX |
| PA11 | K2_CMD | **conflita com USB D-** |
| PA12 | FLASH_CS | **conflita com USB D+** |
| PA13 | SWDIO | reservado no módulo |
| PA14 | SWCLK | reservado no módulo |
| PA15 | CHG_DETIO / USB_CONNECTED | requer liberar JTAG |
| PB0 | TFT_BL | backlight PWM, TIM3_CH3 |
| PB1 | IO_BUZZ | piezo, saída controlada por timer/software |
| PB3 | SW_UP | requer liberar JTAG |
| PB4 | SW_DOWN | requer liberar JTAG |
| PB5 | RANGE_A0 | GPIO |
| PB6 | RANGE_A1 | GPIO |
| PB7 | RANGE_A2 | GPIO |
| PB8 | RANGE_EN | GPIO |
| PB9 | K1_CMD | GPIO |
| PB10 | TFT_RST | GPIO |
| PB11 | TFT_DC | GPIO |
| PB12 | TFT_CS | SPI slave select |
| PB13 | TFT_SCK | SPI2 SCK via RSCK 33 Ω |
| PB14 | TFT_MISO | SPI2 MISO |
| PB15 | TFT_MOSI | SPI2 MOSI via RMOSI 33 Ω |
| PC13 | SW_OK | botão |
| PC14 | NC | reserva marginal / LSE |
| PC15 | NC | reserva marginal / LSE |

### Liberação de PA15/PB3/PB4

São pinos JTAG por default. No início do firmware deve-se desabilitar JTAG mantendo SWD, liberando PA15/PB3/PB4 como GPIO. PA13/PA14 podem permanecer SWD.

## USB da Blue Pill — restrição importante

O conector Micro-USB do módulo usa PA11/PA12 como USB D-/D+.

Na Rev.1 esses pinos já estão conectados a `K2_CMD` e `FLASH_CS`. Portanto:

- USB device nativo **não funciona** com este pinout;
- o firmware não deve inicializar USB;
- o Micro-USB da Blue Pill não deve ser tratado como porta de comunicação da Rev.1;
- console e bring-up devem usar UART e/ou SWD disponível no próprio módulo;
- suporte USB real exige revisão do pinout em hardware futuro.

Isto é diferente de `CHG_VBUS`: o carregador 1S externo possui sua própria entrada USB/5 V e informa sua presença através de J_PWR.

## Conectores

### J_PWR — 1×4

```text
1 VBAT_PROT
2 +5V_SYS
3 GND
4 CHG_VBUS
```

### J_TEST — 2 vias

```text
1 TEST_HI
2 TEST_LO
```

### J_KEY — 1×4

```text
1 GND
2 SW_UP
3 SW_OK
4 SW_DOWN
```

Botões ficam externos à placa; firmware usa pull-up e interpreta fechamento para GND.

### J_UART — 1×3

```text
1 GND
2 DEBUG_RX
3 DEBUG_TX
```

`DEBUG_RX/TX` são nomeados do ponto de vista do STM32.

### J_BUZZ — 1×2

```text
1 BUZZ_LOW
2 +5V_SYS
```

O piezo passivo externo é ligado entre os dois pinos. `BUZZ_LOW` é chaveado por BC817. Há 4,7 kΩ em paralelo com o piezo para descarga de sua capacitância.

### J_TFT — 2×5

```text
1  TFT_DC
2  TFT_MISO
3  TFT_RST
4  TFT_MOSI
5  TFT_CS
6  +3V3
7  TFT_LED
8  TFT_SCK
9  GND
10 GND
```

O segundo GND ajuda o retorno do chicote. Em produto, manter cabo curto e preferir GND próximo de SCK/backlight.
