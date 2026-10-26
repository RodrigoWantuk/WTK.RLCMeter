# Measurement evidence, bounded relay sessions and incremental menus

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

## Authorization and baseline

The owner explicitly requests plan 23 follow-ups: admit trustworthy classification
evidence, keep K1 energized through a bounded automatic search, and improve menu/
settings redraws. The board remains connected with TTL, but no fixture change,
button press or display inspection is available. Do not invent those observations
or require another resistor calibration campaign. Baseline is ac850b9; active
engineering calibration sequence 1 has 33 records and a UID-bound experimental
NTC profile. The exact baseline 0.47-uH attempts and SAFE teardown are archived.

Mandatory architecture/safety/pinout/decision/detailed-design docs and plans 07,
20--23 apply. UI work also follows docs/06. This is a bounded successor to the
completed search change; metrology qualification remains open.

## A. Evidence admission

Keep every attempt for diagnostics. Classification and frequency trend should
ignore failed/nonfinite/clipped captures and selected RET paths that are unusable,
clipped or below the existing 2-mV weak-return policy threshold. Do not discard a
strong capture merely because its classification is inconvenient. Keep existing
R/C/L thresholds, raw signs, primary quality ordering and publication gates.
Replay the actual four-attempt history: early weak capacitive candidates must
not outvote the later usable resistive evidence; no valid 0.47-uH claim follows.

## B. Explicit bounded session authorization

Implement an optional hardware-owned continuation of a single initially issued
and consumed permit, rather than reusing that permit to energize K1 again.
Enable it for PC-provisioned PRODUCT; fixed BRINGUP/calibration captures and
legacy wizard PRODUCT retain individual SAFE transactions. Host factory tests
cover the continuation paths. This limits code cost in the legacy full-wizard
profile and keeps calibration acquisition unchanged.

- An automatic request explicitly opts in; 10-Hz/DC captures do not opt in.
- Initial K1 entry still requires real fresh residual SAFE, battery, charger,
  range, clock and fault checks and the existing 30-ms operate guard.
- Authorization lasts at most six captures and 500 ms from K1 entry, additionally
  bounded by expiry of the original battery evidence (1,000-ms existing limit).
  Deadlines are checked by the cooperative hardware owner; watchdog/reset retain
  fail-safe behavior. No blocking UI/storage/UART is allowed in this window.
- Between captures ADC/DMA is stopped, excitation is NEUTRAL and settled. K1,
  quiet mode, paused auxiliary ADC and workspace ownership remain held while
  bounded DSP/policy chooses the next condition. Raw UART dumps remain forbidden.
- Only the owner's RANGE_REQUEST/RANGE_WAIT states may tolerate intentional
  DISABLED/TRANSITIONING range state, with neutral excitation. The existing range
  service disables RANGE_EN before address changes and enforces settling.
  INVALID, wrong settled ID, timeout or I/O failure aborts.
- Charger/unknown charger, faults, DMA errors, lost K1 ownership, authorization
  expiry, invalid continuation or cancellation cause bounded emergency cleanup.
  No new software permission is synthesized from stale residual measurements.
- Final result publication waits for neutral excitation, K1 SAFE/release guard,
  disabled range, restored auxiliary ADC and quiet/workspace release. Cleanup
  failures invalidate the provisional result. No held session survives reset.

The SAFE residual network is physically disconnected from the DUT in MEASURE;
the authorization is continuous for one bounded search, not permission to swap
parts or connect energized DUTs. A later independent session needs fresh SAFE
residual qualification. Physical switching/abort timing remains
REQUIRES_BENCH_VALIDATION; host tests cannot qualify analog transients.

## C. Incremental menu/settings rendering

Enable the row cache in PC-provisioned PRODUCT, alongside session holding.
The legacy full-wizard PRODUCT exceeds its existing 63-KiB gate with both UI
paths retained (64,768 B measured). Keep its original bounded renderer rather
than raise the gate or remove features; common language/error recovery fixes
still apply. Actual first-unit production is the PC-provisioned profile.

