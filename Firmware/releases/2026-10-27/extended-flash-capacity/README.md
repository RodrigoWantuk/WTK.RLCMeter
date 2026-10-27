# First-unit extended Flash: 128 KiB experimentally verified

Unit: `54ff70068370525760380567`. The factory-size register still reads **64 KiB**,
but this unit passed actual erase/program/readback tests across the entire extra
64 KiB, plus instruction execution near the 128-KiB boundary. At least **131,072
addressable Flash bytes** are experimentally established on this MCU. This does
not change the STM32F103C8 manufacturer specification or qualify other units.

## Actual evidence

- Backed up both 64-KiB halves before writes, with independent recovery copies.
  Current PRODUCT prefix is 61,596 B, SHA-256
  `5dd9a53f4117a5239e2ec0769e0560c10ce4114789f7782ef929da4b684aba7d`.
- Wrote a 16-byte sentinel at `0x0801F3F0` and confirmed it without changing any
  lower byte. The corresponding unused lower bytes were independently erased.
- Erased only upper pages 64–127, one 1-KiB page per command, checking the entire
  lower backup after every erase. No mass erase or option-byte changes.
- Programmed all **65,536 bytes at 0x08010000–0x0801FFFF** with a halfword-address
  permutation containing 32,768 distinct values. Readback matched every byte;
  the same bytes matched again after reset.
- Repeated erase/program/readback/reset with the bitwise complementary pattern,
  exercising both bit states across the complete upper half. Lower bytes stayed
  identical throughout; no address alias was observed.
- Erased only upper page 127, placed `MOVS R0,#0x5A; ADDS R0,#0x69` at
  `0x0801FFF0` and single-stepped. Actual PCs were `0x0801FFF2/0x0801FFF4` and
  R0 was `0x5A/0xC3`. These instructions perform no memory/GPIO access. This is
  instruction-fetch evidence, rather than a programmer merely accepting a size.

Pattern SHA-256 values:

```text
A: 3debe8ce273aeee0094a7e5a0749dabdfd687ecff46c270962a4feb642cb6039
B: de01784d78bb4facf097f381b5b50d9985d254ebde5c2ad735fc26dc6196b346
```

## Restoration and scope

Restored the original upper half, which was all FF, and confirmed **both halves
byte for byte** against their backups. The entire lower-half hash remained
`46d51c9aed62fc5c744a25e217df6d5b003b4fc34d51bba1320ad52bc364e9a1`.
Ordinary 64-KiB programmer configuration then booted the unchanged PRODUCT:
exactly one COM4 SAFE_BOOT, READY/SAFE, zero measurements, active OSL sequence
1/all 33, no faults, range/source OFF and quiet/workspace free. Assets/settings
and UID-bound NTC were preserved. No numeric battery voltage was read via SWD.

Two ordinary burst status queries produced no complete reply after restoration.
A read-only query paced at 3 ms per byte was accepted, reporting READY/session 0.
The firmware remains the original image; serial sender pacing is a separate
follow-up concern, not a capacity failure or a completed measurement claim.

An initial assumption that the unused lower last page was erased proved false:
it contains older-image remnants outside the installed prefix. The first backup
guard stopped before writing, and the sentinel comparison moved to verified
erased bytes beyond the current image. A later 60-s orchestration timeout during
per-page lower verification was also stopped and recorded. Recovery confirmed
the lower image untouched and reset the known halted programmer context before
the successful attempt with a bounded 180-s stage limit. Interrupted attempts
are not counted as capacity failures or successful full-pattern tests.

OpenOCD's temporary `FLASH_SIZE=0x20000` configuration enables the experiment;
its reported configured size is **not** used as evidence. Actual byte comparisons,
reset persistence, unchanged lower contents and executed instructions establish
the result. See the [OpenOCD driver](https://www.openocd.org/doc/doxygen/html/stm32f1x_8c_source.html)
and [ST programming manual](https://www.st.com/resource/en/programming_manual/pm0075-stm32f10xxx-flash-memory-microcontrollers-stmicroelectronics.pdf)
for the programming mechanism.

Production linkers, build budgets, normal deployment geometry and the installed
firmware remain at **64 KiB**. A future explicit unit-specific 128-KiB build would
have **69,476 bytes** beyond the current 61,596-B image. No physical power cycle,
temperature/voltage sweep, endurance or retention qualification was performed.
Addresses beyond 128 KiB were not tested, so this establishes a minimum usable
capacity rather than the exact maximum die density.

[report.json](report.json) records the checks, identities, hashes and current
boot state. [evidence.zip](evidence.zip) contains backups, every stage/readback,
transcripts, deterministic patterns and the exact unit-bound procedures; the
archive hash is in the report. No application/driver/linker source changed,
so previous firmware host/build checks were not rerun. The new validation is the
actual bounded hardware experiment and its independent host comparisons.
