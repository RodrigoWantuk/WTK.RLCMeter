# Display throughput, charging UX and external text completion

STATUS: SOFTWARE_COMPLETE_DEPLOYED_REQUIRES_BENCH_VALIDATION

Owner request: improve display responsiveness; keep normal boot/navigation with the charger connected, block measurement with a friendly notice, audit external multilingual strings/resources. Preserve the existing owner splash image, orientation and three-second hold exactly. Existing plans 03/08 and plan 30 provide drivers, resource/UI and safety prerequisites.

## Scope and expected files

1. `drivers/ili9341.c`: send CASET/PASET/RAMWR under one bounded CS interval, propagate DC/transfer/release failures, always release ownership. Preserve command bytes, bus frequency and quiet admission.
2. `ui/ui_font.c`, `ui/ui_fallback_renderer.c`: one rectangle per nonempty glyph, retain row-major pixel streaming, blank glyph advances, bounded scratch and retry semantics. No framebuffer, new DMA or clock increase.
3. `app/app_product.c`, `ui/ui_product.[ch]`: charger-only idle state uses normal READY/menus with clear notice. Acquisition requests and calibration captures remain blocked; hard faults, residual and critical/invalid supply behavior retain priority. Plugging in during acquisition still aborts through hardware cleanup.
4. `ui/ui_text.[ch]`, `assets/text/{en,pt-BR}.json`, resource tools/manifest: externalize new lead/status/charging user copy with stable appended IDs and both languages. Technical format tokens and emergency fallback may stay internal. Preserve splash bytes and all mutable partition bounds.
5. Relevant driver/font/UI/controller/resource tests, bilingual previews, docs and release evidence.

## Acceptance

- Pixel-identical text in both languages compared with the previous host renderer, including scaled fallback, spaces and technical symbols. Explicit bus trace and mid-command/row failures verify command order, ownership cleanup and full-glyph retry.
- Record actual SPI bytes/CS transaction counts; estimates are not measured physical FPS. No clock increase or DMA1 Channel 5 use (already owned by excitation).
- Host charger plug/unplug/boot/menu/click/active-abort traces; serial OK remains hardware-gated. No measurement while charging, no deferred click after unplug.
- Text-ID/language/font coverage, current/legacy resource behavior, pack CRC and 40-KiB tail preservation. Never change the splash source/payload/timing.
- Strict unit/recovery builds, map/stack budget review and focused suites. Bench TFT appearance/speed requires owner confirmation; the board currently has display, charger and ST-Link, no COM4.
- Prepare reviewable firmware/resources before requesting any necessary physical swap to TTL for resource upload. Existing authorized internal programming and Git/PR workflow remain; no unattended DUT measurement.

## Limits

Measured display throughput and electrical signal integrity need bench evidence. Larger fonts/images are not added merely to fill free Flash. Preserve calibration/NTC/settings and existing unqualified metrology/voltage limits. Runtime display work remains outside acquisition quiet windows.

## Current checkpoints

Glyph/window implementation is committed with 72 pixel-identical before/after screens and failure/quiet tests. Charger-only navigation and 48 appended bilingual IDs are implemented; Resource API 5 explicitly signals the required new 137-entry catalog. Fonts include profile `#`; original splash/READY image payloads remain identical. 66/66 current host entries, 532 Python cases (38 optional skips), focused provisioning ON/OFF and nine strict GNU syntax checks pass. Clean-source matrix/sealing and physical deployment/resource readback/display acceptance remain in progress. See the [execution log](../docs/review/display-ux-2026-10-27/README.md).

Final checkpoint supersedes the in-progress sentence above: source a578bb5 is actually programmed and full internal readback verified. API 5 upload has 903 acknowledgements and complete byte-identical readback; the entire mutable tail is unchanged. Restart reaches READY/SAFE with calibration sequence 1, zero faults and zero acquisitions. Eight supported profiles passed at 651ea7c; the final UI-only live-VBUS correction was rebuilt for release/debug/serial-OFF PRODUCT and retested with both provisioning choices. The installed image uses 73,688 Flash / 15,212 accounted RAM bytes. Owner display/charger/menu/speed acceptance is still pending, so physical acceptance is not marked complete. See the [delivery](../Firmware/releases/2026-10-27/display-charging-ui/README.md).
