# Production completion execution log

STATUS: SOFTWARE_IMPLEMENTED_VALIDATION_IN_PROGRESS_HARDWARE_DEFERRED

The owner authorized implementation after the
[audit](../production-audit-2026-10-27/README.md).
[Plan 30](../../../plans/30-Production-Firmware-Audit-and-Completion.md) defines
the complete scope. This log records actual progress; unfinished packages are
not release-complete. The physical unit still runs the previously recorded image.
A read-only legacy serial STATUS returned READY with the expected disabled-range blocker. No SWD, programming or automatic DUT measurement was performed. The owner later reported possible battery depletion while away; hardware work is deferred until power is confirmed.

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
| A: explicit unit memory profile | 128-KiB CMake/linker/build/manifest/programmer bounds implemented; load-only OSL saves RAM | Deployment/READY/SAFE/UART verified after watchdog correction; stack high-water pending |
| B: emergency SAFE/watchdog | Direct fatal-output shutdown; watchdog failure latched in both shells | Controlled fault injection/timing pending |
| C: settings | Accepted-start adapter, immutable snapshot, CRC schema 4, timeout and legacy migration implemented | Actual save/reboot/interrupted-write pending |
| D: display/feedback | Idle RANGE blocker fixed; product patterns/queue and armed TIM4 stop implemented | Display timeout, tones and quiet interference pending |
| E: UART/telemetry | Interrupt RX, busy/quiet epochs, CRC/request guards, numeric telemetry and PC client implemented; software tests pass | Burst/error/quiet behavior on target pending |
| F: unified product contract | Shared measurement/UI feature macros independent of OSL provisioning; manifests enumerate features | Unit deployment verified; generic 64-KiB overflow remains an explicit gate |
| G: lead math | Production float composition, pole handling and 108 independent oracle cases implemented | Fixture range/amplitude invariance requires bench validation |
| H: lead storage | Separate 100-byte A/B record, CRC/commit/binding/tombstone; legacy offsets preserved | Exact 40-KiB snapshot/readback preservation verified; physical interrupted-save pending |
| I: endpoint service/runtime | Packed-ADC capture, 7-point groups, bounded OPEN retries, DSP composition/provenance implemented | Physical endpoint quality and holdouts pending |
| J: production OPEN/SHORT wizard | Native menu, OPEN/SHORT/review/save, cancellation, toggle and confirmed clear implemented | Display/buttons/reboot checks pending |
| K: truthful diagnostics/UI | Actual freshness/save state/lead status; bounded result detail rows; honest CRC labels | Visual acceptance pending |
| L: integrated qualification | Host tests and eight-profile matrix passed; sealed unit image archived | REQUIRES_BENCH_VALIDATION; battery/power not confirmed |

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

### Native lead workflow integration checkpoint

PRODUCT now attaches a separate lead service to the existing app-owned acquisition IO. The production Calibration status opens a lead menu with new OPEN/SHORT, enable/disable, clear and back actions. OPEN and SHORT use three frequencies and seven coherent windows per group; candidate coefficients remain inactive until review, explicit save and committed full readback. OPEN begins at 1 MOhm/500 mVrms and can retry 100 kOhm then 10 kOhm after returning SAFE; SHORT uses 10 Ohm/100 mVrms and the calibrated HG path. Each group has a 25-second deadline and at most nine captures, with a maximum of three OPEN range candidates. Manual standard changes and final review occur after hardware teardown, not merely result-ready SESSION_HOLD. This fast workflow has engineering limits and needs endpoint observability/fixture range-invariance validation on the actual board.

The endpoint extractor uses packed ADC samples, calibrated HG transfer, selected-stream clipping and source checks. It accepts a quiet SHORT below ordinary finite-DUT return thresholds. Runtime composition happens in measured ratio space before derived R/C/L; every result carries the applied lead sequence. Trimmed samples require identical factory and lead provenance. Activating/disabling a profile invalidates the cached range hint and prior result. Competing external-Flash work is blocked by the lead mutation owner; serial activation and resource maintenance cannot interrupt an endpoint workflow.

Focused host checks pass for lead math/store/capture, production controller, UI and measurement sessions. The new capture test executes 42 packed-ADC windows, six complete SAFE group teardowns, review/commit, toggle, cancel while connected, OSL invalidation and actual DSP correction of a separate synthetic DUT while preserving the uncorrected factory value. This is synthetic integration evidence, not physical lead calibration acceptance. The provisional unit image linked at 73,244 Flash bytes and 15,164 accounted RAM bytes before subsequent diagnostics/profile work. **REQUIRES_BENCH_VALIDATION** remains for physical endpoint captures, contact behavior, comparison DUTs and reboot persistence.

