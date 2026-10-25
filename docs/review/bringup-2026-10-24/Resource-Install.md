# Production Resource Pack physical installation — 2026-10-24

Status: INSTALLED_AND_READBACK_VERIFIED; STANDARD_BRINGUP_RESTORED.
Owner-requested resource provisioning under
[plan18](../../../plans/18-First-Board-Resource-Provisioning.md).

The fitted EF4016 W25Q has4MiB capacity. The current tracked manifest builds a
22891-byte schema2/API4 pack with six entries: complete English and Portuguese
(Brazil) text catalogs, small/medium/large IBM Plex A1 bitmap fonts, and optional
240x320 RGB565 RLE READY artwork. Two independent builds were byte-identical.
The pack SHA256 is
`1048f3baa7942354e1c3d84b9b5cb361e7e3bd7347f221fdc2d3b2660b6ee9b4`.

## Physical operations and preserved data

Preflight COM4 reported real battery/VMID valid, charger ABSENT, zero faults,
K1 SAFE and ranges DISABLED. DUT terminals had been confirmed empty. No DUT
transaction or relay diagnostic was started in this task.

The resource erase span was exactly six4096-byte sectors,
`[0x000000,0x006000)`. The capacity-relative mutable tail is
`[0x3F8000,0x400000)`,32768bytes. Before programming, both ranges were backed up
through the existing read-only W25Q driver on the verified idle BRINGUP image;
both were erased. The rest of the resource partition was outside the write scope.
No chip erase was issued.

Published clean source0dd1f3f factory PRODUCT Release was used temporarily because
it includes the real PLC1 resource receiver and rich-image admission. Its52848
internal-Flash bytes were verified against SHA256
`239ac2b6866da5461a6252264c16ebe1b7f6433811d19adcd62028c22ba442d5`.
The missing-OSL gate remained enforced. This was resource recovery on blank media,
not acceptance of a final calibrated factory PRODUCT deployment.

The initial serial attempt confirmed BEGIN and two data frames,240bytes total.
The next frame timed out. A diagnostic snapshot found RX130/144bytes, update READY,
no pending NOR operation, zero faults and all measurement outputs off. The
incomplete frame had not been applied. Its original report was retained.

After that explicit diagnosis, a separate controlled reset returned the partial
pack to recovery. A new BEGIN transferred the entire pack using10ms per byte,
per-byte flushing and a10s wait after BEGIN for the recovery redraw. All193
frames were acknowledged in order; the final response was COMPLETE/state6.
The unpaced host uploader was not physically qualified by this operation.
Keep these pacing requirements when reproducing this board's polled-UART path.
The observed truncation is transport evidence, not a W25Q defect diagnosis.

## Verification

Live PRODUCT state after the upload was resource statusOK and
CALIBRATION_REQUIRED, with no active measurement, no valid calibration, no faults,
and K1/RANGE_EN/PWM_EXC command latchesLOW. A controlled PRODUCT restart again
admitted the required bilingual texts/fonts and remained CALIBRATION_REQUIRED.
The pack includes the optional artwork; visible pixels were not inspected here.

The exact standard BRINGUP image was restored and its65488bytes readback matched
SHA256 `e4af20eff8f4e06de291fd2c06e5de1437517ad0f0364da253a736f6f244876a`.
Reading the external Flash through that image then established:

- All22891 pack bytes exactly match the built file and pass the host pack
  inspector, including header/table/payload CRCs, text/font and image bounds.
- The remaining1685bytes in the six-sector erase span are still0xFF.
- The entire32768-byte mutable tail is byte-identical to its pre-write backup,
  SHA256 `2d864c0b789a43214eee8524d3182075125e5ca2cd527f3582ec87ffd94076bc`.
- No calibration/settings/curve/diagnostic record was created or overwritten.

Debugger driver calls use the idle shared scratch arena, which was restored.
Debugger/Flash algorithms can disturb CPU execution context, so a separate
controlled reset/run and captured UART boot followed the backup/readback work;
debugger detach alone was not used as application boot acceptance.

The final captured standard BRINGUP boot identifies0dd1f3fe40ce, HSE_PLL72MHz.
Final status: batteryOK, residualSAFE, chargerABSENT, faultmask0, K1SAFE,
rangeDISABLED and NO_VALID_CALIBRATION. No serial port/server remains open.

## Reproduction and evidence

The [published bundle](../../../Firmware/releases/2026-10-24/production-resources.md)
contains the installed pack, summary, source manifest and font license. Rebuild
from the tracked manifest using the existing
[resource tools](../../../Firmware/assets/README.md). Source-file hashes, physical
readback, partition preservation, temporary/restored firmware identity and
post-reset PRODUCT states are in [resource-install.json](resource-install.json).
All successful PLC1 TX/ACK bytes are retained in
[resource-upload-events.json](resource-upload-events.json); the final UART
transcript is [resource-final-bringup-uart.txt](resource-final-bringup-uart.txt).
Full original backups/debugger/programmer logs remain in ignored
`Firmware/build/assets-install-20261024/`.

This establishes real erase/program/readback and reset persistence for this pack.
Full-capacity coverage, wear, interrupted-write recovery, power removal,
standalone supply sequencing and physical PRODUCT screen appearance remain
REQUIRES_BENCH_VALIDATION. The independent K1 hardware inhibit and calibration
campaign remain separate pending work.
