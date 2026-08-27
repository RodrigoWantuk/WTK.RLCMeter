# `tools`

Host-side tools that support firmware, assets, calibration, and diagnostics.

## Planned tools

- PNG/font to RGB565/mask converter;
- asset packer with manifest, offsets, and CRC;
- UART log parser;
- measurement-session analysis scripts;
- comparison tooling against reference-instrument data;
- known-vector generation for host-side tests.

## Implemented tools

- `reference_impedance.py`: independent double-precision synthetic impedance and raw
  replay/reference helper for Phase 06 tests.
- `inspect_calibration_record.py`: decodes the Phase 07 Stage 2A calibration frame,
  verifies CRC/commit state, and prints record keys and correction coefficients.
- `firmware_size.py`: reports STM32 ELF Flash/RAM usage, reserved stack/heap floor,
  largest symbols, optional JSON output, and Release/Bringup size-budget gates.

Tools should be deterministic and should record the format/version used to generate artifacts consumed by firmware.
