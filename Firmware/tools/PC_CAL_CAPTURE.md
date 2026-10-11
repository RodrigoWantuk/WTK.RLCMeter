# A06.2a calibration capture

For a complete resumable operator workflow use the [guided campaign](PC_OSL_CAMPAIGN.md).
Individual capture commands below remain supported.

Build `cmake --preset stm32-bringup-cal` and `cmake --build --preset stm32-bringup-cal`
from Firmware. This development image runs an actual USART1 service at 115200 8N1.
It has no product UI, ordinary measurement command or resource receiver. A06.2b
adds the [installation service](PC_OSL_INSTALL.md) in the same development profile. PRODUCT and BRINGUP keep their existing wizard, boot gate and resource behavior.
The PCB is unassembled: the binary is built, but physical operation requires bench validation.

Install `pyserial` on the PC for COM access. The backend and C-backed simulation do
not require pyserial. Run from Firmware:

```powershell
python tools/pc_cal_capture.py ports
python tools/pc_cal_capture.py identify --port COM5
python tools/pc_cal_capture.py status --port COM5
python tools/pc_cal_capture.py capture --port COM5 --rref 1000 --frequency 1000 --amplitude 100 --standard OPEN --campaign captures.json
python tools/pc_cal_capture.py capture --port COM5 --rref 1000 --frequency 1000 --amplitude 100 --standard SHORT --campaign captures.json
python tools/pc_cal_capture.py capture --port COM5 --rref 1000 --frequency 1000 --amplitude 100 --standard LOAD --load-real 1003 --tolerance-pct 1 --reference-id R1003 --campaign captures.json
python tools/pc_cal_capture.py cancel --port COM5 --capture-id 123
python tools/pc_cal_capture.py simulate --bridge build/a062a-host/tests/Debug/wtk_pc_cal_capture_bridge.exe --campaign build/serial-campaign.json --candidate build/serial-candidate.bin
```

Choose `--port` when multiple COM ports exist. Serial identity/state is printed,
including explicit UNCALIBRATED status. Repeating a completed condition/standard
skips its acquisition; campaigns retain device UID, firmware identity, raw artifact
hex and SHA-256. Reconnect negotiates a fresh ID after a stale IDENTIFY rejection;
`--first-id` overrides negotiation for diagnostics. A firmware change requires a new
campaign. Capture JSON and ADC provenance feed the unchanged A06.1 backend.
Capture evidence is never physically qualified. Nominal ADC scales are explicitly
NOT calibrated; no previous OSL is needed or fabricated. LOAD tolerance describes
an interval around the supplied value, not proof of an exact reference.

## Wire contract

PLC1 remains version 1: 16-byte little-endian `<IBBHHHI>` header, magic PLC1,
version, type, flags, sequence, payload length, payload CRC-32. Payload maximum
128 bytes; flags must be zero. Existing Resource Pack IDs/semantics are unchanged.
The capture API version is separately 1. Requests start with `<BBHI>`: API,
command echo, zero reserved u16, nonzero monotonically increasing u32 request ID.
Header sequence must equal its low 16 bits. Command and identity are protected by
payload CRC as well as checked against the header. IDs do not wrap during a boot;
reboot starts a new device session. One PC owns the port; multiplexed clients are unsupported.

Responses use type `command | 0x80` and `<BBHIHH>`: API, command echo, reserved,
request ID, error u16, reserved. Both reserved fields are zero. Error codes 0..11:
OK, BAD_FRAME, BAD_COMMAND, BAD_PAYLOAD, STALE_REQUEST, BUSY, UNSUPPORTED,
SAFETY_BLOCKED, NOT_READY, CANCELED, TIMEOUT, ACQUISITION_ERROR. Invalid CRC/header
without trusted identity is discarded. STALE_REQUEST additionally returns the
last accepted request ID u32; only identification can negotiate automatically.

| Command | ID | Request body | Success body |
| --- | --- | --- | --- |
| IDENTIFY | 0x50 | Empty | 60-byte identity |
| STATUS | 0x51 | Empty | 32-byte status |
| START | 0x52 | `<4B2f>` range/frequency/amplitude/standard IDs, known LOAD real/imag | Empty ACK |
| RESULT | 0x53 | `<IHBB>` capture ID, offset, count, zero | `<IHHI>` capture ID, offset, total length, artifact CRC; data |
| CANCEL | 0x54 | Capture ID u32 | Empty ACK after safe transfer is possible |

Conditions use the existing enum order: RREF 10/100/1k/10k/100k/1M,
frequency 100/1000/10000 Hz, amplitude 100/500 mVrms. Standards OPEN/SHORT/LOAD
are 0/1/2. 10 ohm/500 mVrms is forbidden; 33 keys remain. Non-LOAD impedance
must be zero. LOAD must be finite, passive and nonzero. Reference ID/tolerance
remain PC sidecar metadata; the device receives the requested complex center.

