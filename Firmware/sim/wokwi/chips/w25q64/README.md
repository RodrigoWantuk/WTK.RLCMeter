# W25Q64 Custom Chip

This is a deliberately small Wokwi model for Phase 03A virtual-hardware regression.

It models only the command subset currently used by the production firmware:

- `0x9F` Read JEDEC ID;
- `0x05` Read Status Register 1;
- `0x06` Write Enable;
- `0x03` Read Data;
- `0x0B` Fast Read;
- `0x02` Page Program;
- `0x20` 4 KiB Sector Erase.

The simulated JEDEC ID defaults to `EF 40 17` for the W25Q64 family. The model accepts the full 24-bit address space, returns erased `0xFF` for unimplemented addresses, and implements the final reserved 4 KiB bring-up sector as writable storage. This keeps the model small without aliasing unrelated addresses.

The custom chip follows the Wokwi SPI device API pattern: `CS` starts/stops the SPI interface with `spi_start()`/`spi_stop()`, and `MISO` is initialized as `INPUT` rather than `OUTPUT_HIGH`. The API transfers response bytes through the SPI buffer while selected, so the model must not present the shared Rev.1 MISO net as a permanently driven high GPIO when W25Q `CS` is inactive.

While `BUSY` is set, the model keeps `0x05` status reads available and deterministically ignores mutating commands such as write-enable, page-program, and sector-erase until the busy timer clears. Accepted program/erase commands require `WEL`, a complete 24-bit address, and the correct transaction boundary; accepted mutations clear `WEL` and start a nonzero busy interval.

The Phase 03 production driver currently reports no-response/all-`0xFF`, invalid JEDEC, and unsupported valid-looking JEDEC through the same `W25Q_STATUS_UNSUPPORTED_DEVICE` boot diagnostic. The virtual scenarios keep that diagnostic wording while distinguishing the injected conditions in their diagram attributes.

Generated `.chip.wasm` files are build artifacts created by:

```bash
wokwi-cli chip compile chips/w25q64/w25q64.chip.c -o ../../build/virtual/wokwi/chips/w25q64/w25q64.chip.wasm
```

The source C and `.chip.json` are the canonical files.
