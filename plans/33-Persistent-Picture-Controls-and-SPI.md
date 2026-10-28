# Persistent picture controls and bounded TFT throughput

STATUS: SOFTWARE_COMPLETE_DEPLOYED_REQUIRES_BENCH_VALIDATION

Owner requests persistent screen image controls and more throughput, supplies ST-Link/UART and will be absent. Plan 32 corrections are deployed as 21a9a41, READY/SAFE; the owner identified the apparent brightness problem as the TN viewing angle. No new acquisition is authorized or required. Preserve boot splash appearance, orientation and completed-image 3000-ms hold.

## Evidence and decisions

The ILITEK datasheet documents only preset gamma 2.2 on ILI9341 (section 14); unlike ST7789, its other GAMSET curves are unspecified. Neither sharpness nor saturation is a standard image adjustment register. Controller backlight commands do not establish an electrical connection to this module's PB0-driven LED input. Do not expose imaginary controller controls, change analog VCOM/power registers, or import C++ TFT_eSPI into C17.

Implement explicitly software RGB565 gamma/contrast/saturation and standard DCS inversion, alongside the existing real PWM brightness/timeout. Identity is bit-exact. Use bounded integer transforms and a tiny fixed LUT, no heap/framebuffer. Apply only after splash and outside quiet, with a clean renderer boundary and cache invalidation; defaults preserve existing artwork. No sharpening filter is invented without neighbor pixels.

The 36-MHz APB1 SPI2 can supply 18 MHz maximum. ILITEK specifies 100-ns minimum write cycle / 150-ns read cycle. Select 9-MHz TFT writes, retain 4.5-MHz reads and W25Q, plus existing explicit slow recovery. No 27/40-MHz claim, new DMA, overclock or full-screen sprite. Existing bounded row/image streaming and row cache remain.

## Implementation boundaries and expected files

- `ili9341_color.[ch]`: pure fixed RGB565 adjustment and exhaustive neutral/limits tests; driver command/quiet/error tests.
- `ili9341.[ch]`, `spi_bus.[ch]`: bounded picture application and read/write clock separation; no safety/application policy in drivers.
- Settings service: portable schema 5, CRC-protected picture fields in the previously reserved header word, unchanged frame/slot sizes, schema 2/3/4 migration to neutral picture. Retain commit-last redundant slots, immutable in-flight snapshots and checked parsing.
- Product controller/view/output/board adapter and renderer: gamma/contrast/saturation/inversion editors, confirm/save, cancel/restore and screen-only defaults. Menu selection and numerical changes retain dirty-row behavior; a global picture change requires a clean repaint. No relay/ADC policy changes.
- API 6 EN/PT-BR labels and regenerated fonts, matched resource bundle, full physical upload/readback and exact mutable-tail comparison. Images are byte-identical.
- Relevant host/strict embedded profiles/maps, physical verified deployment, indexed evidence, docs and PR update. Hardware visual picture/SPI acceptance remains REQUIRES_BENCH_VALIDATION while display is disconnected.

Primary references: [ILITEK datasheet](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf), sections 8.2.15/16/17, 14 and 18.3.4; [Bodmer initialization](https://github.com/Bodmer/TFT_eSPI/blob/master/TFT_Drivers/ILI9341_Init.h). The owner-linked article is context, not hardware qualification. No speculative analog initialization is copied from other modules.

## Final checkpoint

All requested supported picture controls are implemented with preview/confirm/cancel, screen-only restore and schema-5 persistence. Inversion uses standard DCS; gamma/contrast/saturation use a 128-byte LUT and bounded integer RGB565 transformation. Neutral is exhaustive bit-exact. Ten cached rows account for the expanded menu/save-warning lifetime. Eight embedded profiles pass; the final unit image is 75,156 Flash / 15,288 accounted RAM bytes. Generic full PRODUCT retains its expected 64-KiB budget stop.

66 host entries, three focused provisioning-OFF entries, 532 Python cases with 38 optional skips, 76 measurement and 36 status-page bilingual/fallback renders pass. Stale API-5 test expectations were updated to API 6. An attempted parallel build conflicted with an executing C bridge on Windows; the final full build/CTest/Python run is sequential and passes. Runtime/assets remain identical to clean source 9c14a6c; subsequent changes are test/docs/evidence only.

The sealed firmware is actually programmed and full internal readback verified. All 905 API-6 frames are acknowledged, all 108,288 resource bytes read back exactly, and both image payloads match the old physical backup. The complete mutable tail remains byte-identical to the fresh preprogram snapshot (8825cacce71cecbc652228bca75e83b042269dbc58d2918dc765e9e6a736fdff). Restart reaches READY/SAFE with OSL sequence 1, loaded settings, no faults/UART errors and no acquisition. Battery is 4.134 V through UART. The display is disconnected, so physical picture/save/reboot/9-MHz acceptance remains pending. See the [delivery](../Firmware/releases/2026-10-28/picture-controls/README.md).