### Shared production contract and truthful presentation

The generated `WTK_PRODUCT_MODERN` contract is independent of factory provisioning. Both provisioning choices share continuous K1 ownership, three-frequency classification, seven-point estimates, selected-channel clipping semantics, rich result provenance and row caching. The provisioning flag controls only the full factory wizard/load-only implementation. New deployment manifests enumerate effective feature macros and inspection checks the enumeration against the sealed build config. Historical manifests without the new optional enumeration remain inspectable.

The current full PRODUCT requires the verified unit's 128-KiB engineering preset; generic 64-KiB builds retain their original gates and may fail them. No silent generic memory expansion or reduced measurement behavior is used to make a preset fit. Generic BRINGUP/BRINGUP_CAL remain the diagnostic/recovery route. Host provisioning ON/OFF checks expose the same production behavior while retaining separate factory wizard tests.

Diagnostics now show actual sensor freshness, settings state/errors and lead status. Result detail rows keep full range/RET/attempts, AVG7/3, coverage and both sequence IDs without truncation. Resource labels describe header/table CRC verification, not a nonexistent full-payload qualification. The read-only UART Flash snapshot command is bounded to 108-byte reads, guarded by physical SAFE and bus ownership, and cannot erase/program/start acquisition. Its PC client archives the 40-KiB tail and resource header with identity, raw protocol evidence and hashes. Physical snapshots/deployment remain pending at this checkpoint.

### Hardware deferral

The owner confirms TTL/COM4 and ST-Link with a nominal 470-uH inductor, then reports possible depleted battery while away. Do not flash, reset or start acquisition based on that old fixture confirmation alone. Complete software/artifact/Git work now. Next physical session: confirm stable charged power, preserve tail/assets, flash the sealed UID-bound image, verify READY/SAFE and real UART battery/NTC/OSL, then perform scoped tests. No hardware acceptance is inferred from host tests.

### Optional K1 diagnostic memory correction

The final matrix exposed a 608-byte overflow in the optional 64-KiB K1 pulse image. Size-only compiler switches did not solve it and were not adopted. The once-only empty-fixture relay diagnostic now explicitly excludes the unrelated factory campaign solver/commit command family, referring those requests to BRINGUP_CAL. Normal BRINGUP and the dedicated factory service retain their calibration paths. Safety authorization/interlocks, measurements, NTC profile, assets and boot behavior are unchanged. The scoped image links at 61,080 bytes before final commit metadata; binary-composition checks verify both the unavailable message and solver exclusion. No threshold or generic Flash gate was relaxed.

The external-supply diagnostic also exceeded 64 KiB by 412 bytes. It now shares the explicit factory-campaign exclusion; its 100-mVrms-only envelope cannot capture the complete 33-key factory set. Acquisition/sensors/ADC tracing and every admission/abort guard remain present. Full factory provisioning stays in BRINGUP_CAL. The lead menu also now drains an accepted settings write before enabling a capture: a regression test prevents entering that menu from stranding the previous settings owner.

### Final optional-curve and CI integration correction

Review of the actual shell found that WCRV readback during a held quiet session was correctly refused by Flash arbitration, but therefore could never supply that optional correction. WCRV-enabled builds now prefetch a fully CRC/commit/binding-verified selected frame at boot into an immutable 33-condition coefficient cache, using the existing shared workspace for wire bytes. DSP reads RAM only. The cache is approximately 1.74 KiB, allocated only with the optional runtime ON; its final target map must pass the existing RAM gate. Missing keys stay absent, OSL changes invalidate the cache and corrupt prefetch cannot claim success. Default production remains OFF. Combined WCRV/lead correction remains explicitly unavailable pending qualification.

The forensics CI workflow is updated to build the explicit unit/recovery matrix and retain generic 64-KiB product overflow as an expected negative check. Historical baseline evidence stays separate. Host math-oracle/capture executables explicitly link libm on non-MSVC platforms; embedded dependencies are unchanged. The CI environment directory is ignored so source fingerprints remain stable. These are software/build checks, not hardware qualification.

The optional cache regression covers a complete 33-condition qualified frame, including sparse condition index 35, with the Flash reader deliberately blocked during cache lookup. It also checks absent keys, OSL sequence invalidation and corrupted prefetch. No sensor or component value is synthesized in the embedded result path.

