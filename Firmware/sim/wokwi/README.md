# Wokwi Virtual Hardware

This directory contains the Phase 03A Wokwi project for WTK.RLCMeter.

The simulation loads the normal STM32 Lab artifact:

```bash
cd Firmware
cmake --preset stm32-lab
cmake --build --preset stm32-lab
python tools/run_virtual_tests.py --smoke
```

Use `python tools/run_virtual_tests.py --build --smoke` to configure/build the Lab ELF before running the short suite.
Use `python tools/run_virtual_tests.py --lint-only` to compile custom chips and run `wokwi-cli lint` without consuming simulation time.

## Prerequisites

- `wokwi-cli` in `PATH`;
- `WOKWI_CLI_TOKEN` set in the environment;
- `Firmware/build/stm32-lab/WTK.RLCMeter.elf` built from the repository CMake flow.

The runner checks that the token is present without printing it. It does not download or execute installer scripts. It compiles the W25Q64 custom chip into `Firmware/build/virtual/wokwi/chips/w25q64/` before lint/simulation when `wokwi-cli` is available. Official installation options are documented by Wokwi:

- <https://docs.wokwi.com/wokwi-ci/cli-installation>
- <https://docs.wokwi.com/wokwi-ci/cli-usage>
- <https://docs.wokwi.com/guides/custom-chips-to-wasm>
- <https://docs.wokwi.com/chips-api/spi>

## Circuit Model

The Wokwi model uses `board-stm32-bluepill` plus:

- ILI9341 on the Rev.1 SPI2/control pins;
- a minimal custom W25Q64 model sharing SPI2 with the TFT and using PA12 as `FLASH_CS`;
- three active-low pushbuttons on PB3, PB4, and PC13;
- three 8-channel logic analyzers for safe outputs, SPI/display/range pins, and UART/button/SWD observation.

The W25Q64 model implements only the command subset used by the firmware and a writable final reserved 4 KiB test sector. It is not a complete Winbond datasheet model.

PA13/PA14 remain SWD pins. Native USB is not modeled as a product interface because Rev.1 assigns PA11/PA12 to board functions.

## Scenarios

- `boot-safe`: UART identity plus safe boot pin assertions.
- `uart-boot`: stable Lab boot diagnostics.
- `buttons`: UP/DOWN/OK press/release and OK long press via UART diagnostics.
- `pwm-backlight`: PB0 PWM capture for VCD post-processing.
- `spi-display`: ILI9341 transaction smoke with W25Q present.
- `spi-cs`: VCD post-processing that rejects simultaneous TFT/Flash chip-select assertion.
- `w25q-detect`: W25Q64 JEDEC/capacity boot diagnostics.
- `w25q-selftest`: Lab-only non-blocking reserved-sector erase/program/readback test.
- `w25q-bad-jedec`: generated diagram variant with unsupported JEDEC.
- `w25q-absent`: generated diagram variant with no Flash response.
- `quiet-mode`: Lab serial commands exercise buzzer muting and quiet-mode denial of new W25Q work.

Lab-only serial commands used by scenarios:

```text
lab quiet on
lab quiet off
lab buzzer <frequency_hz> <duration_ms>
lab flash info
lab flash selftest
```

Artifacts are written under `Firmware/build/virtual/wokwi/`.

## Limits

Wokwi is virtual regression evidence only. It does not replace bench validation for electrical behavior, relay safety, analog residual-voltage thresholds, ADC/DMA metrology, signal integrity, watchdog reset behavior, or final display quality.

Current Wokwi STM32F103 support includes GPIO, USART, SPI, TIM1-4, RCC, and AFIO. ADC2, DMA, and IWDG are not available for the current Phase 03A evidence set.
