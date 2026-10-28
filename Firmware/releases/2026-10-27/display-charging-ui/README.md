# Deployed display throughput, charging navigation and bilingual resources

October 27, 2026, America/Sao_Paulo — **DEPLOYED / READY / SAFE**. Physical TFT appearance, responsiveness and charging navigation remain **REQUIRES_BENCH_VALIDATION**. This supersedes the earlier production/watchdog image without replacing its qualification limitations.

Normal boot, READY and menu navigation remain available with USB charging detected. A localized footer asks the user to unplug USB before measuring. Acquisition admission, native OPEN/SHORT capture, active-session abort and hard-fault priority retain their safety gates. Disabled measurement clicks are consumed, including across unplug. Live connection changes invalidate the READY footer; metadata-only refresh suppression remains.

TFT window commands share one bounded CS interval; external and fallback fonts open one rectangle per glyph and stream rows using existing small buffers. No framebuffer, clock increase or new DMA is introduced. A representative 12x18 glyph needs 19 instead of 108 ownership intervals and 443 instead of 630 TFT wire bytes. These protocol counts are not measured physical FPS. Existing menu row caching remains; full-image performance has a different cost.

The API 5 external catalog has 137 stable IDs per language, including 48 appended bilingual EN/PT-BR lead/status/charging strings. Plex fonts include accented text and the technical `#` symbol. Emergency recovery retains internal text. The existing owner splash and READY art payloads are **byte-identical**, including the existing orientation and three-second splash hold. No new decorative art replaces them.

## Installed artifacts and memory

[firmware.zip](firmware.zip) contains the clean-source sealed image, ELF, config, linker and manifest. Source: `a578bb561a426f625f31edf5aeb0654a266fc258`. Profile: `FACTORY_PRODUCT_UNIT128`, explicitly bound to UID `54ff70068370525760380567`. BIN SHA-256: `b44deef152dee2ce5130d11b05a41e3eddd69729f0e7f2e99750a4ff9e04849a`.

| Memory | Bytes |
| --- | ---: |
| Flash used / explicit unit capacity | 73,688 / 131,072 |
| Flash free / hard-gate margin | 57,384 / 56,360 |
| Static RAM / aligned stack reserve | 13,160 / 2,052 |
| RAM accounted / hard-gate margin | 15,212 / 2,196 |

The image saves 1,404 Flash bytes relative to the previous 75,092-byte deployment. The factory register still declares 64 KiB; experimentally usable 128 KiB applies only to this previously tested unit. Runtime stack high-water and worst-case cooperative latency remain unqualified.

[resources-api5.wrp2](resources-api5.wrp2) is **108,106 bytes**, SHA-256 `51ca70f3471da43b988cfa7e9ac1fa41efbccacbcad4130fcf08ea0c7490d191`. Firmware and this pack must be deployed together: API 4 is deliberately rejected into safe PC-link maintenance. Existing resource-tool upload instructions apply; the actual transaction used 27 sectors `[0, 110592)` of the physical 4-MiB W25Q.

## Actual board evidence

The owner supplied TTL and ST-Link; Windows enumerated the CH340 as **COM3**, despite its earlier COM4 assignment. Internal page programming and full 128-KiB readback passed on the expected UID. Before replacing resources, the entire old API 4 pack was backed up and matched its original 104,499-byte image exactly. All **903 upload frames were acknowledged**; a complete subsequent UART readback matches every API 5 byte. The whole **40-KiB mutable tail is identical** before/after, SHA-256 `ddefec8ae8b4e979470a6597eb6a6d2a91b506acf87b6e8716e6744538487bc7`. Factory OSL sequence 1/all 33, settings/lead slots and experimental compiled NTC identity are preserved.

One controlled restart after upload produces one SAFE_BOOT. Fresh UART reports source `a578bb561a42`, READY, zero faults/errors/overflows, K1 commanded SAFE, range disabled, calibration sequence 1 and zero acquisition sessions. Battery reads **3.955 V at 21:21:50 on October 27**, charger absent following the TTL swap. Numeric voltage was read only through UART and uses nominal VDDA conversion. Earlier read-only GPIO evidence with display/charger attached recorded PA15=1 and K1/range outputs low. No DUT measurement or serial OK was requested during this work.

## Validation and remaining acceptance

66/66 optional-curves host entries, focused controller/UI checks with both provisioning choices, 532 Python cases with 38 explicit optional skips, nine strict GNU syntax checks, and 72 byte-identical pre/post optimization screens pass. The initial missing `#` coverage in a legacy test font is retained as a failed historical checkpoint and then corrected. Eight supported embedded profiles pass at source 651ea7c; after the final UI-only live-connection correction, release/debug/serial-OFF PRODUCT builds and both provisioning UI regressions were refreshed at a578bb5. The generic full PRODUCT still hits its unchanged 64-KiB budget gate.

[report.json](report.json) records hashes, memory and observations. [evidence.zip](evidence.zip) has a SHA-indexed 287-file inventory: raw programming/readbacks, boot/UART, complete old/new resource snapshots, upload ACKs, mutable-tail comparison, pixel proofs, tests/maps and distinct superseded checkpoints. The packaged firmware remains the exact clean-source image actually installed; later documentation commits do not change it.

Owner validation requested: reconnect the display with power off, boot with charger attached, confirm unchanged splash, normal READY/USB notice and menu text responsiveness. Broader settings/reboot, buttons/buzzer/quiet, lead OPEN/SHORT, independent R/C/L holdouts, electrical timing, polarized envelope and 10-mF endpoint remain REQUIRES_BENCH_VALIDATION. No repeated factory LOAD campaign is required.
