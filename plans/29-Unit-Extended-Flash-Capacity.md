# First-unit writable extended-Flash capacity experiment

STATUS: COMPLETE_UNIT_EXPERIMENT_128_KIB_VERIFIED_PRODUCTION_LIMIT_UNCHANGED

The owner explicitly requests determining whether this MCU has writable 128 KiB,
after a 64-KiB factory register and inconclusive all-FF reads were reported.
This authorizes a bounded upper-half programming experiment on the known first
unit. It does not change the canonical C8 linker/deployment limit or qualify
undocumented capacity for production. No measurement or firmware feature change.

Prerequisites: live exact-image/UID/SAFE/idle/fault/charger/power guard; preserve
the current 61,596-B PRODUCT image, all 64 KiB below 0x08010000 and the readable
original upper half. Expected unit UID is 54ff70068370525760380567. Numeric VBAT
is never read over SWD. Archive and hash all backups before any Flash write.

Use the pinned OpenOCD target's temporary FLASH_SIZE=0x20000 override only in
this diagnostic. The size log is configuration, not proof. Check unprotected
factory option state, UID, device/core ID and 1-KiB page geometry. Do not change
option bytes, protection, firmware linkers or persistent calibration/assets.
Never mass erase. Keep the core in known reset-halted SAFE state during writes;
resume only after cleanup/readback and a controlled UART-observed clean boot.

First program a 16-byte sentinel at 0x0801F3F0 without erase, only after both
upper bytes and the corresponding lower bytes outside the installed image are
confirmed erased. The original last lower page contains stale prior-image data;
it is backed up rather than assumed erased. The unused end of page 60 provides
the verified erased lower comparison bytes for this first programming check.
Check every lower byte for aliasing, and verify the exact upper sentinel. Only
then erase upper pages individually. Verify the entire lower backup after each
page erase; stop on the first mismatch. Program all 65,536 upper bytes with a
halfword-address-dependent permutation, then a complementary pattern after a
separate page erase pass. Verify every upper byte and unchanged lower bytes,
including after reset. Distinct per-halfword data detects internal address alias.

After both full data patterns pass, erase only upper page 127 and place a tiny
Thumb diagnostic at 0x0801FFF0. Single-step MOVS/ADDS under debugger control and
check PC progression and R0 (0x5A then 0xC3). It performs no memory/GPIO accesses.
This confirms instruction fetching near the 128-KiB boundary, without changing
the production vector table or executing a new measurement application.

Restore the original upper data and confirm lower/upper backups byte for byte.
If aliasing touches the lower half, stop upper writes and recover changed lower
pages from the verified backup using the ordinary 64-KiB geometry before boot.
Program errors must retain readbacks and checkpoints, never count as PASS.

Expected durable files: this plan, capacity result documentation and archived
unit-bound diagnostic procedures/backups/transcripts/readbacks. Application,
calibration, BSP, linker and standard programmer geometry remain unchanged.
Use bounded host orchestration and stage checkpoints. Verify deterministic
patterns, address/page bounds and original-image hashes before target access.
Record actual results, restore/boot/OSL status and any remaining unit-specific
temperature/endurance/manufacturer qualification limits. Publish through the
existing authorized Git/PR workflow.

Acceptance: both complete upper-half patterns and their reset readbacks match;
all lower bytes are preserved. Two instructions at 0x0801FFF0 were executed with
the expected R0/PC progression. Original upper contents and the complete lower
backup were restored. Unchanged PRODUCT boots READY/SAFE with zero measurements,
OSL 1/all 33 and no fault. At least 128 KiB is experimentally usable on this unit;
canonical C8 limits stay 64 KiB. No firmware rebuild was needed.

The backup's last lower page was not blank; the guard stopped before programming
and the first sentinel moved to verified erased unused lower comparison bytes.
The initial full erase/check stage exceeded its 60-s host deadline; a known SAFE
programmer-context recovery confirmed the lower image untouched, then bounded
180-s stages completed. Two burst serial status queries timed out after boot;
one paced read-only query succeeded, with no measurement. These observations
remain recorded rather than hidden or counted as Flash failures.

No power cycle, temperature/endurance/retention qualification or test beyond
128 KiB was performed. See the
[unit capacity evidence](../Firmware/releases/2026-10-27/extended-flash-capacity/README.md).
