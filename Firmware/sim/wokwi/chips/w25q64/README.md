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

Generated `.chip.wasm` files are build artifacts created by:

```bash
wokwi-cli chip compile chips/w25q64/w25q64.chip.c -o ../../build/virtual/wokwi/chips/w25q64/w25q64.chip.wasm
```

The source C and `.chip.json` are the canonical files.
