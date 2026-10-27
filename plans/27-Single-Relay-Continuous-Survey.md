# Single-relay continuous survey with real power supervision

STATUS: IN_PROGRESS

The owner rejects plan 26's repeated K1 SAFE/re-entry cycles and reasserts one
engagement and one final release for the entire three-frequency, seven-sample
measurement. Preserve the seven-valid-point estimator, classification, calibration
and ownership contracts. This supersedes the segmented factory path only through
an explicit continuous option.

An ordinary fresh initial permit authorizes one K1 entry for one unchanged,
unpowered passive DUT. Keep neutral guarded ranges, amplitude restrictions,
coherent profiles, quiet/workspace ownership, charger/fault/DMA/cancel checks and
SAFE-before-publication. Fixed diagnostics/calibration and legacy fast requests
retain existing behavior.

Bound continuous surveys to twelve search plus eight statistical attempts and
a 30-s hardware connected deadline, inside the 60-s app limit. Battery evidence
retains its 1,000-ms expiry and is renewed only from actual ADC conversions. The
old 500-ms/six-search contract stays unchanged for nonopted requests. This is not
a blind long ADC pause.

Reuse ADC restore/acquire while no coherent DMA window is active. During sine
settling or immediately before capture, lend the ADC for a paused-auxiliary
power-only group: four VMID and four VBAT conversions, no OV/NTC or residual
publication. Main-loop sampling advances while K1/range/source/quiet stay owned.
Check about every 250 ms outside capture; a 400-ms coherent window may finish before
another check, starting with refreshed evidence. Transfer/sampling has a 100-ms
bound. Missing/invalid/critical/unknown battery, bad VMID, stale/future/preceding
timestamps, ADC/restore/reacquire faults and all live hard blockers abort.

The excitation timer continues during ADC reconfiguration. Preserve the original
settle deadline rather than restarting 800 ms after every monitor check. Reacquire
the identical calibrated dual-ADC profile before capture; never sample power
concurrently with metrology DMA. No new BSP register path, sample profile, DMA
topology, OSL model/coefficient or NTC constant is introduced.

Residual evidence stays UNKNOWN throughout these power-only updates; do not
fabricate fresh SAFE readings or another permit while K1 remains connected.
Final/cancel/fault returns SAFE, restores normal auxiliaries and releases quiet/
workspace. Track real power-refresh count/timestamps without debugger battery
voltage reads. Monitoring failure has no unsupervised fallback.

Expected files: factory flag; metrology callbacks/states/request; auxiliary
power-only sampling; production adapter/app opt-in; auxiliary/hardware/app tests;
docs/PR and release evidence. Keep static buffers and inspect the map under the
existing 17,408-B hard accounted-RAM gate with 2-KiB stack reservation.

Test only VMID/BAT channels, real four-sample averages, residual invalidation,
paused/active exclusion and ADC failures; full 10-Hz/fast/frequency/statistical
sequences with one K1/initial permit; preserved settling and capture exclusion;
battery/VMID/timestamp/charger/DMA/ownership/cancel/cleanup faults; wrap/count/time
bounds and all legacy regressions. Run nine strict unit-bound builds and host
tests, preserving all 33 OSL/NTC/assets.

Deploy only after SAFE live preflight; record UART clean boot and nonhalting
identity/image/sequence-guarded metadata. Owner initiates physical tests. Confirm
seven admitted points, three frequencies, actual power refreshes, final result
and SAFE cleanup; request a single-engagement/release witness. Basic C/L checks
do not qualify accuracy, contact/abort timing, polarized envelope or 10-mF endpoint.

Software checkpoint: both full host suites and all nine strict embedded builds
must pass on clean committed source. Final physical acceptance is pending.