Use a bounded line cache, not a framebuffer. For the same state/page/language,
skip unchanged text/color/geometry and clear/redraw only changed row rectangles.
Erase old longer values and removed warning rows. Cache only completed rows;
updates during a glyph/clear must converge on the latest generation. A screen,
language or resource-mode transition and any failed/interrupted screen paint
must invalidate relevant cache state. Quiet still suspends all transfers.
Menu selection should repaint old/new selected rows; brightness/timeout edits
should repaint their affected row. Preserve full clears for screen transitions
and the READY image rules. No repeated W25Q save per numeric redraw.

## Files and acceptance

Expected changes: measurement_engine + tests; optional hw_metrology_measure
session state/APIs + app_measurement_session lifecycle + app_shell adapter and
tests; ui_product context/renderer + transfer-count/interruption tests;
CMake/config profile flag; architecture/safety/UI/calibration docs and plan index.
Compact unsolicited BRINGUP verbose boot/button logging to INFO if needed to
fit the guaranteed C8 Flash; keep all diagnostic commands and safety messages.

Run standard/factory host suites, strict unit-bound embedded matrix and memory
checks. Preserve calibration/resources, program the verified factory PRODUCT,
read back exact internal Flash and inspect passive serial boot and real SAFE
metadata. No new measurement is triggered by reset; any unattended measurement
requires an existing supported command, not debugger state injection. Archive
evidence and push changes. Record physical button/display/relay-session behavior
as pending when unavailable. Do not mark independent accuracy qualified.

## Implementation review notes

The row cache also covers maintenance and normal diagnostic/status/about screens;
errors, safety, measurement/result and wizard transitions retain their own paint
rules. Fonts taller than the minimum 28-pixel row pitch fall back to full repaint.
Provider invalidation and generation-zero initial rendering are covered explicitly.

All actual board adapters already provide live charger state; hardware IO init
now rejects a missing callback rather than silently lose runtime supervision.
The workspace adapter was corrected to retain, not reacquire, the metrology lease
between held captures; a lost lease aborts. Cancellation/impossible app handoffs
cannot abandon an active hardware owner. New tests exercise provisional-result
revocation and between-attempt cancellation. One early two-capture synthetic
vector accidentally required a third rerange; the fixture now keeps both ideal
capacitive frequencies within the existing reference ratio window.

## Software validation snapshot

Standard host CTest 46/46 and PC-provisioned factory 47/47 pass with strict host
warnings. A subsequent diagnostic-string-only addition has a passing focused
app-session test. New tests cover all four captured 0.47-uH attempts, selected
path clipping/nonfinite/weak evidence and trend filtering, four continuous
captures through the actual range FSM (including timer wrap), 16 held fault
cases, six-capture expiry, neutral transition timeout and slow-capture fallback.
App tests cover provisional-final revocation, late cleanup failures, cancellation
between attempts, impossible handoffs and exclusive workspace retention. UI tests
cover old/new selection rows, shorter values, warning removal, quiet/interrupted
paints, I/O failures, tall-font fallback and provider/generation invalidation.

Archived positive 10-nF/470-uH evidence is also inspected: every selected return
in those four earlier sessions exceeds the retained 2-mV threshold. This input
eligibility audit is not a new physical capture or independent accuracy test.
Fresh committed nine-profile builds and real passive boot/SAFE verification are
next. No new measurement, fixture change or button interaction has occurred.

## Delivery checkpoint

Committed source 7550d92dcf99c13d846d0d7762b30e2854ef030e passes all nine strict
UID-bound embedded builds and memory gates. The PC-provisioned PRODUCT image is
56,260 B Flash / 16,040 B accounted RAM. It is programmed and readback verified;
passive COM4 boot and actual READY/SAFE metadata confirm active OSL sequence 1,
all 33 records, no faults, no measurement/capture/held ownership and a free
workspace. No new DUT measurement or owner interaction occurred. Actual relay
session behavior, analog settling/phase and visual button/menu validation remain
REQUIRES_BENCH_VALIDATION. Implementation/software deployment acceptance is met;
independent hardware/metrology qualification is not marked complete. See
[delivery](../Firmware/releases/2026-10-26/session-and-menu/README.md).
