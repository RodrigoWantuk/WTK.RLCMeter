# `storage`

Persistence layer over external W25Q SPI Flash.

## Planned modules

```text
record_store.c/.h
asset_store.c/.h
settings_store.c/.h
```

## Implemented modules

```text
resource_store.c/.h
storage_crc32.c/.h
storage_layout.c/.h
```

Calibration-specific serialization and the redundant two-slot store live under
`Firmware/src/measurement/` so the storage layer does not need to understand impedance
math. The store uses the logical partitions defined here through abstract read/erase/
program callbacks.

## Baseline

No filesystem is planned initially. Flash is divided into logical regions for resource/assets, settings, calibration, and optional diagnostics.

Every persistent record must contain identification, version, bounds information, and CRC. Settings/calibration should use redundant slots or a small journal so interrupted writes do not destroy the last valid record.

W25Q is the instrument's external resource/data ROM. Large UI fonts, numeric glyphs, measurement symbols, icons, and bitmap resources stay in W25Q and are read in bounded chunks through stable resource IDs. STM32 internal Flash stores only rendering/decoding code and a tiny emergency fallback font.

Phase 03 introduces a provisional resource-pack header/entry contract for manifest bounds checking and stable ID/offset addressing. It is deliberately not a final asset-pack format.

Phase 07 Stage 2A defines the W25Q mutable tail:

```text
calibration slot A  4096 bytes
calibration slot B  4096 bytes
settings            4096 bytes
diagnostics         16384 bytes
bring-up test       final 4096-byte sector
```

The resource pack occupies the lower address range before that mutable tail.

Because W25Q and ILI9341 share SPI, storage/resource access must release Flash CS before TFT transfer begins. A small fixed scratch buffer is shared by resource streaming; installed font/resource size must not scale SRAM usage.

## Rules

- never trust Flash contents without validation;
- calibration schema carries `hardware_revision`;
- incompatible versions are rejected or migrated explicitly;
- erase/program does not occur during critical acquisition;
- Flash size/address assumptions stay inside the driver/layout layers;
- UI accesses assets by stable IDs, never pointer-like physical assumptions.
- STM32 firmware does not parse TTF/OTF and does not embed FreeType.
