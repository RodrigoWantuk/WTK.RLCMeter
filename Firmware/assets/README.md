# `assets`

Source assets for the graphical UI.

## Expected content

- startup/splash artwork;
- icons;
- fonts;
- auxiliary images;
- source manifests/metadata.

Files in this directory are source inputs. Firmware consumes packed/generated assets produced by tooling under `tools/`.

## Rules

- do not assume a full framebuffer;
- prefer source formats that convert cleanly to RGB565 or compact masks;
- preserve licensing/source information for third-party assets;
- assign stable asset IDs so UI code does not depend on physical Flash offsets.
