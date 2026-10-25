# 14B — Read-only battery telemetry during PC calibration

STATUS: SOFTWARE_VERIFIED — DEPLOYMENT_PENDING_STLINK_CONNECTION

The owner requests battery voltage over TTL, including while charging, without
SWD reads. Current BRINGUP_CAL STATUS lacks voltage; actual COM4 requests confirmed
that only charger/safety/temperature are exposed. A firmware update is necessary;
ST-Link will be needed once for programming, not for subsequent voltage reads.

## Explicit protocol extension

Keep PLC1 API1 and the 32-byte STATUS body. Advertise capability bit5 only when
the snapshot producer supports battery telemetry. With this capability, bytes
30..31 contain little-endian nominal-ADC battery millivolts. Zero means unavailable
(not a measured empty battery); stale/unknown samples must encode zero. Without
the capability those bytes remain zero and legacy decoding is unchanged.
Temperature-valid byte29 is unchanged. This is negotiated use of previously
reserved bytes; no capture, installation or calibration-record format changes.

The shell supplies the already monitored auxiliary ADC snapshot, within its
existing 2000-ms battery freshness bound. STATUS never starts ADC metrology,
excitation, range selection or K1 and is readable with charger interlock active.
The host reports voltage only after identifying this capability. No charge
percentage or accuracy beyond nominal ADC scaling is inferred.

## Expected changes and verification

- app_cal_capture_service.h/.c: snapshot field, capability and STATUS encoding.
- app_cal_capture_shell.c: fresh, bounded millivolt conversion; no new ADC call.
- pc_cal_capture.py: optional capability-negotiated decoding; invalid data rejected.
- Existing real-C protocol fixture and tests: legacy zeros, supported millivolts,
  unavailable sentinel, invalid millivolts, charger block with read-only STATUS.
- Update PC protocol documentation and retain current unit NTC build profile.
- Run host checks and strict nine-profile builds. Program the verified unit
  BRINGUP_CAL image only after the ST-Link is physically available; validate the
  new read over COM4. No external W25Q writes or OSL installation.

The three high-Z/10-kHz RLC fits remain unresolved. This isolated request fixes
power observability before the separate longer-integration investigation.

## Software checkpoint

Source f86b7f4e946c4a9fafe605c88a5d476252e9e027 passes 46/46 host CTests and
all nine strict embedded builds, with the current unit NTC profile explicitly
selected. BRINGUP_CAL uses 36,040 bytes Flash and 13,668 bytes accounted RAM;
the service context remains 1000 bytes. Existing memory gates pass unchanged.
The guarded schema2 image and logs are preserved under
`Firmware/releases/2026-10-25/battery-serial-telemetry/`.

Physical deployment and fresh COM4 voltage remain REQUIRES_BENCH_VALIDATION.
The owner disconnected ST-Link to attach the charger; TTL remains available.
Read-only serial STATUS from running source 0da88bfd confirms SAFE/idle and
charger absent after disconnection, but cannot provide voltage. Do not substitute
the earlier 3.54740-V reading or infer a fresh value from the charger state.
The owner was asked to reconnect ST-Link once for programming; subsequent
voltage reads will use TTL only. No firmware has been programmed in this phase.