The complete-domain test also exposed an older WCRV table overflow: 33 supported keys use sparse condition indices up to 35. Both the store index and optional RAM cache now reserve the complete 36-position domain; unsupported keys remain rejected. This does not change the on-Flash schema or condition mapping.

Software closure checkpoint: host CTest passed 59/59 with factory provisioning ON, 59/59 OFF, and 62/62 with optional WCRV ON. The optional unit target links at 77,976 Flash bytes and 17,052 accounted RAM bytes, including 2,052 bytes reserved/aligned stack (356 bytes below the unchanged hard RAM gate). Default production does not include this optional cache. The first Python run passed 531 cases with 113 unavailable optional/bridge skips; the final evidence must distinguish that run from a bridge-enabled rerun. No physical tests or programming were performed.

## Final software delivery

The [sealed release and indexed evidence](../../../Firmware/releases/2026-10-27/production-completion/README.md) are archived at clean runtime source `603f6715307c2b11f48b2f0470622139ca951728`. All eight supported profiles pass; generic full PRODUCT correctly stops at its unchanged 64-KiB memory gate. The release and forensics unit binaries match exactly: 75,092 Flash bytes, 13,156 static RAM bytes, 2,048 reserved stack bytes, 15,204 accounted RAM bytes. Physical Flash remainder is 55,980 bytes; hard-budget margins are 54,956 Flash and 2,204 RAM bytes. Optional WCRV's map is separate and is not the delivered configuration.

Final host results: 59/59 factory, 59/59 standard, 62/62 curves. The bridge-enabled Python rerun passes 531 cases with 38 explicit skips; its raw log is distinct from the initial bridge-disabled run. Strict GNU warning checks exposed implicit double promotions in the independent host oracle; test-only follow-up `11add6a` makes those conversions explicit, passes all three new test translation units under warning-as-error syntax checks, and executes the oracle again. Embedded runtime code is unchanged after the image source.

The sealed archive was extracted and inspected again. FakeSWD alone verifies page bounds/readback/outside-page preservation and wrong-UID rejection. No target was flashed, reset or measured, no fresh battery value was obtained, and external calibration/resources were not read or modified. Runtime stack high-water, cooperative latency and all listed physical acceptance remain pending. This closes software preparation, not plan 30's physical release gate.

## First deployment: watchdog startup correction

The owner confirms power is still present and explicitly authorizes programming to obtain numeric UART battery telemetry. The sealed 603f671 image programs/readbacks exactly; UART reports 3.701 V with actual age 269 ms, but boot correctly latches WATCHDOG fault 0x100. K1 remains SAFE, range disabled and no acquisition starts. Live read-only non-halting register evidence shows LSI ready, PR/RLR still at reset defaults and PVU/RVU both pending. The existing BSP waited for those update flags before enabling IWDG; the newly checked return exposes the earlier ignored failure.

The correction matches the pinned STM32CubeF1 HAL_IWDG_Init ordering: enable IWDG, unlock/write PR/RLR, wait boundedly for updates, then reload and mark started. The fault latch is retained. An actual-BSP register-model test covers successful ordering, missing LSI and stuck update flags; only completed initialization permits normal servicing. The three new checks and related safety/load/telemetry regressions pass. Physical boot after this correction, independent watchdog timing and all broader qualification remain separately tracked. See the [ST HAL source](https://github.com/STMicroelectronics/stm32f1xx-hal-driver/blob/master/Src/stm32f1xx_hal_iwdg.c) and the pinned copy under third_party.

### Corrected physical handoff

The [6bb3a68 correction and indexed physical evidence](../../../Firmware/releases/2026-10-27/production-completion/watchdog-startup-fix/README.md) supersede the initially prepared image. Programming/full internal readback match the new binary exactly. One controlled SAFE_BOOT reaches READY, fault mask zero, K1 commanded SAFE, range disabled and zero measurement sessions. Actual UART battery is 3.688 V, then 3.689 V in the preservation snapshot at 19:32 America/Sao_Paulo; no numeric battery value is read via SWD. Non-halting register reads show PR=4, RLR=1250 and SR=0. This verifies startup/configuration, not forced watchdog-expiry timing.

Before/after UART snapshots match the entire 40-KiB mutable tail exactly, including the original 2,760-byte OSL frame and unchanged resource header. The resource pack ends at 104,499, below the actual 4-MiB device limit 4,153,344. No external Flash write or new calibration capture occurs. The final eight-profile matrix passes with clean 6bb3a68 provenance; current optional-curves host suite passes 65/65 and focused checks 6/6. All broader physical UI/settings/native lead/holdout/fault-timing and stack/latency acceptance remains pending.
