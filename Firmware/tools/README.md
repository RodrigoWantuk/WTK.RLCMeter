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
- `calibration_campaign.py`: host-only, standard-library interval-fit prototype for
  supplementary R/C/L standards. Input is a JSON object with `schema_version=1`,
  `hardware_revision=65537`, all 33 `osl_conditions`, and per-condition groups with
  optional `prior_curve` (12 matrix coefficients at 0.1/1/10 times RREF) and
  `standards`. Each standard provides `id`, `capture_id`, `capture_safe=true`,
  `capture_dsp_status=OK`, `capture_calibration_sequence`, `type` (`R`/`C`/`L`),
  `nominal_si`, `tolerance_fraction`, exact frequency/amplitude and datasheet
  frequency, plus OSL-corrected `measured_z_re_ohms`/`measured_z_im_ohms`. Capacitor
  `esr_max_ohms` and `d_max`, and inductor `q_min`, are optional; matching loss
  constraints require calibrated board/datasheet temperatures. A loss field may set
  `<field>_frequency_hz`; out-of-band loss data is reported as ignored, never fitted.
  Run `python tools/calibration_campaign.py --template --out campaign.json` for an
  unverified key skeleton, then run the same tool with `campaign.json` and
  `--out provisional.json` after entering actual evidence. The template is not proof
  that OSL captures occurred.
  The JSON output is **not** a W25Q record, does not verify real OSL evidence, and
  must not be installed as qualified firmware calibration.
- `inspect_calibration_record.py`: decodes the Phase 07 Stage 2A calibration frame,
  verifies CRC/commit state, and prints record keys and correction coefficients.
- `firmware_size.py`: reports STM32 ELF Flash/RAM usage, reserved stack/heap floor,
  largest symbols, optional JSON output, PRODUCT size gates, and the Phase 09A BRINGUP
  physical-MVP size gate.
- `build_resource_pack.py`: builds the deterministic Resource Pack v2/API v4 binary
  from `assets/resource_manifest.json`, including EN/PT-BR text resources and the
  SMALL/MEDIUM/LARGE external A1 font roles plus optional READY art.
- `generate_plex_bitmap_sources.py`: offline Pillow-based rasterization of the pinned
  IBM Plex sources into the three deterministic A1 font source files; normal pack
  builds and firmware do not require Pillow.
- `prepare_rgb565_image_resource.py`: offline PNG-to-RGB565/RLE converter with
  deterministic channel quantization for bounded W25Q command streams.
- `render_product_ui_previews.py`: calls the host-only real PRODUCT renderer for
  EN/PT-BR screens, blank-W25Q recovery, and refresh-equivalence checks; composes
  the reviewed contact sheet with offline Pillow.
- `inspect_resource_pack.py`: validates Resource Pack v2 header, entry table, payload
  CRCs, required EN/PT-BR text resources, dense text IDs, text index CRCs, bounds,
  UTF-8, and font A1 header/index/bitmap semantics before printing a concise summary.
- `resource_pack_format.py`: shared host-side Resource Pack v2 encoder/inspector logic
  used by the builder and Python unit tests.

Tools should be deterministic and should record the format/version used to generate artifacts consumed by firmware.
