# 14B — Read-only battery telemetry during PC calibration

STATUS: IN_PROGRESS — OWNER_REQUESTED_SERIAL_READING

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
