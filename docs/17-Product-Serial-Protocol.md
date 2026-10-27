# PRODUCT serial control and telemetry

TTL UART is 115200 8N1, 3.3-V signals, with DTR/RTS deasserted by the PC client.
Use the documented J_UART wiring; the first board shares its physical module
space with the display. The PRODUCT interface is distinct from BRINGUP and
BRINGUP_CAL capture/install protocols. It is not native USB.

## Framing and admission

Frames retain the 16-byte PLC1 header and payload CRC described by
`app_pc_link_protocol.h`. All fields are little-endian. Control/read requests use
12 bytes: API 1, echoed command byte, zero uint16, nonzero uint32 request identity,
and uint32 expected measurement-session sequence. Header sequence equals the low
16 bits of request identity. Identities increase within the current MCU boot.

| Command | Type | Response | Payload |
| --- | --- | --- | --- |
| STATUS | 0x30 | 0xB0 | Existing 16-byte state/session/blocker response |
| Short OK | 0x31 | 0xB0 | Existing 16-byte UI-event acknowledgment |
| TELEMETRY | 0x32 | 0xB2 | 128-byte sensor/result/capability response below |

Short OK only uses the normal PRESS/RELEASE controller path from READY/RESULT.
The caller must also verify actual idle hardware, source/ranges off, K1 SAFE,
workspace available, no overlapping buttons/storage operation and real safety
evidence. Its reply acknowledges an input event, not measurement completion.
Wake-only consumption does not start a measurement. Rejected requests consume
their identity; malformed, duplicate and stale control cannot queue an action.

USART1 RX uses a fixed 256-byte ring, interrupt priority 3 below ADC DMA 1 and
excitation DMA 2. TIM4 feedback has priority 4. ISR work is bounded to status/data
capture, enqueue or discard, and counters. Main-loop pop/block/snapshot operations
use short interrupt critical sections. Overflow or hardware framing/parity/noise/
overrun faults invalidate the pending frame; the parser follows the receive epoch
and searches PLC1 magic again.

PRODUCT discards received bytes across boot, measurement/quiet and settings-busy
boundaries. Application and quiet holds are independent; releasing one cannot
release another. Partial frames are invalidated across these transitions. No
busy-time OK is retained for later activation. Idle fault diagnostics can remain
available without granting a measurement permit. No numeric battery reading
requires SWD.

## TELEMETRY payload, API 1

Unavailable fields use validity flags plus sentinels; they are not zero-valued
measurements. Binary floats are IEEE binary32. The PC decoder emits JSON null
for unavailable values and rejects nonfinite data marked valid.

| Offset | Encoding | Meaning |
| ---: | --- | --- |
| 0 | u8 x4 | API, echoed command, status enum, UI state |
| 4 | u32 | Echoed request identity |
| 8 | u32 | Current measurement-session sequence |
| 12 | u32 | MCU uptime in ms at sensor snapshot |
| 16 | u32 | Raw hardware safety blocker mask |
| 20 | u32 | Latched application fault mask |
| 24 | u32 | Loaded factory OSL sequence |
| 28 | u32 | Build capability bits |
| 32 | u16 | Battery mV, or 0xFFFF unavailable |
| 34 | u8 | Battery state enum |
| 35 | u8 | Debounced charger state: absent 0, present 1, unknown 2 |
| 36 | u32 | Battery sample age, ms |
| 40 | i32 | NTC milli-degC, or INT32_MIN unavailable |
| 44 | u32 | NTC sample age, ms |
| 48 | u32 | Validity/provenance bits |
| 52 | u16 x2 | Actual last ADC_BAT and ADC_NTC raw samples |
| 56 | u32 x2 | UART hardware-error and overflow counters |
| 64 | 12 bytes | ASCII build Git prefix, zero padded |
| 76 | 12 bytes | MCU UID bytes |
| 88 | f32 x4 | Last published R(AC), X, C, L; unavailable quiet NaN |
| 104 | u8 | Result flags: derived 1, C 2, L 4 |
| 105 | u8 | Primary frequency enum: 10 Hz 0, 1 kHz 1, 10 kHz 2 |
| 106 | u8 | Primary RREF enum, or 255 unavailable |
| 107 | u8 | Primary return channel, or 255 unavailable |
| 108 | u32 | Active lead-profile sequence, zero if absent |
| 112 | u8 x2 | Settings service status and last BSP error |
| 114 | u8 x2 | K1 commanded state and range FSM state |
| 116 | u32 | Dropped/replaced feedback counter (not an electrical fault) |
| 120 | u8 | Last feedback playback BSP status |
| 121 | u8 | Seven accepted primary samples if trimming applied, otherwise zero |
| 122 | u8 | Frequency-attempted mask when supported |
| 123 | u8 | Reserved zero |
| 124 | u16 | Configured internal Flash KiB; not factory-size-register evidence |
| 126 | u16 | Reserved zero |

Capability bits at 28: serial UI 1, trimmed estimate 2, frequency survey 4,
load-only factory OSL 8, explicit unit 128-KiB profile 16, experimental NTC 32.
Validity bits at 48: battery sample usable 1, NTC temperature usable 2,
experimental NTC 4, OSL loaded 8, published derived result 16, active lead profile
32, UID available 128. Other bits remain reserved until a documented extension.

Battery/NTC validity includes finite/plausible values and auxiliary evidence age
limits of 2,000/5,000 ms respectively. Diagnostic validity is **not** measurement
permission: actual measurement retains its stricter battery permit/freshness and
residual/interlock checks. ADC voltage uses nominal VDDA; NTC and OSL qualification
are separate from successful record loading. R/X/C/L report the last published
session, not a new acquisition triggered by this query. K1 status is a command
state, not independently sensed physical contact confirmation.

## PC client

```powershell
python Firmware/tools/pc_serial_ui.py --port COM4 --telemetry --out <new-evidence-directory>
python Firmware/tools/pc_serial_ui.py --port COM4 --ok --out <another-new-directory>
```

Optional `--pace-ms 3` sends bytes individually for older polled images. It does
not add retries. The updated receiver accepts bounded bursts; actual maximum
latency/noise/flood behavior still requires bench validation. Intended TX, actual
write-reported TX prefix, RX and JSON outcome are retained. A timeout or short
write never automatically repeats an activation. STATUS/TELEMETRY are read-only;
the latter is only supported after deploying a build that advertises this API.
