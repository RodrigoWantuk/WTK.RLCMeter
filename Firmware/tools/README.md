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
  `standards`. For a real BRINGUP capture, each standard provides `id`,
  `capture_file`, the SHA-256 `capture_id` printed by `pc_capture.py`, `type`
  (`R`/`C`/`L`), `nominal_si`, `tolerance_fraction`, and the datasheet frequency.
  The tool derives the exact range/frequency/amplitude, active-calibration sequence,
  and OSL-processed impedance printed in that completed capture. Capacitor
  `esr_max_ohms` and `d_max`, and inductor `q_min`, are optional; matching loss
  constraints require calibrated board/datasheet temperatures. A loss field may set
  `<field>_frequency_hz`; out-of-band loss data is reported as ignored, never fitted.
  Run `python tools/calibration_campaign.py --template --out campaign.json` for an
  unverified key skeleton. With actual captures, run
  `python tools/calibration_campaign.py campaign.json --capture-root captures --out provisional.json`.
  Use `--synthetic-unbound` instead of `--capture-root` only for synthetic development
  inputs. All standards in one campaign must name the same active OSL sequence.
  The 33 OSL keys remain an unverified skeleton, not proof of a completed OSL campaign.
  The JSON output is **not** a W25Q record, does not independently verify OSL evidence, and
  must not be installed as qualified firmware calibration.
- `pc_capture.py`: passive COM collector for one successful BRINGUP `RAW v1`
  DUT-measurement dump. Run `python tools/pc_capture.py --port COM5 --out raw.txt`,
  then initiate the existing BRINGUP DUT capture separately. The collector sends no
  command to the instrument, verifies 256 complete six-channel ADC rows and permit/
  relay metadata, DSP framing, sample cadence and active-calibration sequence, and
  refuses failed or non-DUT dumps. It writes the exact ASCII bytes it hashes, including
  on Windows. It does not prove electrical safety independently; the BRINGUP dump's
  printed OSL result is bound to the raw file, not independently recalculated by this tool.
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
