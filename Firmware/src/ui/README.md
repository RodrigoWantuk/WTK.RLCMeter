# `ui`

Graphical user interface for the ILI9341 display.

## Implemented Phase 08 Stage 1 files

```text
ui_format.c/.h
ui_product.c/.h
```

`ui_format` provides bounded SI/unit formatting for product result views without target
float `printf`. `ui_product` renders the initial PRODUCT view model with no full
framebuffer and defers rendering while quiet mode is active. The Phase 08 Stage 1.1
renderer is cooperative: clears/fills are chunked by the ILI9341 driver, fallback text
draws one scaled character per step, and newer view generations are coalesced rather
than queued without bound.

## Planned files

```text
ui_core.c/.h
ui_theme.c/.h
ui_navigation.c/.h
ui_widgets.c/.h
screen_startup.c/.h
screen_measure.c/.h
screen_details.c/.h
screen_graph.c/.h
screen_settings.c/.h
screen_calibration.c/.h
screen_diagnostics.c/.h
```

## Rules

- no full-screen framebuffer;
- incremental/dirty-region rendering;
- no blocking animation delays;
- large bitmaps streamed from W25Q;
- UI never controls relays or ranges directly;
- SI-prefix/unit formatting remains separate from metrology calculations;
- heavy SPI updates are suspended in quiet mode.

Same-screen product updates clear only the compact result/body region instead of forcing
a full-screen clear. Full clears remain for state/page transitions and the first render.
The current pixel chunk size intentionally stays small to preserve SRAM until physical
SPI/display timing data justifies a larger scratch buffer.

## Baseline screens

- startup;
- main measurement;
- impedance/phase detail;
- graphs/derived visualizations;
- settings;
- calibration;
- diagnostics.

## Interaction

Three buttons: UP, OK, and DOWN, with press, long-press, and repeat events where appropriate.

## Phase 03 resource/font baseline

The current UI resource layer provides only stable streaming and backend contracts:

```text
W25Q/resource reader -> 256-byte scratch buffer -> renderer/display writer
font backend lookup/read callbacks -> replaceable glyph source
```

Resource streaming uses a tri-state result: `OK`, `DEFERRED`, or `ERROR`. A deferred W25Q/TFT operation preserves the resource offset so quiet mode can pause rendering and resume later instead of aborting the stream.

This keeps the font renderer replaceable. The firmware is not committed to MCUFont or any other library yet. A tiny built-in 5x7 fallback font plus emergency renderer exists only for minimal diagnostic/error rendering if W25Q resources are absent or corrupt.

## Phase 08 Stage 3A resource text

Emergency safety, fault, storage, and calibration-gate text remains available from
internal Flash. Normal product menu labels and localized UI text are resolved through
stable text IDs backed by Resource Pack v2 text catalogs in W25Q.

Resource text lookup is explicitly tri-state: `OK`, `DEFERRED`, or `ERROR`. `DEFERRED`
lets quiet mode or W25Q mutation policy pause rendering without losing the pending
text operation. Missing, corrupt, or incompatible resource packs put PRODUCT UI into
the emergency `RESOURCE_ERROR` presentation; this is a product-operation blocker, not
a safety fault.
