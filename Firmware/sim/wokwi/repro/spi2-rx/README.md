# SPI2 RX standalone Wokwi reproducer

This directory is **not** WTK.RLCMeter product firmware. It isolates Wokwi
STM32F103 SPI master receive with the fewest possible lines after Stage 5
observed PB14 carrying `EF 40 17` while `w25q_device_probe()` still reported
`UNSUPPORTED_DEVICE`.

Do not treat a pass or fail here as virtual qualification of the instrument.

## Simulator versions

Record the exact CLI/API versions used when capturing evidence. Stage 4/5:

```text
Wokwi CLI: 0.26.1 (9d71b975b7eb)
Simulation API: 1.0.0-20260803-gf69c6c93
```

## What it configures

```text
MCU:     STM32F103C8 / Blue Pill
Clock:   HSI 8 MHz (reproducer only; production remains HSE/PLL 72 MHz)
SPI2:    PB13 SCK, PB14 MISO, PB15 MOSI, PA12 CS, mode 0, DIV8
SPI1:    PA5 SCK, PA6 MISO, PA7 MOSI, PA4 CS (only if SPI2 RX is all-zero)
Slave:   custom chip, SPI Device API, MISO=INPUT, response A5 5A C3 3C
UART:    USART1 115200 for TX/RX hex dump
```

Expected slave bytes on MISO:

```text
A5 5A C3 3C
```

## Commands

From `Firmware/`:

```text
cmake -S sim/wokwi/repro/spi2-rx -B build/virtual/wokwi/repro/spi2-rx -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-gcc.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build/virtual/wokwi/repro/spi2-rx
wokwi-cli chip compile sim/wokwi/repro/spi2-rx/chips/slave/slave.chip.c -o sim/wokwi/repro/spi2-rx/chips/slave/slave.chip.wasm
wokwi-cli sim/wokwi/repro/spi2-rx --scenario sim/wokwi/repro/spi2-rx/scenario.yaml --timeout 4000 --timeout-exit-code 124 --serial-log-file build/virtual/wokwi/repro/spi2-rx/repro.serial.log --vcd-file build/virtual/wokwi/repro/spi2-rx/repro.vcd
```

The `wokwi-cli` working directory for chip compile should be `Firmware/sim/wokwi` so it can reuse `chips/w25q64/wokwi-api.h`.

## Observed vs expected

Live capture (same CLI/API as Stage 4/5):

```text
Expected RX8: A5 5A C3 3C
Observed SPI2 RX8: 00 00 00 00
Observed SPI2 DR16: 0000 0000 0000 0000
Observed SPI1 RX8: 00 00 00 00
Observed SPI1 DR16: 0000 0000 0000 0000
```

Production Lab ELF + W25Q model (separate `--miso-probe`): PB14 VCD carried `EF 40 17` after command `9F`, while `w25q_device_probe()` still reported `UNSUPPORTED_DEVICE`. That is the line-level SPI2 evidence. This standalone binary proves `SPI_DR` stays zero on SPI2 and SPI1.

## Scope of any simulator bug

```text
MCU: STM32F103C8 (Wokwi Blue Pill)
Peripherals: SPI2 (primary), SPI1 (comparison)
Behavior: SPI master MOSI/SCK/CS work; SPI_DR does not sample MISO
Wokwi CLI: 0.26.1 (9d71b975b7eb)
Simulation API: 1.0.0-20260803-gf69c6c93
```

Do not write "Wokwi does not support SPI RX". Custom-chip MOSI capture works, and production PB14 can show JEDEC bits. The blocker is STM32 SPI master receive into `SPI_DR` under this simulator/API version.
