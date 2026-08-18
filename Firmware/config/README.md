# `config`

Versioned build and firmware defaults.

## Planned content

- laboratory feature flags;
- UI defaults;
- logging defaults;
- hardware-revision identity;
- non-metrology defaults required even when external Flash is invalid.

Measured calibration data does not belong here as hard-coded constants; it belongs in versioned persistent calibration records.

No normal configuration option may disable mandatory safety interlocks.