Identity offsets: 0 hardware revision u32 (0x10001), 4 model u16 (4), 6 profile
u16 (1=BRINGUP_CAL), 8 capability bitmap u32 (bits 0 identity, 1 status, 2 capture,
3 cancel), 12 condition count u16, 14 artifact size u16, 16 timeout ms u32,
20 ADC provenance u16 (1), 22 synthetic u8, 23 zero, 24 UID[12], 36 git[8],
44 firmware version[16], NUL padded. Capability bit 4 now advertises the optional A06.2b installation handler.
Status offsets: 0 capture ID, 4 active OSL sequence, 8 faults, 12 safety blockers,
16 protocol errors (all u32), 20 temperature mC i32; bytes 24 capture error,
25 session busy, 26 result available, 27 active calibration valid, 28 transfer safe,
29 temperature valid, 30..31 zero. An active valid record is not physical qualification.

## CCO1 observation artifact (188 bytes)

RESULT chunks contain at most 96 bytes; offsets/counts are explicitly checked.
The PC validates chunk identity, whole-artifact CRC, condition and device identity.

| Offset | Field |
| ---: | --- |
| 0 | Magic CCO1 u32; API u16; size u16 |
| 8 | Capture ID u32, hardware revision u32, model u16 |
| 18 | Range/frequency/amplitude/standard u8 each |
| 22 | Flags u16 |
| 24 | Safety faults u32, teardown safety blockers u32, timestamp ms u32, temperature mC i32 |
| 40 | Accepted/rejected/attempts/workflow result u8 each |
| 44 | ADC provenance u16; reserved u16 |
| 48 | Six GND-referenced complex phasors, real/imag float32 pairs |
| 96 | Six ADC scale/offset float32 pairs |
| 144 | Git identity[8], device UID[12] |
| 164 | Hardware error u32, cumulative reject flags u32 |
| 172 | Last accepted block permit issue/validation ms u32 each |
| 180 | Accepted-block stream clipping mask u32 |
| 184 | CRC-32 over bytes 0..183 |

Channel order is VEXC1, RET1X, VEXC2, RET_HG, VMID_ADC1, VMID_ADC2.
Flags bits 0 stable, 1 safe teardown, 2 usable 1X, 3 usable HG, 4 observed HG
overlap, 5 temperature available, 6 nominal ADC NOT calibrated, 7 1X clipped,
8 HG clipped, 9 synthetic. No bit asserts physical qualification. GND phasors
are running means of the existing accepted VMID-referenced samples restored with
the same averaged VMID; the existing normalization/OSL equations remain unchanged.
The existing session uses `measurement_adc_calibration_ideal()` even if an OSL
set is present. These actual nominal 3.3/4095 V/code and zero-offset values are
reported, not substituted with invented measured ADC values.

Range blocker bit 3 is expected at teardown because range is disabled. The PC
retains it and rejects other blockers/faults. Hardware still selects the range and
issues/validates its real permit for every attempt; this is not a range bypass.
The service uses six stable accepted repeats, existing headroom/path checks and
ten-attempt limit. It owns one 3,072-byte workspace; no second ADC buffer exists.
RX is 144 bytes, two bounded TX replies total 288 bytes, artifact 188 bytes, and
six complex means 48 bytes; complete service context is 976 bytes on ARM.

RX fragments expire at 250 ms. A capture expires after 20 seconds independently
of PC connectivity and drains the existing hardware abort FSM. CANCEL also drains
that FSM. TX occurs only after SAFE, excitation OFF, range disabled, acquisition
inactive and quiet mode released. Main-loop byte reads/writes are bounded; TXE
polling is nonblocking. Queue overflow aborts capture and invalidates its result.
Only one volatile result exists; another START invalidates it. Capture commands
do not install anything; use the separate A06.2b installer afterwards. An installation
already programming can complete after disconnect. Resend IDENTIFY/STATUS to recover
port state; do not
automatically replay START after an ambiguous interrupted write.

The host fixture links this same parser, session, stable evidence extraction and
DSP. Its acquisition callbacks are mocked: synthetic quantized six-channel ADC
samples derive from series Z/RREF=0.08+j0.002 and shunt Y*RREF=0.0003+j0.00008,
the voltage divider, HG=15.31+j0.03, and requested excitation. The deliberately
large synthetic SHORT residual supports stable HG overlap; it is not a board model
or accuracy claim. Existing hardware FSM mock tests separately cover real permits,
range sequencing, timeouts, charger abort and safe teardown. Physical GPIO/ADC/UART
behavior is REQUIRES_BENCH_VALIDATION.
