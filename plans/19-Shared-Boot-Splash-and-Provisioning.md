# 19 — Shared three-second boot splash and resource provisioning

Status: IMPLEMENTED_AND_PROGRAMMED. REQUIRES_BENCH_VALIDATION for actual visible
TFT appearance/timing; the owner explicitly deferred that check. Assigned2026-10-24,
physical delivery completed2026-10-25.

Merge the owner's new PNG from origin/main without discarding bring-up work.
Keep the existing READY art and add the personalized240x320 splash as a distinct
optional schema2/API4 RGB565 RLE resource. Reuse the existing bounded image
decoder, resource CRC/bounds checks and SPI ownership; no framebuffer or heap.

All PRODUCT, factory PRODUCT, BRINGUP diagnostic variants and BRINGUP_CAL shall
draw the splash and hold the completed image for3000ms. BRINGUP_CAL gains a
boot-only display phase then returns to its existing serial-only service. Boot
graphics must be cooperative: sensors, watchdog and SAFE handling keep running;
no acquisition, range transition, relay pulse or calibration/resource mutation
is admitted until the splash completes. Missing/corrupt assets or failed display
must end the optional presentation with bounded progress and retain safety gates.
Keep the existing default display orientation and current calibration identity.

Prerequisites: plans03/08/17/18 implementations, source-bound build tools and
the first-board resource upload/readback evidence. Read current safety/UI/storage
and bring-up documentation before firmware edits. Expected files: image source/
generated resource/manifest and IDs, shared splash UI controller, application
boot integration in both shells, CMake/profile composition checks, host tests,
paced serial uploader and documentation, execution evidence and build bundles.

Check all nine existing embedded profiles/variants within guaranteed64KiB/20KiB
and project budgets. Standard BRINGUP was previously within48bytes of Flash;
measure the new link before selecting a memory response. If the experimental DC
pilot prevents the requested splash from fitting, make that experimental feature
an explicit separate opt-in, retain all ordinary AC/calibration/safety commands,
document the change and fail builds that exceed hard gates. Do not silently
delete active metrology or safety behavior or increase silicon/linker limits.

Add supported byte pacing/flushing, post-BEGIN settling and safe DTR/RTS opening
to resource upload tooling, addressing the observed first-board frame truncation.
Validate frames/ACKs and record final completion; transmission alone is not a
persisted-byte acceptance. Back up the complete erase span and mutable tail,
use a verified receiver-capable image, install the new pack and read it back
completely through the existing W25Q driver. Confirm mutable data unchanged,
post-reset PRODUCT resource admission and final updated BRINGUP SAFE boot.

Acceptance: meaningful host timing/failure/wraparound and image-ID tests,
resource conversion/build/inspection, all relevant host/PC checks and all nine
embedded builds, traceable clean source/binary identities, physical resource
readback, SAFE final firmware and Git publication. Actual visible splash timing
needs the owner with the display connected; SWD/software evidence is distinct
from visible pixels. Other electrical/metrology release gates remain pending.

Implementation status: shared boot controller and both shells integrated; optional
resource ID0x00030006 added without an API bump; indexed PNG conversion preserves
native RGB565. The pack is104499bytes, SHA256
d5e6f558173a67f3a30212f4951be11db7893121e12fc210d3b06df30c2cd78f,
with seven entries and26 erased4KiB sectors. Temporary product receiver admission
from idle presentation uses existing application policy and guards, with TFT work
deferred during update. Paced upload records ACKs and stops on ambiguity.

Memory response: first BRINGUP link with DC was67804bytes, exceeding64KiB by2268.
The default now excludes the experimental DC pilot (explicit opt-in remains),
retains AC/calibration/safety commands, and resets the complete console context
in one deterministic initialization. Default BRINGUP development link is65532
bytes, with15384 accounted RAM; hard gates and silicon limits are unchanged.
Final clean-source nine-profile build and physical delivery remain pending.

Software checks before the source-bound matrix:44/44 host CTest checks pass;
489 Python tool tests pass with99 environment-dependent skips. New coverage
includes full-image3000ms hold/wraparound, bounded validation/clear, deferred I/O,
CRC/missing/display timeout/fault exits, exact owner-PNG RGB565 runs, indexed
palette transparency/bounds, safe serial line opening, missing-ACK stop and idle
maintenance admission. Physical preflight observes battery about3.81V/OK,
charger ABSENT, K1 SAFE, range DISABLED, residual SAFE and zero faults.

The first full matrix exposed missing boot-only sources in the exclusive CAL list
and Flash overflows in bench/K1 variants. CAL now explicitly includes resource/
image/boot, geometry/color and Flash-access policy dependencies, while excluding
PRODUCT UI/settings/console. Bench/K1 retain every command and use log level3 plus
minimal SAFE_BOOT, reducing unsolicited logging only. Development checks pass for
these corrected compositions; repeat the full matrix on the corrected clean SHA.

Corrected clean source52df01cdd341 passes all nine strict-warning embedded builds.
The initial physical upload at1ms byte pacing failed at the first header (BAD_MAGIC,
no erase/program operation started). The tool stopped without replay. A live SAFE
snapshot confirmed update IDLE/NOR idle; after a separate controlled reset, the
reviewed new session uses10ms per BEGIN byte, then2ms per byte while TFT redraw is
suspended. These are now the supported defaults; the earlier1ms default is not
qualified. The firmware binary source remains52df01cdd341; this tooling correction
and later physical evidence are tracked in subsequent commits.

The actual running-core boot trace found full-image HOLD for3000ms but no full
CLEAR: ILI9341 fill returns OK for each successful chunk, not only completion.
Correct the controller to follow fill.active, and correct the host fake to model
that contract. The revised timing/clear host test passes. The updated BRINGUP link
uses65536bytes; unused DC-only status branches are excluded when DC is disabled.
No safety/AC/calibration commands were removed. Rebuild the full source-bound
matrix again before final programming. The resource pack itself needs no change.

Final acceptance: clean firmware source adfce3c59aac085a95b7fa487fed5f836dca21ec
passes all nine embedded builds and project/silicon gates. Standard BRINGUP uses
65536 Flash bytes and15380 accounted RAM; no Flash headroom remains in this
engineering image. Final host checks:44/44 CTest passed; Python490 run,391 passed,
99 skipped. Transport/resource/deployment tooling checks also pass.

The complete104499-byte seven-entry pack matches physical W25Q readback exactly;
sector padding is erased and the mutable32KiB tail is byte-identical. All873
frames were acknowledged, including END/COMPLETE. PRODUCT admitted the pack after
reset with invalid OSL still blocked. Final updated BRINGUP was programmed and
read back over the entire64KiB, then independently booted after external reads:
K1 SAFE, range DISABLED, charger ABSENT, zero faults and invalid calibration.
Running-core sampling observed complete-image HOLD then full cooperative CLEAR,
with first observed clear3012ms after the visible timestamp and no commanded
measurement outputs. No DUT capture, relay pulse or calibration/settings mutation
was requested. The initial clear-contract issue was corrected and regression-tested.

[Delivery report](../docs/review/bringup-2026-10-25/Boot-Splash.md) and
[bundles](../Firmware/releases/2026-10-25/boot-splash) preserve source/binary IDs,
physical transfer/readback and timing evidence. Visual TFT validation remains
pending by the owner's choice; software/SPI state is not substituted for pixels.
