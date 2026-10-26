# Product UI resources

`resource_manifest.json` is the source of truth for the W25Q product pack. The outer
wire schema is v2 and the product resource API is v4. API v4 requires complete EN and
PT-BR UTF-8 catalogs (`0x0001..0x0059`) and three A1 bitmap font roles. An older API
v3 pack must be rebuilt and uploaded; the firmware must not silently reinterpret it.

The current seven-entry pack includes optional 240x320 RGB565 RLE art for the READY
screen and a separate personalized three-second boot splash (`0x00030006`).
The READY artwork contains no words or numerical readouts. The firmware composes localized text,
values, status, and icon glyphs over it. Corrupt or absent optional art falls back to
the plain background; missing/corrupt required text or fonts enters the internal
bilingual PC-link recovery screen. The small internal emergency font is independent
of W25Q and never grants measurement permission.

## Sources and regeneration

- `text/en.json`, `text/pt-BR.json`: stable localized text IDs, at most 31 UTF-8 bytes
  each. Change both catalogs together.
- `source/fonts/IBMPlexSans-Regular.ttf` and `IBMPlexMono-Regular.ttf`: IBM Plex,
  pinned upstream revision `763c36ef9117782905ae010056dfbe8fd2653a25`.
  `source/fonts/LICENSE.txt` contains the SIL Open Font License. The MCU never parses
  TTF. `font/plex-*-a1.json` are deterministic monochrome raster outputs, including
  eight private-use menu icons. Small/medium use Sans at 12/16 px; large numerics use
  Mono at 28 px.
- `source/screens/boot-art-v2.png`: wordless generated bitmap source. The derived
  `image/boot-art-v2-rle.json` is 240x320 with three significant bits per RGB channel
  before RGB565 encoding, keeping the compressed command stream bounded. The other
  screen image sources are historical experiments and are not packaged.
- `source/screens/wtk-rlcmeter-boot-splash-rodrigo-wantuk-240x320.png`: owner-supplied
  indexed PNG. `image/personalized-boot-splash-rle.json` preserves native RGB565
  precision (`--channel-bits 8`). Every embedded profile streams this optional
  splash, holds the completed image for 3000 ms, then clears it.
- `font/wtk-pixel-base.json`: previous provisional source, retained for legacy
  synthetic format tests; it is not in the product pack.

Regenerate the font JSON after any catalog or font-source change (Pillow is required
only for this offline step):

```sh
python Firmware/tools/generate_plex_bitmap_sources.py
```

Regenerate the optional art when its PNG source changes:

```sh
python Firmware/tools/prepare_rgb565_image_resource.py Firmware/assets/source/screens/boot-art-v2.png -o Firmware/assets/image/boot-art-v2-rle.json --width 240 --height 320 --channel-bits 3
```

Regenerate the personalized splash without recoloring it (8-bit indexed, RGB and
RGBA non-interlaced PNGs are supported):

```sh
python Firmware/tools/prepare_rgb565_image_resource.py Firmware/assets/source/screens/wtk-rlcmeter-boot-splash-rodrigo-wantuk-240x320.png -o Firmware/assets/image/personalized-boot-splash-rle.json --width 240 --height 320 --channel-bits 8 --description "Owner supplied personalized boot splash; native RGB565 precision"
```

Build, inspect, frame, and upload the pack from the repository root:

```sh
python Firmware/tools/resource_pack_tool.py bundle Firmware/assets/resource_manifest.json -o Firmware/build/resources/product.wrp2 --summary Firmware/build/resources/product.json --stream Firmware/build/resources/product.wpc
python Firmware/tools/resource_pack_tool.py inspect Firmware/build/resources/product.wrp2
python Firmware/tools/resource_pack_tool.py upload Firmware/build/resources/product.wrp2 --port COM5 --report Firmware/build/resources/upload-001.json
```

The last command uses the product PC-link resource receiver through a serial COM
port. It is for provisioning/recovery; ordinary menus, measurement, and calibration
do not require UART. Never put a resource pack inside the internal MCU Flash image.

The supported uploader opens with DTR/RTS inactive, flushes each byte, defaults to
10 ms byte pacing for BEGIN, 2 ms for subsequent frames and 250 ms post-BEGIN
settling, and validates every ACK including
END/COMPLETE. It stops on any rejection, malformed response or timeout without
replaying frames. A saved report records partial failures; inspect the target
before restarting a transfer. An ACK is not an independent physical readback.
Back up the complete sector-rounded erase span and the capacity-relative mutable
32 KiB tail before replacement; compare the complete installed pack and tail after.

A receiver-enabled PRODUCT accepts RESOURCE_BEGIN from idle READY,
CALIBRATION_REQUIRED, Maintenance or PC-link presentation only, with inactive
runtime/teardown and no pending settings save. The shell additionally requires
K1 SAFE, range and excitation off, no quiet window or safety fault; normal Flash
ownership checks remain mandatory. This permits asset replacement without a
connected display. Active measurement, calibration, faults and unsafe states do
not grant host maintenance entry. No calibration permission is granted by upload.
TFT redraw is deferred while the receiver owns Flash, keeping polled UART live.
Wait for the shared boot presentation to finish (up to 33 s including bounded
progress/clear time) before initiating any serial session; do not send during boot.

The older receiver used for the [first-board installation](../../docs/review/bringup-2026-10-24/Resource-Install.md)
required `--inter-byte-delay-ms 10 --settle-after-begin-ms 10000`. Its 193 ACKs and
22891-byte readback qualify that older pack only. The new pack is 104499 bytes,
uses 26 sectors (`[0,0x1A000)`), and received independent complete readback
acceptance in the [splash delivery report](../../docs/review/bringup-2026-10-25/Boot-Splash.md).
The mutable 32 KiB tail was preserved; actual TFT appearance is still pending.

## Preview and validation

The host-only `wtk_ui_product_preview` target links the real product renderer and
reads the built pack. It renders 240x320 PPMs with a mock TFT. Generate all EN/PT-BR
screens, both blank-W25Q recovery screens, and the reviewed contact sheet with:

```sh
cmake --preset host-debug
cmake --build --preset host-debug --target wtk_ui_product_preview
python Firmware/tools/render_product_ui_previews.py --preview-exe Firmware/build/host-debug/tests/Debug/wtk_ui_product_preview.exe --output-dir Firmware/build/ui-preview --contact-sheet Firmware/renders/product-ui-en-pt.png
```

The script builds the pack from the manifest and writes primary, secondary
submenu/wizard, and blank-W25Q recovery contact sheets under `Firmware/renders/`.
Pass `-` instead of a pack path when
calling the preview executable directly to render the internal recovery screen without
W25Q. The script verifies that `result-refresh` and `result-updated` produce identical
pixels, proving that an updated result clears its old footer. The reviewed EN/PT-BR
contact sheet is `Firmware/renders/product-ui-en-pt.png`. The contact-sheet step
requires Pillow offline; the firmware and normal pack builder do not.

The resource builder verifies IDs, UTF-8 bounds, required glyph coverage, payload
CRCs, and image run coverage. Host tests additionally check menu widths and
quantization. Physical TFT appearance, SPI timing, and readability still require
bench validation.

## Runtime constraints

- No full framebuffer or heap on the MCU. W25Q image commands and glyphs are streamed
  through bounded buffers; pack size does not scale SRAM usage.
- Text and images defer while the shared Flash bus is unavailable or quiet is active.
- The pack is an output artifact; manifest, source JSON, font TTF/license, and source
  PNG are versioned. Do not edit generated font or RLE JSON by hand.
- Internal emergency/recovery text remains deliberately small and independent of the
  installed language pack.
