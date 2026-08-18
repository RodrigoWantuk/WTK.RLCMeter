# `tools`

Host-side tools that support firmware, assets, calibration, and diagnostics.

## Planned tools

- PNG/font to RGB565/mask converter;
- asset packer with manifest, offsets, and CRC;
- calibration-table generator/validator;
- UART log parser;
- measurement-session analysis scripts;
- comparison tooling against reference-instrument data;
- known-vector generation for host-side tests.

Tools should be deterministic and should record the format/version used to generate artifacts consumed by firmware.
