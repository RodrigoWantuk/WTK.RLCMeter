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
- Build the binary pack with:

```bash
python Firmware/tools/build_resource_pack.py Firmware/assets/resource_manifest.json -o Firmware/build/resources/wtk_resources.bin
```

Generated pack binaries are build artifacts; the checked-in source of truth is the
manifest plus JSON catalog input files.

Authoring fonts such as TTF/OTF are never parsed by STM32 firmware. Development-host tooling converts them into compact MCU-oriented font resources containing rasterized glyph data, glyph metrics, supported symbols, and optional simple compression.

## Rules

- do not assume a full framebuffer;
- prefer source formats that convert cleanly to RGB565 or compact masks;
- keep installed font/resource size independent from SRAM use by designing for chunked W25Q reads;
- preserve licensing/source information for third-party assets;
- assign stable asset IDs so UI code does not depend on physical Flash offsets;
- keep a tiny internal-Flash emergency fallback font for basic diagnostic/safety messages.
