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

RGB565 pixels are converted to the ILI9341 wire order explicitly: high byte first, then low byte. The driver does not reinterpret little-endian `uint16_t` memory as an SPI byte stream.

It does not know about screens, units, navigation, or RLC results.

## W25Q

The driver exposes JEDEC ID, normal/fast read, status, page-program start, sector-erase start, and pollable completion.

Compatible W25Q16/32/64/128 parts are decoded through an explicit JEDEC table containing manufacturer, memory type, and capacity. Erase/program operations are intentionally stateful (`start` then `poll`) so the cooperative superloop can continue servicing the watchdog; the low-level Flash driver does not service the application watchdog by itself.

## Shared SPI

TFT and Flash share SCK/MOSI/MISO and use independent chip selects.

Invariants:

- never select both devices at the same time;
- avoid long transactions during quiet mode;
- TFT updates are incremental;
- Flash erase/program does not occur during critical acquisition.

The bus abstraction centrally owns chip-select exclusion and observes the BSP quiet-mode request bit. A quiet-mode block is reported as `BSP_STATUS_BUSY`, not as a hard transaction error. Resource rendering can alternate:

```text
select W25Q -> read bounded chunk -> release W25Q
select TFT  -> transmit/render chunk -> release TFT
```

## Buttons

Debouncing converts GPIO states into `PRESS`, `RELEASE`, `LONG_PRESS`, and `REPEAT` events without embedding UI navigation policy.

Host tests cover debounce, bounce rejection, long-press boundaries, repeat behavior, and deterministic simultaneous-button ordering.
