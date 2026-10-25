# Calibration serial battery telemetry — deployed image

Source `f86b7f4e946c4a9fafe605c88a5d476252e9e027` adds capability-negotiated,
read-only nominal battery millivolts to BRINGUP_CAL STATUS over TTL. The image
retains this unit's experimental NTC profile, bound to UID
`54ff70068370525760380567`. All 46 host tests and nine strict embedded profile
builds passed, including existing memory gates. The service context remains
1000 bytes; BRINGUP_CAL Flash is 36,040 bytes and accounted RAM 13,668 bytes.

`bringup-cal.zip` contains the schema2 guarded deployment package, map/HEX,
unit NTC metadata and build/test logs. Verify its hashes against `checkpoint.json`
and use the deployment manifest with the bound UID. Generic production profiles
keep their existing behavior; this capability belongs to the calibration service.
No external W25Q update, OSL installation or acquisition change is required.

The owner reconnected ST-Link and TTL and disconnected the charger. Programming
and full internal-Flash readback passed; COM4 IDENTIFY matches `f86b7f4e` with
capabilities 63. STATUS reports 3.949 V, SAFE/idle, no safety faults, no protocol
errors and charger absent (blockers 8). The earlier pre-charge reading was
3.54740 V: the nominal readings increased by about 0.402 V. No state-of-charge
percentage or calibrated voltage accuracy is inferred. The voltage was read
over TTL, not SWD. No external Flash was programmed; the OSL slots remain blank.

`physical-evidence.zip` preserves sealed programming/readback, preflight,
post-boot identity, raw UART transactions and serial battery evidence. The
historical `serial-before-update.json` demonstrates why the old image required
updating. Charger-present voltage reading remains REQUIRES_BENCH_VALIDATION;
the charger-present protocol case passes the C-backed host tests. The three
high-Z/10-kHz OSL fits remain pending.

See [protocol and CLI](../../../tools/PC_CAL_CAPTURE.md) and
[execution plan](../../../../plans/14B-Calibration-Battery-Telemetry.md).
