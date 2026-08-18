# `storage`

Persistence layer over external W25Q SPI Flash.

## Planned modules

```text
storage_layout.c/.h
record_store.c/.h
asset_store.c/.h
settings_store.c/.h
calibration_store.c/.h
```

## Baseline

No filesystem is planned initially. Flash is divided into logical regions for assets, settings, calibration, and optional diagnostics.

Every persistent record must contain identification, version, bounds information, and CRC. Settings/calibration should use redundant slots or a small journal so interrupted writes do not destroy the last valid record.

## Rules

- never trust Flash contents without validation;
- calibration schema carries `hardware_revision`;
- incompatible versions are rejected or migrated explicitly;
- erase/program does not occur during critical acquisition;
- Flash size/address assumptions stay inside the driver/layout layers;
- UI accesses assets by stable IDs, never raw physical offsets.
