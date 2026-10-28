# Display throughput and charging UX — October 27, 2026

Active [plan 31](../../../plans/31-Display-Throughput-and-Charging-UX.md). The owner has display, charger and ST-Link attached; COM4 is absent. Non-halting GPIO reads confirm PA15=1, RANGE_EN=0 and K1_CMD=0. No acquisition is requested. The owner asks for normal charging navigation, friendly inhibition and external bilingual copy while preserving the personal boot splash.

## Glyph/window checkpoint

CASET/PASET/RAMWR now share one CS interval rather than five, retaining the same 11 bytes, DC boundaries, clock and timeout. Every successful acquisition is released after transfer/DC errors; quiet admission still occurs before bus ownership. Ordinary init command/data pairs likewise share one bounded transaction. Pixel writes remain short independent bus leases.

External A1 and emergency scaled glyphs now set one rectangle and stream row-major rows, retaining the existing bitmap/row buffers. Empty space advances without IO. BUSY during a row leaves cursor/index unchanged; a retry resets and redraws the full glyph. No framebuffer, extra static RAM, SPI clock increase or DMA is introduced.

Three focused driver/font tests pass, including new actual-driver command/DC traces, every command/DC failure, release failure, quiet rejection, multi-row Unicode, spaces, row BUSY/replay and scaled fallback pixel checks. The previous host preview executable was preserved before edits: **72 before/after screens match byte-for-byte** across both languages, 18 scenarios and normal/fallback resources. This is pixel and protocol evidence, not a measured physical speed claim.

For an illustrative 12x18 nonempty glyph, window/pixel ownership intervals fall from 108 to 19, and TFT wire bytes from 630 to 443; real screen benefit depends on glyphs, Flash reads and foreground scheduling. Full-background image speed does not improve by the same ratio. Physical responsiveness/appearance, strict final maps and charging UX/resource integration are pending.

## Charging navigation and text externalization checkpoint

Charger-only inhibition no longer hijacks idle presentation. Normal startup, READY, result browsing and menus remain available, with a localized USB-connected/unplug-to-measure notice. Acquisition scheduling and hardware permits still retain CHARGER. A disabled click is consumed and cannot replay after unplug. Plugging in during active acquisition drains the real owner before returning to inhibited READY. Actual residual, unknown/critical supply and fault flags keep their original warning priority. Native OPEN/SHORT capture cannot start or be confirmed while VBUS is present.

The live first-unit GPIO check confirms charger detect and de-energized command outputs. Subsequent TTL reconnection enumerates as COM3, with battery around 3.97 V and charger absent; that later sample does not overwrite the earlier connected observation. No DUT acquisition is requested.

48 appended stable text IDs externalize lead workflow, result-quality/error, diagnostic and friendly charging copy in EN/PT-BR. Small/medium/large Plex sources are regenerated from pinned TTFs; the technical `#` glyph is now required for profile sequence displays instead of silently showing a question mark. Emergency recovery and minimal unavailable/error tokens remain internal; units, protocol names and numeric format tokens are not prose localization.

Resource API **5**, text IDs 1..0x0089 (137 entries per language), explicitly replaces API 4's 89-entry catalog. The wire layout/schema stays unchanged. Old/new IDs 1..0x0059 retain their numbers. An old pack is deliberately rejected by the new firmware and routes to the safe PC-link recovery UI until a compatible pack is installed. Update procedure: preserve old resources/tail, program verified firmware, upload API 5 pack, full readback/CRC and exact tail comparison, then reconnect the display. No opaque count mismatch is treated as a valid catalog.

The new pack is 108,106 bytes (27 erase sectors), SHA-256 `51ca70f3471da43b988cfa7e9ac1fa41efbccacbcad4130fcf08ea0c7490d191`. Both image payloads are exactly identical to the installed API 4 pack: READY art 5,416 bytes, owner splash 81,576 bytes. Splash source/decoding/hold code is unchanged. No new decorative images replace the owner's concept.

Current software checks: 66/66 optional-curves host entries; focused controller/UI checks pass with both provisioning choices; 532 Python cases pass with 38 explicit optional skips; nine changed C/test translation units pass strict GNU warning-as-error syntax checks. A legacy synthetic font fixture also needed the new `#` glyph; its missing coverage was corrected without changing production images. A standalone test import path was made explicit. These are software checkpoints; final sealed source/profile maps and physical display acceptance are tracked next.

The provisional unit evaluation is 73,668 Flash / 15,212 accounted RAM bytes (including 2,052 aligned reserved stack bytes), below unchanged hard gates. Final clean-source image sizes supersede this evaluation. The owner has supplied TTL/COM3 and ST-Link for programming; before mutation the entire 40-KiB tail matches the prior snapshot exactly, and a fresh 104,499-byte API 4 resource backup matches the original installed pack byte-for-byte.

Final review adds an explicit live plug/unplug regression: the old READY generation coalescer ignored the new charging field. It now treats footer connection changes as visible while retaining metadata-only suppression. Both transition assertions fail against 651ea7c and pass after the correction. This is an additional UI-only correction before physical deployment; the first sealed/matrix checkpoint remains separately identified.
