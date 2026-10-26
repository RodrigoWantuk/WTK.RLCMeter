# Evidence admission, relay sessions and incremental menus

Source: 7550d92dcf99c13d846d0d7762b30e2854ef030e. Unit:
54ff70068370525760380567. Implementation and unattended deployment are complete;
physical session/UI/metrology qualification remains REQUIRES_BENCH_VALIDATION.

Classification and frequency-trend evidence now require a successful finite
capture and usable, unclipped selected RET with finite peak >= the existing
2-mV weak-return threshold. Every raw attempt stays in history. The actual
four-attempt nominal 0.47-uH history is replayed: its early weak capacitive
candidates no longer outvote its later resistance-dominated observations.
No R/C/L threshold is lowered, no raw sign is clamped and no sub-uH accuracy or
successful 0.47-uH reading is claimed. An input audit confirms all selected
returns in the archived positive 10-nF/470-uH sessions exceed this threshold;
this audit is not a new physical capture.

PC-provisioned PRODUCT retains K1 through one bounded automatic search after
the original fresh initial permit is consumed. It allows at most six captures,
a 500-ms cooperative connected deadline and the original battery-evidence expiry.
Between captures DMA is stopped, excitation is neutral/settled, and quiet,
auxiliary ADC and exclusive workspace ownership remain held. Only the owner's
neutral range transition can admit DISABLED/TRANSITIONING; the range service
still disables RANGE_EN before address changes and waits for settling. No permit
is reissued or reused to energize K1 again. Live charger/unknown charger, faults,
DMA, invalid range/ID, timeout, lost ownership and cancellation fail closed.

Final publication waits for K1 SAFE/release guard, range/excitation OFF, restored
auxiliary ADC, quiet and workspace release. A late cleanup fault revokes the
provisional result. The initial IO contract now rejects missing live charger
supervision. Fixed/10-Hz/DC/calibration captures and legacy wizard PRODUCT keep
SAFE per capture. This is continuous authorization for an unchanged, unpowered
DUT; no fresh residual data is fabricated while K1 disconnects the SAFE network.
Deadline checks are cooperative; actual release latency/transients need bench
validation. No new measurement starts automatically at boot.

The factory UI caches nine exact completed rows plus one in-flight row, without
a framebuffer. Main/maintenance menu navigation paints old/new selected rows;
brightness/timeout/sound/language and normal status/about screens skip unchanged
rows. Shorter values and removed save warnings erase their full old extent.
Updates during a glyph/clear converge on the latest generation. Only completed
rows are cached; failed writes rebuild the baseline. Language/resource/screen
transitions clear appropriately, including READY's bottom hint. Fonts taller
than the minimum menu pitch use a full repaint to protect neighboring rows.
Quiet suspends all clears/glyphs; editor persistence remains separate from paint.
READY coalescing, normal TFT clock, 75% saved brightness and boot splash remain.

The row cache and session holding are enabled in PC-provisioned PRODUCT only.
The full legacy-wizard image exceeded its existing Flash gate with the extra UI
path; its previous bounded renderer is retained instead of raising that gate or
removing features. Common evidence/language/error recovery fixes remain shared.
Full BRINGUP uses INFO default logging, omitting unsolicited verbose boot/button
logs while preserving commands and safety diagnostics.

## Validation and memory

Standard host CTest 46/46 and factory 47/47 pass, plus a focused final diagnostic
state test. Fault scenarios include held charger/unknown state, DMA, range,
neutral transition timeout, six-capture/time/battery limits, timer wrap, lost
K1, invalid request, cancellation, late ADC restore and impossible app handoffs.
Workspace tests prevent reacquiring/stealing the held lease. UI tests measure
two changed rows for menu navigation and one for numeric edits, and cover stale
warning removal, quiet/interruption/error recovery, tall fonts and invalidation.
These are host/fake-I/O tests, not physical relay/display qualification.

All nine strict UID-bound embedded profiles pass existing size/composition gates:

| Profile | Flash bytes | Accounted RAM bytes | Session / row cache |
| --- | ---: | ---: | --- |
| factory-release | 56260 | 16040 | ON |
| bringup | 63828 | 15380 | OFF |
| product-debug | 61280 | 15200 | OFF |
| product-release | 64036 | 15520 | OFF |
| factory-debug | 55880 | 15960 | ON |
| bringup-cal | 37572 | 13744 | OFF |
| bench-supply | 65272 | 15468 | OFF |
| k1-test | 65392 | 15396 | OFF |
| peripheral | 14628 | 12252 | OFF |

Factory PRODUCT uses 56,260 B Flash and 16,040 B accounted RAM, within the
existing 64-KiB silicon / profile Flash and RAM gates. Its two optimizations
retain fixed storage and no heap. Some legacy/diagnostic variants remain close
to their gates; inspect firmware-builds.json before adding further code there.
The UID-bound experimental NTC profile is validated in every image.

## Actual unattended board checkpoint

Internal MCU programming and complete readback match SHA-256
1fcf311bad8d9bd2da6a363ca5aeb5751cbdf566970f484d910cd9e9c34a69d0
(56,260 B). Controlled boot produces one passive TTL SAFE_BOOT banner. Actual
metadata confirms READY, K1 commanded SAFE, range/excitation OFF, no faults,
resources OK, OSL sequence 1/all 33 records active, hardware IDLE, zero sessions/
captures, no connection/quiet ownership and a free workspace. No reset-triggered
or debugger-injected DUT measurement occurred. No battery voltage was read via
SWD. The connected fixture and buttons were not touched by the owner during this
work. No external-Flash erase/program was requested; calibration/assets were
preserved. ADC/BSP timing, drivers, DSP impedance/calibration math and unit NTC
coefficients are unchanged by this source change.

Actual single-relay-cycle measurement behavior, switching transients, settling/
phase consistency and physical menu appearance remain pending owner bench work.
The existing PRODUCT serial link has no supported measurement-trigger command,
so passive serial boot plus SAFE metadata is the physical evidence here.
No further resistor calibration campaign was performed or requested.

firmware-images.zip contains all nine images/maps/ELFs and the guarded PRODUCT
manifest. validation-evidence.zip contains checks, exact internal-Flash readback,
compiler-derived metadata layout, passive UART and bounded deployment procedures.
See checkpoint.json and [plan 24](../../../../plans/24-Measurement-Evidence-Session-and-UI.md).
