# Display throughput and charging UX — October 27, 2026

Active [plan 31](../../../plans/31-Display-Throughput-and-Charging-UX.md). The owner has display, charger and ST-Link attached; COM4 is absent. Non-halting GPIO reads confirm PA15=1, RANGE_EN=0 and K1_CMD=0. No acquisition is requested. The owner asks for normal charging navigation, friendly inhibition and external bilingual copy while preserving the personal boot splash.

## Glyph/window checkpoint

CASET/PASET/RAMWR now share one CS interval rather than five, retaining the same 11 bytes, DC boundaries, clock and timeout. Every successful acquisition is released after transfer/DC errors; quiet admission still occurs before bus ownership. Ordinary init command/data pairs likewise share one bounded transaction. Pixel writes remain short independent bus leases.

External A1 and emergency scaled glyphs now set one rectangle and stream row-major rows, retaining the existing bitmap/row buffers. Empty space advances without IO. BUSY during a row leaves cursor/index unchanged; a retry resets and redraws the full glyph. No framebuffer, extra static RAM, SPI clock increase or DMA is introduced.

Three focused driver/font tests pass, including new actual-driver command/DC traces, every command/DC failure, release failure, quiet rejection, multi-row Unicode, spaces, row BUSY/replay and scaled fallback pixel checks. The previous host preview executable was preserved before edits: **72 before/after screens match byte-for-byte** across both languages, 18 scenarios and normal/fallback resources. This is pixel and protocol evidence, not a measured physical speed claim.

For an illustrative 12x18 nonempty glyph, window/pixel ownership intervals fall from 108 to 19, and TFT wire bytes from 630 to 443; real screen benefit depends on glyphs, Flash reads and foreground scheduling. Full-background image speed does not improve by the same ratio. Physical responsiveness/appearance, strict final maps and charging UX/resource integration are pending.
