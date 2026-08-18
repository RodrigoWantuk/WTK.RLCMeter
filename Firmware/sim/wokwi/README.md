# Wokwi Virtual Hardware

This directory contains the Phase 03A Stage 1 Wokwi project for WTK.RLCMeter.

The simulation loads the normal STM32 Lab artifact:

```bash
cd Firmware
cmake --preset stm32-lab
cmake --build --preset stm32-lab
python tools/run_virtual_tests.py --smoke
```

Use `python tools/run_virtual_tests.py --build --smoke` to configure/build the Lab ELF before running the short suite.

## Prerequisites

- `wokwi-cli` in `PATH`;
- `WOKWI_CLI_TOKEN` set in the environment;
- `Firmware/build/stm32-lab/WTK.RLCMeter.elf` built from the repository CMake flow.

The runner checks that the token is present without printing it. It does not download or execute installer scripts. Official installation options are documented by Wokwi:

- <https://docs.wokwi.com/wokwi-ci/cli-installation>
- <https://docs.wokwi.com/wokwi-ci/cli-usage>

## Circuit Model

The Wokwi model uses `board-stm32-bluepill` plus:

- ILI9341 on the Rev.1 SPI2/control pins;
- three active-low pushbuttons on PB3, PB4, and PC13;
- three 8-channel logic analyzers for safe outputs, SPI/display/range pins, and UART/button/SWD observation.

The Stage 1 model deliberately does not add a fake W25Q device. The missing-Flash path is expected and is used to smoke the degraded display/resource path without claiming Flash validation.

PA13/PA14 remain SWD pins. Native USB is not modeled as a product interface because Rev.1 assigns PA11/PA12 to board functions.

## Scenarios

- `boot-safe`: UART identity plus safe boot pin assertions.
- `uart-boot`: stable Lab boot diagnostics.
- `buttons`: UP/DOWN/OK press/release and OK long press via UART diagnostics.
- `pwm-backlight`: PB0 PWM capture for VCD post-processing.
- `spi-display`: ILI9341 transaction smoke with W25Q absent.
- `spi-cs`: VCD post-processing that rejects simultaneous TFT/Flash chip-select assertion.

Artifacts are written under `Firmware/build/virtual/wokwi/`.

## Limits

Wokwi is virtual regression evidence only. It does not replace bench validation for electrical behavior, relay safety, analog residual-voltage thresholds, ADC/DMA metrology, signal integrity, watchdog reset behavior, or final display quality.

Current Wokwi STM32F103 support includes GPIO, USART, SPI, TIM1-4, RCC, and AFIO. ADC2, DMA, and IWDG are not available for the current Phase 03A evidence set.
