# Wokwi Virtual Hardware

This directory contains the Phase 03A Wokwi project for WTK.RLCMeter.

The simulation loads the normal STM32 Bringup artifact:

```bash
cd Firmware
cmake --preset stm32-bringup
cmake --build --preset stm32-bringup
python tools/run_virtual_tests.py --smoke
```

Use `python tools/run_virtual_tests.py --build --smoke` to configure/build the Bringup ELF before running the short suite.
Use `python tools/run_virtual_tests.py --lint-only` to compile custom chips and run `wokwi-cli lint` without consuming simulation time.
Use `python tools/run_virtual_tests.py --uart-probe` to record PA9 VCD activity and Serial Monitor capture without adding that probe to the acceptance suite.
Use `python tools/run_virtual_tests.py --miso-probe` to promote `logic-spi` first and capture SPI2 SCK/MISO/MOSI/FLASH_CS for JEDEC line-level diagnosis.

USART1 must stay wired to the Wokwi Serial Monitor as well as to `logic-io`:

```text
mcu:A9  -> $serialMonitor:RX
$serialMonitor:TX -> mcu:A10
mcu:A9  -> logic-io:D0
```

`run_virtual_tests.py` fails the static check if either monitor connection is missing. `wokwi-cli` 0.26.1 scenario `expect-pin` steps must use `value:`, not `expected:`.

## Prerequisites

- `wokwi-cli` in `PATH`;
- `WOKWI_CLI_TOKEN` set in the environment;
- `Firmware/build/stm32-bringup/WTK.RLCMeter.elf` built from the repository CMake flow.

The runner compiles the W25Q64 custom chip into `Firmware/build/virtual/wokwi/chips/w25q64/` and copies the WASM next to `Firmware/sim/wokwi/chips/w25q64/` (`*.chip.wasm` is gitignored) so `wokwi.toml` can load `chips/w25q64/w25q64.chip.wasm`. The runner checks that `WOKWI_CLI_TOKEN` is present without printing it. Official installation options are documented by Wokwi:

- <https://docs.wokwi.com/wokwi-ci/cli-installation>
- <https://docs.wokwi.com/wokwi-ci/cli-usage>
- <https://docs.wokwi.com/guides/custom-chips-to-wasm>
- <https://docs.wokwi.com/chips-api/spi>

## Circuit Model

The Wokwi model uses `board-stm32-bluepill` plus:

- ILI9341 on the Rev.1 SPI2/control pins. Firmware never reads the TFT, so `tft:MISO` is left disconnected in the virtual diagram (the Wokwi ILI9341 model can hold MISO);
- a minimal custom W25Q64 model sharing SPI2 MOSI/SCK with the TFT and using PA12 as `FLASH_CS` and PB14 as Flash MISO;
- three active-low pushbuttons on PB3, PB4, and PC13;
- three 8-channel logic analyzers for safe outputs, SPI/display/range pins, and UART/button observation.

`wokwi-cli` 0.26.1 validates the Blue Pill virtual board pins as `A8`, `B12`, `C13`, etc. rather than the STM32 package-style `PA8`, `PB12`, `PC13` names used in the hardware documentation. The diagram and scenario `expect-pin` entries therefore use Wokwi's connector names while comments, diagnostics, and repository documentation keep the Rev.1 net names. The current logic analyzer part also rejects the old `channelNames` attribute, so VCD post-processing maps analyzer `D0..D7` channels back to canonical firmware net names in `tools/run_virtual_tests.py`.

The W25Q64 model implements only the command subset used by the firmware and a writable final reserved 4 KiB test sector. It is not a complete Winbond datasheet model.

PA13/PA14 remain SWD pins in firmware. The current Wokwi Blue Pill part does not expose PA13/PA14 as connectable pins, so SWD preservation is checked through the boot diagnostic rather than a logic-analyzer channel. Native USB is not modeled as a product interface because Rev.1 assigns PA11/PA12 to board functions.

## Scenarios

- `boot-safe`: UART identity plus safe boot pin assertions.
- `uart-boot`: stable Bringup boot diagnostics.
- `buttons`: UP/DOWN/OK press/release and OK long press via UART diagnostics.
- `pwm-backlight`: PB0 PWM capture for VCD post-processing.
- `spi-display`: ILI9341 transaction smoke with W25Q present.
- `spi-cs`: VCD post-processing that rejects simultaneous TFT/Flash chip-select assertion.
- `w25q-detect`: W25Q64 JEDEC/capacity boot diagnostics.
- `w25q-selftest`: Bringup-only non-blocking reserved-sector erase/program/readback test.
- `w25q-bad-jedec`: generated diagram variant with unsupported JEDEC.
- `w25q-absent`: generated diagram variant with no Flash response.
- `quiet-mode`: bring-up serial commands exercise buzzer muting and quiet-mode denial of new W25Q work.

Bringup-only serial commands used by scenarios:

```text
lab quiet on
lab quiet off
lab buzzer <frequency_hz> <duration_ms>
lab flash info
lab flash selftest
lab range 10r
lab range 100r
lab range 1k
lab range 10k
lab range 100k
lab range 1m
lab range off
lab range status
lab safety status
lab charger status
lab sensors status
lab adc status
lab fault status
lab permit status
```

Artifacts are written under `Firmware/build/virtual/wokwi/`.

## Limits

Wokwi is virtual regression evidence only. It does not replace bench validation for electrical behavior, relay safety, analog residual-voltage thresholds, ADC/DMA metrology, signal integrity, watchdog reset behavior, or final display quality.

Current Wokwi STM32F103 support includes GPIO, USART, SPI transmit, TIM1-4, RCC, and AFIO. ADC2, DMA, and IWDG are not implemented.

`EXTERNAL_SIMULATOR_BLOCKER` for this project: STM32 SPI **master receive into `SPI_DR`** does not sample MISO under Wokwi CLI 0.26.1 / API `1.0.0-20260803-gf69c6c93`. PB14 can still show JEDEC bits in VCD; the MCU reads zeros. SPI1 in the standalone reproducer also returns zeros. See `repro/spi2-rx/`. W25Q content scenarios therefore cannot be classified `VIRTUAL_HARDWARE_TESTED`. HSE/PLL is not ready in the model; production firmware fail-closes to HSI 8 MHz and reports `clock_status: TIMEOUT`. IWDG is unimplemented but does not block the boot banner (watchdog starts after UART). `wokwi-cli --vcd-file` records only the first logic analyzer.
