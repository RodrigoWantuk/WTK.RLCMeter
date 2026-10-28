# Deployed product status fixes and persistent picture controls

October 28, 2026 — **DEPLOYED / FULL READBACK VERIFIED / READY / SAFE**. Physical display appearance and throughput remain **REQUIRES_BENCH_VALIDATION** while the module is disconnected for UART. This supersedes the October 27 display/charging image.

About previously overflowed the 240-pixel screen while drawing the complete hardware compatibility name in the medium face. The actual rasterizer reproduced an off-screen 4x12 glyph at x=240/y=113, returning INVALID_ARG and entering FAULT despite an electrical mask of zero. The small face now fits the full identity; error handling remains checked. Diagnostics publish fresh NTC data as rounded tenths of a degree Celsius, preserving sign, locale, age and stale/invalid `n/a`. Selecting Calibration opens the quick lead OPEN/SHORT menu directly, with factory status and back still available; menu entry never starts capture.

The owner resolved the perceived brightness problem as the TN viewing angle. Earlier non-halting reads at 100% confirmed continuous-high PWM and PB0 HIGH; no PWM polarity or analog-drive change was needed.

## Screen controls and throughput

Display includes brightness, timeout, gamma 1.0/0.8/1.2, contrast 50..150%, saturation 0..200%, inversion, restore display defaults and back. UP/DOWN previews; short OK saves through the existing transaction; long OK restores the entry values. Restore affects screen settings only. Gamma/contrast/saturation are real, bounded **software RGB565 adjustments**; inversion uses DCS 20h/21h. All 65,536 neutral RGB565 values are bit-identical. A 128-byte fixed gamma LUT and integer arithmetic avoid heap/framebuffer allocation. Unsupported sharpness and speculative panel power/VCOM controls are not exposed.

Changes wait for an idle renderer and the end of quiet, then invalidate its baseline to repaint all affected pixels. Ordinary navigation/numeric edits retain dirty-row rendering. Ten cached rows (one added row) cover the expanded menu and disappearing save warning. The original splash/READY image payloads, mounting orientation and completed-image 3000-ms boot hold remain unchanged. Persisted picture adjustments apply **after** the splash.

TFT writes use **9 MHz**, reads and W25Q **4.5 MHz**, with the existing slow recovery override. The board's 36-MHz APB1 SPI2 cannot produce the linked ESP32 examples' 27/40-MHz clocks. The [ILITEK datasheet](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf), sections 14 and 18.3.4, documents only the fixed gamma-2.2 preset and 100/150-ns minimum write/read cycles. No C++ TFT_eSPI, display DMA or full-screen double buffer is introduced. Configured clock separation is tested; actual signal integrity/FPS still requires the display and bench observation.

## Persistence and resources

Settings schema **5** retains the 36-byte frame, eight-byte payload, commit location and redundant A/B slots. Its previously reserved, CRC-covered header word contains gamma bits 0..1, contrast bits 2..4, saturation bits 5..7 and inversion bit 8. Invalid indices/reserved bits are rejected. Schemas 2/3/4 load with neutral picture defaults. No migration writes occur merely on boot; saves remain explicit. Host tests cover reload, legacy migration, invalid values, edits/cancellation, immutable transactions and power interruptions. Actual nonneutral save/reboot acceptance remains pending; the physically loaded prior user settings are preserved exactly.

API **6** adds five EN/PT-BR labels through ID 0x008E (142 per language). [product-api6.wrp2](product-api6.wrp2) is **108,288 bytes**, SHA-256 `c0aec2d24e11a5594f0b7a1e6144b4df1e9e49a70b81a0bcb703c01cafff0502`. Use this pack with the matched firmware; API 5 is deliberately rejected into safe PC-link maintenance until updated. Fonts were regenerated and remained identical because existing glyph coverage already includes the new copy. Both image payloads are byte-identical to the complete physical API 5 backup.

## Actual delivery

[firmware.zip](firmware.zip) contains the sealed clean-source image, ELF, linker/config and manifest. Source: `9c14a6c8496f5a8b97ca0d76878cca2f89389a8d`. BIN SHA-256: `a6462652c2ab703ab451876635340d3bb94b4f2a32dc7dd7b381bcb99629f011`. Later commits change only contract tests/documentation/evidence; runtime/assets match this source exactly.

| Memory | Bytes |
| --- | ---: |
| Flash used / explicit verified-unit capacity | 75,156 / 131,072 |
| Flash free / hard-gate margin | 55,916 / 54,892 |
| Static RAM / aligned stack reserve | 13,236 / 2,052 |
| Accounted RAM / hard-gate margin | 15,288 / 2,120 |

The factory register remains 64 KiB. The 128-KiB engineering profile is bound to the previously tested first UID `54ff70068370525760380567`; generic C8 geometry is unchanged. Runtime stack high-water and worst-case latency remain unqualified.

The owner reconnected ST-Link and the CH340 UART, enumerated **COM3**. Plan-32 image 21a9a41 was first programmed/readback verified and booted READY/SAFE. The final 9c14a6c image was then physically programmed with bounded internal page operations and full 128-KiB readback, without changing bytes outside its approved pages. The complete old API 5 pack was backed up, then **905 resource frames acknowledged**. A full UART readback matches every new pack byte. The complete **40-KiB mutable tail** is identical before/after, SHA-256 `8825cacce71cecbc652228bca75e83b042269dbc58d2918dc765e9e6a736fdff`, preserving factory OSL sequence 1/all 33, lead slots and the owner's saved settings. This hash differs from October 27 because the owner had saved brightness edits before these updates; no false equality with the older snapshot is asserted. The compiled experimental NTC profile is unchanged.

One controlled restart after resource installation produces one SAFE_BOOT. Fresh UART reports source 9c14a6c8496f, READY, zero faults/errors/overflows, K1 commanded SAFE, range disabled, calibration sequence 1, loaded settings and zero acquisition sessions. Battery is **4.134 V** (nominal-VDDA conversion); charger is absent. No battery voltage was read through SWD. No DUT measurement, serial OK, new lead capture or physical settings mutation was requested during this work.

Validation: **66/66 host entries**, focused app/UI/settings checks **3/3 with provisioning OFF**, **532 Python cases** with 38 explicit optional skips and the genuine C bridge, **eight supported embedded profiles**, expected generic 64-KiB full-product budget stop, 76 bilingual/fallback measurement renders plus **36 status renders** with actual fonts/strict bounds, and exhaustive neutral colors. Older failed checkpoints retain stale API assertions and a Windows executable-link collision; the final build, CTest and Python runs execute sequentially and pass.

[report.json](report.json) and SHA-indexed [evidence.zip](evidence.zip) contain both actual deployment stages, raw UART/readbacks, upload ACKs, mutable-tail preservation, image proofs, tests/maps and separate failed/superseded logs. [picture-preview.png](picture-preview.png) is an illustrative host preview, not a physical screenshot or current sensor reading.

Pending owner checks: About/Diagnostics, direct Calibration → New OPEN/SHORT discovery, display controls/confirm/cancel/save/reboot and visible 9-MHz performance. Broader lead-end compensation, buttons/buzzer/quiet, electrical timing, independent R/C/L accuracy, polarized envelope, 10-mF endpoint and stack/latency remain REQUIRES_BENCH_VALIDATION. No new factory LOAD campaign is required.
