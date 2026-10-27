# Serial OK control and continuous-survey functional check

Embedded source: 9ecaa696ec785e6c15f240df23f75bd07d40d5ff.
Deployment-tool persistence fix: 42cd8d8. Unit: 54ff70068370525760380567.
PRODUCT serial short-OK, continuous power supervision, seven-sample publication
and final SAFE cleanup are observed in one actual unchanged-DUT measurement.
Physical contact count and independent accuracy remain unqualified.

## Control and observation

WTK_ENABLE_SERIAL_UI is ON by default in every PRODUCT profile, including Debug.
It uses existing CRC-checked PLC1 framing, boot-local increasing request IDs and
an exact expected measurement-session sequence. One accepted request delivers
ordinary OK PRESS/RELEASE. It cannot directly control K1 or bypass fresh hardware
permission. Duplicate/malformed/stale-session, unsafe, busy, physical-button
overlap and nonmeasurement UI contexts reject activation. Quiet acquisition
defers UART parsing/output; partial receive expires at 250 ms. Resource payload
bytes are not scanned for commands. WOKE_ONLY preserves the normal first-click
backlight-wake behavior. An acknowledgement does not claim completion.

BRINGUP and BRINGUP_CAL keep their existing dedicated diagnostic/capture
protocols and have no PRODUCT OK action. The feature can be disabled with
`-DWTK_ENABLE_SERIAL_UI=OFF`; a separate factory OFF build passes. Wire details
are in [UI diagnostics](../../../../docs/06-UI-UX-and-Diagnostics.md).

From the repository root, use a new output directory for every invocation:

```text
python Firmware/tools/pc_serial_ui.py --port COM4 --out <new-status-directory>
python Firmware/tools/pc_serial_ui.py --port COM4 --ok --out <new-click-directory>
```

The PC tool queries first and never automatically retries activation. It records
exact request/reply bytes and leaves an ambiguous timeout for explicit review.
DTR/RTS stay deasserted, with no reset lead or debugger measurement command.

The owner confirmed only the same nominal 470-uH inductor remained connected,
with COM4 and ST-Link, then left the bench. A fresh query and exactly one serial
OK were acknowledged. Session 1 completed with **472.656 uH at 1 kHz**, five
raw search conditions across all three frequencies and six extra primary
captures: eleven captures total, seven accepted primary samples. Independent
replay reproduces R/X masks 69/38 and the aggregate 7.322481 + j2.969788 Ohm.
The result is final/nonpartial, inductance valid, coverage attempted/usable 7/7.

Five real power refreshes occurred. Their actual timestamps renewed the
unchanged 1,000-ms battery deadline; final due/deadline separation is 750 ms.
One initial permit remains traceable throughout the connected workflow. The
firmware connected-entry to final SAFE-guard interval is 1,617 ms, reconstructed
from deadlines rather than a scope/contact measurement. Final live checks show
hardware IDLE, K1 SAFE, range/source OFF, quiet/workspace free, no fault and
active OSL sequence 1/all 33. A subsequent status query and intentional exact
packet replay returned STALE_REQUEST without changing session/capture counts.
The initial direct replay produced no reply; it is retained as ambiguous,
with unchanged final metadata, and no root cause inferred. The later verified
rejection used a fresh status query on the same connection.

Completed data came from live image/UID/sequence-guarded DAP without halting or
resetting acquisition; serial supplied the actual request and acknowledgement.
No numeric battery voltage was read through SWD. No new resistor campaign,
capacitor run, W25Q erase/program or OSL/NTC change was requested. The owner was
absent, so one physical relay engagement/release was not witnessed. Prior C/L
observations remain historical; independent accuracy, polarized envelope,
actual 10-mF endpoint, contact/abort timing, stack high-water and TFT appearance
remain REQUIRES_BENCH_VALIDATION.

## Builds, deployment and capacity

Host CTest passes **50/50 factory and 48/48 standard**. All nine strict UID-bound
profiles plus the serial-OFF factory variant pass. The deployment persistence
suite also passes 61 cases with five explicitly skipped artifact-only cases.
The PC tool's CRC/identity/bounds/uncertain-write tests and ordinary UI gesture
tests cover rejection, duplicate handling and wake-only behavior.

The first attempt stopped while atomically persisting a pre-program request:
Windows returned access denied replacing the report. Its conservative report
marked programming unknown. Recovery independently read the unchanged old image
and known halted SAFE-reset preflight state before repeating the same authorized
programming. Bounded retries for transient PermissionError preserve atomicity;
persistent failure still stops before the next hardware action. Both attempts,
temporary evidence and the successful retry remain archived.

Programming/full readback match SHA-256
`5dd9a53f4117a5239e2ec0769e0560c10ce4114789f7782ef929da4b684aba7d`.
Clean COM4 boot captured one SAFE_BOOT and zero automatic measurements, with
all six factory flags enabled and calibration preserved.

The MCU factory-size register returned **0x0040 at 0x1FFFF7E0: 64 KiB**. Current
firmware occupies **61,596 B (93.988%)**, leaving **3,940 B** within the declared
65,536-B capacity and 2,916 B below the product hard budget. RAM accounting is
17,116 B including the reserved stack, inside the unchanged 17,408-B hard gate.
All-FF reads above 64 KiB do not prove writable extra capacity; no out-of-range
erase/program test was done. See [capacity evidence](flash-capacity.json),
[all profile sizes](memory-table.md) and [checkpoint/checksums](checkpoint.json).

Archives contain ten image/map/config sets, manifests, complete readback,
UART boot, live result/health data, serial exchanges, failure/recovery evidence,
host logs, build matrix and exact procedures.
