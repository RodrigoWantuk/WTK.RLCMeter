# Production completion execution log

STATUS: IN_PROGRESS_NOT_DEPLOYED

The owner authorized implementation after the
[audit](../production-audit-2026-10-27/README.md).
[Plan 30](../../../plans/30-Production-Firmware-Audit-and-Completion.md) defines
the complete scope. This log records actual progress; unfinished packages are
not release-complete. The physical unit still runs the previously recorded image.
No target access or automatic DUT measurement has occurred during this work.

## Serial checkpoint

USART1 now receives through a bounded interrupt ring with explicit error/overflow
poisoning, receive epochs and independent quiet/application discard holds. Boot,
measurement and settings-busy requests cannot remain queued for a later OK.
Actual GPIO/source/power admission remains separate. Fault diagnostics can resume
after hardware teardown, and an accepted settings operation can drain safely
without starting a new save in FAULT.

[The PRODUCT serial protocol](../../17-Product-Serial-Protocol.md) adds a read-only
128-byte telemetry response with actual auxiliary ADC-derived battery/NTC values,
ages/validity, UID/build identity, last published RLC, features and error counters.
Unavailable data uses sentinels and becomes JSON null in the PC client. Optional
byte pacing preserves compatibility with older polled receivers; activation
is never retried automatically.

Focused CTest runs pass **4/4 factory and 4/4 standard**, and PC client tests pass
**6/6**. The strict unit build uses **64,908 B Flash / 14,484 B accounted RAM**.
See [serial-checkpoint.json](serial-checkpoint.json) and
[serial-evidence.zip](serial-evidence.zip). The generic 64-KiB PRODUCT hard Flash
budget is unchanged and is now smaller than the complete telemetry image; the
explicit 128-KiB unit profile is used. Final supported/compatibility profile
selection remains package F work, rather than silently relaxing a C8 gate.
No hardware access or deployment was performed for this checkpoint.

## Foundation checkpoint

| Package | Software state | Physical acceptance |
| --- | --- | --- |
| A: explicit unit memory profile | 128-KiB CMake/linker/build/manifest/programmer bounds implemented; load-only OSL saves RAM | New deployment/boot and stack high-water pending |
| B: emergency SAFE/watchdog | Direct fatal-output shutdown; watchdog failure latched in both shells | Controlled fault injection/timing pending |
| C: settings | Accepted-start adapter, immutable snapshot, CRC schema 4, timeout and legacy migration implemented | Actual save/reboot/interrupted-write pending |
| D: display/feedback | Idle RANGE blocker fixed; product patterns/queue and armed TIM4 stop implemented | Display timeout, tones and quiet interference pending |
| E: UART/telemetry | Interrupt RX, busy/quiet epochs, CRC/request guards, numeric telemetry and PC client implemented; software tests pass | Burst/error/quiet behavior on target pending |
| F: unified product contract | NOT_STARTED | Pending |
| G: lead math | NOT_STARTED (audit prototype only) | Pending |
| H: lead storage | NOT_STARTED | Pending |
| I: endpoint service/runtime | NOT_STARTED | Pending |
| J: production OPEN/SHORT wizard | NOT_STARTED | Pending |
| K: truthful diagnostics/UI | NOT_STARTED | Pending |
| L: integrated qualification | IN_PROGRESS, no release claim | REQUIRES_BENCH_VALIDATION |

The read-only factory service uses the active set as temporary load output while
it is invalid/inactive, scans both slots and rereads the best fully usable slot
with sequence/CRC identity verification. It rejects a newer incomplete or corrupt
set and fails closed if final readback fails. Full factory OSL capture/write
profiles retain their independent service and candidate context.

First unit build after foundation changes: **62,828 B Flash / 14,204 B accounted
RAM**, including the unchanged 2-KiB stack floor. The permanent calibration service
is 2,824 B on Arm rather than 5,776 B. These are development-build measurements,
not an installed image or physical accuracy certificate. Final package sizes will
supersede this checkpoint.

The generic C8 linker remains 64 KiB. `stm32-unit-release` explicitly selects
128 KiB only for UID `54ff70068370525760380567` and preserves its NTC profile.
Manifest/ELF bounds and programmer geometry remain distinct from the factory
register, which must still read 64 KiB on this specific unit. Programming remains
page-by-page, with complete before/after readback and outside-page preservation;
no option-byte changes, mass erase or external W25Q access are introduced.

Relevant host regressions pass for settings, actual settings/W25Q driver adapter
with emulated SPI, load-only calibration, product idle state, feedback and invalid
power values. Settings coverage includes edits during writes, corruption of the
sequence header, schema 2/3 migration, cuts at each transaction step and tick wrap.
The full host suites pass **53/53 factory and 51/51 standard entries**. The
capacity/deployment tool suite passes **64 cases, with five explicit optional
artifact/offline-OpenOCD skips**. Full host/matrix results will be recorded after
the remaining integration work.

See [foundation-checkpoint.json](foundation-checkpoint.json) and the hashed
[foundation evidence archive](foundation-evidence.zip). The actual linked
128-KiB manifest passes inspection, and a simulated full-capacity programming
transaction verifies page-bounded writes, full readback and unchanged data after
the approved pages. This simulation performs no hardware access. A wrong-UID
128-KiB CMake configuration is rejected before building.

Pending physical checks do not block source implementation. They do block claims
of real-world shutdown latency, audible timing, SRAM high-water or complete
product/metrology qualification. The existing OSL records and unit assets remain
unchanged; a new six-resistor campaign is not part of these application fixes.

### Lead model and separate persistence checkpoint

The new pure model composes the existing factory OSL transform with signed series impedance and shunt admittance at 10 Hz, 1 kHz and 10 kHz. It works directly in the measured complex ratio plane, including the factory OPEN pole. 108 independent double-oracle cases exercise the production float implementation (maximum relative algebra error 2.746e-6); this is numerical verification, not physical accuracy qualification.

Lead records use a 100-byte versioned little-endian frame, CRC over header and payload, a last-written commit marker and two independent 4 KiB sectors. The new sectors are immediately BEFORE the old 32 KiB mutable tail. All previous calibration, settings, supplementary-curve and diagnostic offsets remain unchanged. The resource-pack ceiling is now capacity minus 40 KiB. The PC resource uploader checks that ceiling before opening the port; its conservative default is the smallest supported W25Q (2 MiB).

A profile is bound to MCU UID, factory OSL sequence and frame CRC, hardware revision and model. Disable and clear are new committed records; a clear tombstone prevents an older enabled slot from resurfacing. Failed pre-commit saves retain the prior profile. A lost verification after commit is explicitly uncertain: reboot may select the complete new record; no success is fabricated. Load read errors block writes to unknown slots. Host tests cover power cuts at every cooperative boundary, immutable snapshots, contention, CRC/header corruption, tombstones, wrap-safe timeout, unread slots and exact preservation of the legacy tail.

Before first physical use, back up the last 40 KiB and verify that the installed resource pack ends below the new boundary. No existing external Flash has been migrated or overwritten by this implementation checkpoint. Endpoint capture, UI and runtime application remain the next integration package. **REQUIRES_BENCH_VALIDATION**.
