# `assets`

Source assets for the graphical UI.

## Expected content

- startup/splash artwork;
- icons;
- source fonts for offline conversion;
- auxiliary images;
- source manifests/metadata.

Files in this directory are source inputs. Firmware consumes packed/generated resources produced by tooling under `tools/` and stored in W25Q external Flash.

## Implemented Resource Pack v2 inputs

- `resource_manifest.json` is the deterministic source manifest for the external W25Q
  resource pack.
- `text/en.json` and `text/pt-BR.json` provide UTF-8 product text catalogs with stable
  numeric text IDs. Firmware does not depend on enum ordinal order or physical Flash
  offsets.
- Stage 3A.1 freezes the current text-catalog semantic ABI as dense IDs
  `0x0001..0x0041`, every ID present in English and Portuguese (Brazil), and each
  UTF-8 string no longer than 31 bytes.
- Stage 3B.1 adds a repository-owned deterministic A1 bitmap font source at
  `font/wtk-pixel-base.json`. The builder emits `FONT_UI_SMALL`, `FONT_UI_MEDIUM`,
  and `FONT_UI_LARGE` W25Q font resources from that source.
- Resource API v3 also admits optional `RGB565_IMAGE` entries in
  `IMAGE_RGB565_RLE_V1` format for rich screens, icons, and splash artwork. These
  are 16-bit RGB565 run streams with a CRC-checked image header and command stream,
  designed for chunked W25Q-to-TFT rendering without a full framebuffer. They are
  optional resources; normal boot still requires only the text catalogs and font
  roles. Optional full-screen PRODUCT artwork uses 240x320 portrait resources under
  the `IMAGE_SCREEN_*` IDs and remains independent from emergency rendering.
- `source/screens/*.png` contains AI-generated PRODUCT artwork sources for rich
  screen resources.
- `image/wtk-splash-rle.json` is the compact startup splash currently consumed by
  the PRODUCT startup renderer. It is derived from the AI source artwork by
  `tools/prepare_rgb565_image_resource.py`.
- `image/screen-*-rle.json` contains optional full-screen 240x320 portrait
  RGB565 RLE artwork for future rich PRODUCT screens. These assets are packaged
  into W25Q but normal boot safety does not depend on them.
- The Resource Pack outer schema remains version 2. PRODUCT resource API version is
  now 3 because firmware requires text catalogs plus the three external font roles
  and defines optional external image resources.
- Build the binary pack with:

```bash
python Firmware/tools/build_resource_pack.py Firmware/assets/resource_manifest.json -o Firmware/build/resources/wtk_resources.bin
```

The preferred product-facing PC utility wraps build, inspection, framed-stream
generation, and optional serial upload:

```bash
python Firmware/tools/resource_pack_tool.py build Firmware/assets/resource_manifest.json -o Firmware/build/resources/wtk_resources.wrp2 --summary Firmware/build/resources/wtk_resources.summary.json
python Firmware/tools/resource_pack_tool.py inspect Firmware/build/resources/wtk_resources.wrp2
python Firmware/tools/resource_pack_tool.py frame Firmware/build/resources/wtk_resources.wrp2 -o Firmware/build/resources/wtk_resources.wpc
python Firmware/tools/resource_pack_tool.py upload Firmware/build/resources/wtk_resources.wrp2 --port COM5
```

Convert an AI-generated PNG source into a firmware image resource with:

```bash
python Firmware/tools/prepare_rgb565_image_resource.py Firmware/assets/source/screens/boot-splash-ai.png -o Firmware/assets/image/wtk-splash-rle.json --width 64 --height 32
```

The upload subcommand expects a service/manufacturing firmware image built with
`WTK_ENABLE_PRODUCT_RESOURCE_UPDATE=ON`. Normal product measurement, calibration, and
menus remain menu-driven and do not depend on UART.

Generated pack binaries are build artifacts; the checked-in source of truth is the
manifest plus JSON catalog input files.

Authoring fonts such as TTF/OTF are never parsed by STM32 firmware. Development-host
tooling converts them into compact MCU-oriented font resources containing rasterized
glyph data, glyph metrics, supported symbols, and optional simple compression. Stage
3B.1 uses a project-owned JSON pixel source as a provisional license-clean font, not
the final product typography.

## Rules

- do not assume a full framebuffer;
- prefer source formats that convert cleanly to RGB565 or compact masks;
- keep installed font/resource size independent from SRAM use by designing for chunked W25Q reads;
- preserve licensing/source information for third-party assets;
- assign stable asset IDs so UI code does not depend on physical Flash offsets;
- keep a tiny internal-Flash emergency fallback font for basic diagnostic/safety messages.
