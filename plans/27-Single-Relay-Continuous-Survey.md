# Single-relay continuous survey with real power supervision

STATUS: IMPLEMENTED_DEPLOYED_BASIC_INDUCTOR_CHECK_PASS_REQUIRES_PHYSICAL_QUALIFICATION

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

Software checkpoint: 49/49 factory and 47/47 standard host tests pass, as do all
nine strict UID-bound embedded profiles on clean source 40a0b420ef60. The actual
auxiliary FSM is integrated into the synthetic continuous hardware test; failure
coverage includes 21 monitor/ADC/time/power/cancel contract variants, capture
limits and mode locks. The installed factory image is 61,136 B Flash / 17,104 B
accounted RAM and passes the unchanged hard gates.

Internal programming and full readback match. COM4 captured one SAFE_BOOT and
nonhalting live DAP observed READY/SAFE with zero sessions/captures, active OSL
sequence 1/all 33 and no fault. No W25Q write or SWD numeric battery read occurred.
The factory-size register returned 64 KiB. Readable all-FF probes above that
range do not qualify writable additional capacity. Current unused declared
Flash is 4,400 B; no destructive extra-capacity test was performed.

New owner-triggered C/L captures, actual power-refresh telemetry and physical
single-engagement/release witness remain pending. Prior plan 26 capacitor/inductor
results are historical, not acceptance of this image. No final physical acceptance
or independent accuracy/polarized/10-mF qualification is claimed. See the
[delivery and evidence](../Firmware/releases/2026-10-27/single-relay-continuous-survey/README.md).

Plan 28 follow-up: source 9ecaa696ec78 adds guarded serial OK without changing
the metrology profile or estimator. One actual owner-authorized remote trial on
the unchanged nominal 470-uH inductor returned 472.656 uH, seven accepted samples,
coverage 7/7 and five actual power refreshes. Eleven captures (five search plus
six primary extras) retained the initial permit and finished SAFE with no fault.
The firmware connected-entry/final-guard interval is 1,617 ms. Owner absence
prevents a physical contact-count witness; no new capacitor or accuracy test is
claimed. See the [serial-control delivery](../Firmware/releases/2026-10-27/serial-ok-control/README.md).
