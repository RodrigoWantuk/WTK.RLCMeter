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
  `esr_max_ohms` and `d_max`, and inductor `q_min`, are optional. Capacitor
  `esr_nominal_ohms`/`esr_tolerance_fraction` and
  `d_nominal`/`d_tolerance_fraction` are also optional independent pairs; a
  nominal loss value without its explicit nonzero tolerance is rejected.
  Matching loss
  constraints require calibrated board/datasheet temperatures. A loss field may set
  `<field>_frequency_hz`; out-of-band loss data is reported as ignored, never fitted.
  Standards default to `role: "FIT"`. A held-out `role: "VALIDATION"` standard
  has the same capture/datasheet requirements but does not alter the curve; it
  checks the solved curve against its interval afterward. The report gives each
  condition's `constraint_rank` (out of 12), fit/validation counts, validation
  failures, and conservative coverage. Rank 12 does not prove coefficient
  uncertainty or hardware accuracy. A failed held-out interval yields
  `HOST_VALIDATION_FAILED` and a nonzero CLI exit, never an installable record.
  Run `python tools/calibration_campaign.py --template --out campaign.json` for an
  unverified key skeleton. With actual captures, run
  `python tools/calibration_campaign.py campaign.json --capture-root captures --osl-frame active-cal.bin --out provisional.json`.
  `active-cal.bin` must contain one committed Rev.1 calibration frame or complete
  4096-byte slot image from the active set. The PC checks the frame CRC, 33 unique
  supported keys, OSL coefficients and sequence against every RAW capture.
  On BRINGUP, run `python tools/pc_cal_frame.py --port COM5 --out active-cal.bin`
  to request `lab cal frame` and save a validated canonical serialization of the
  active in-RAM set. It streams one 16-byte hex line per cooperative step using
  the existing shared workspace, without reading or mutating W25Q.
  Use `--synthetic-unbound` instead of `--capture-root` only for synthetic development
  inputs. All standards in one campaign must name the same active OSL sequence.
  The export is BRINGUP-only and is not a byte-exact readback of the physical slot.
  Matching CRC and sequence do not authenticate the device or prove the physical
  OPEN/SHORT/LOAD fixtures. The JSON output is **not** a W25Q record and
  must not be installed as qualified firmware calibration.
- `pc_campaign_add.py`: append one completed BRINGUP RAW capture to a host-only
  campaign without hand-editing the condition JSON. Start with
  `python tools/calibration_campaign.py --template --out campaign.json`, then use
  `python tools/pc_campaign_add.py campaign.json --capture-root captures --osl-frame active-cal.bin --capture resistor-001.raw --id R-001 --type R --nominal-si 1000 --tolerance-fraction 0.01`.
  The command derives the condition, SHA-256 capture ID, and active OSL sequence
  from the checked evidence; it rebinds and solves the entire candidate campaign
  before atomically replacing the JSON. `--role VALIDATION` adds a held-out
  standard, not a fit constraint. Optional `--esr-max-ohms`,
  `--esr-nominal-ohms` with `--esr-tolerance-fraction`, `--d-max`,
  `--d-nominal` with `--d-tolerance-fraction`, and `--q-min` accept only the
  component type and condition supported by the solver. Matching loss data
  additionally require `--calibrated-board-temp-c` and
  `--datasheet-temp-c`; this user-declared temperature is not authenticated by
  the RAW dump. The tool does not command measurement hardware, install a
  correction, or qualify accuracy. An incompatible FIT capture leaves the
  previous campaign file unchanged; a failed held-out check is recorded and
  returns a nonzero status for review.
- `pc_capture.py`: passive COM collector for one successful BRINGUP `RAW v1`
  DUT-measurement dump. Run `python tools/pc_capture.py --port COM5 --out raw.txt`,
  then initiate the existing BRINGUP DUT capture separately. The collector sends no
  command to the instrument, verifies 256 complete six-channel ADC rows and permit/
  relay metadata, DSP framing, sample cadence and active-calibration sequence, and
  refuses failed or non-DUT dumps. It writes the exact ASCII bytes it hashes, including
  on Windows. It does not prove electrical safety independently; the BRINGUP dump's
  printed OSL result is bound to the raw file, not independently recalculated by this tool.
- `pc_dc_pilot.py`: explicitly requests one experimental BRINGUP 1 MOhm pilot
  through COM. Run only on a current-limited, controlled bench with
  `python tools/pc_dc_pilot.py --port COM5 --out pilot.txt --ack-current-limited-bench`.
  It verifies AC screen, fresh residual qualification, DC result, and SAFE
  teardown markers, then saves exact ASCII evidence and a SHA-256 identity.
  The result is exploratory; ideal ADC scaling is marked `IDEAL_UNQUALIFIED`.
  It does not calibrate DCR, authorize lower RREF, or claim insulation resistance.
- `pc_cal_frame.py`: read-only BRINGUP COM collector for the currently active OSL
  set. It sends `lab cal frame`, reconstructs the bounded hex stream, validates
  commit/CRC and all 33 Rev.1 condition records, and writes a binary frame only
  on complete success. It does not install calibration or authorize measurement.
- `inspect_calibration_record.py`: decodes the Phase 07 Stage 2A calibration frame,
  verifies CRC/commit state, and prints record keys and correction coefficients.
  Its `decode_full_rev1_frame()` helper additionally validates the complete
  33-condition active-set shape for host campaign binding.
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
