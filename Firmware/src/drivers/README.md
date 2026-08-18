# `drivers`

Device drivers and small peripheral abstractions.

## Baseline

```text
ili9341.c/.h
w25q.c/.h
buttons.c/.h
spi_bus.c/.h       # if shared-bus ownership needs an explicit layer
crc32.c/.h         # location may change if a common utility module is added
```

## ILI9341

The driver exposes low-level display operations such as init, reset, ID/status readback, rotation, address window, fill, and RGB565 pixel transfer.

It does not know about screens, units, navigation, or RLC results.

## W25Q

The driver exposes JEDEC ID, normal/fast read, page program, sector erase, status, and wait-ready operations.

It should recognize compatible W25Q16/32/64/128 devices instead of assuming W25Q64 capacity everywhere.

## Shared SPI

TFT and Flash share SCK/MOSI/MISO and use independent chip selects.

Invariants:

- never select both devices at the same time;
- avoid long transactions during quiet mode;
- TFT updates are incremental;
- Flash erase/program does not occur during critical acquisition.

## Buttons

Debouncing converts GPIO states into `PRESS`, `RELEASE`, `LONG_PRESS`, and `REPEAT` events without embedding UI navigation policy.
