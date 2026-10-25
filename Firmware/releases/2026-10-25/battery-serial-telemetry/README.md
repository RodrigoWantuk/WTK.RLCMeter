# Calibration serial battery telemetry — prepared image

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

Status at preparation: NOT_DEPLOYED_STLINK_DISCONNECTED. The running `0da88bfd`
image was queried over COM4 after charger disconnection and has no serial voltage
field. No fresh voltage was inferred from its charger state or old readings.
One internal-Flash update remains necessary before physical serial validation.
Battery telemetry with/without the charger is REQUIRES_BENCH_VALIDATION until
the updated image is running. The three high-Z/10-kHz OSL fits remain pending.

See [protocol and CLI](../../../tools/PC_CAL_CAPTURE.md) and
[execution plan](../../../../plans/14B-Calibration-Battery-Telemetry.md).
