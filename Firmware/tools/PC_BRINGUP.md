# B01.0 — first power-on evidence toolkit

`pc_bringup.py` is a read-only Windows/COM tool for **BRINGUP**, USART1 115200
8N1. Offline preparation, fake transport, replay and reports use Python 3.9+
standard library only. Real serial/port enumeration requires `pyserial` on the PC:

```powershell
python -m pip install pyserial
python Firmware/tools/pc_bringup.py ports
```

The PCB is unassembled. Software tests do not approve assembly, power-on, relay
operation, capture or accuracy. Follow the [A05 staged procedure](../../docs/review/a05/bringup-checklist.md)
and resolve applicable physical gates. No PCB or embedded firmware change is made.

## Prepare today without hardware

From the repository root; keep actual session data outside Git (example `Firmware/build/`):

```powershell
python Firmware/tools/pc_bringup.py prepare --session Firmware/build/first-board.json --profile BRINGUP
python Firmware/tools/pc_bringup.py fake --session Firmware/build/first-board.json --out Firmware/build/b01-demo
python Firmware/tools/pc_bringup.py replay --session Firmware/build/first-board.json --transcript Firmware/build/b01-demo/transcript.json --out Firmware/build/b01-replay
python Firmware/tools/pc_bringup.py report --session Firmware/build/first-board.json --out Firmware/build/b01-pending
```

Use a new output directory for every run. Existing sessions/output are never
silently replaced. Fake responses are **SIMULATED**, source-derived grammar with
invented test values. All replay bundles are flagged SIMULATED/OFFLINE_REPLAY;
their original manual evidence remains intact. `SIMULATED.txt` labels the entire
bundle, including raw UART files. Replay re-parses RX/TX bytes, ignores saved
parsed values, and preserves original connection/time failures. It cannot recreate
physical timing. Identical transcripts produce identical replay events.

Edit session metadata to record board revision/order/Gerber identity, photographs,
fit/DNP worksheet, substitutions, exact module/18650 identities, instruments,
UART adapter, pending/rejected checks and notes. No unknown value is filled with
a nominal voltage. `expected_sha` may be UNKNOWN or the exact deployed 12..40-digit
Git SHA; the observed banner SHA is reported separately (normally 12 digits).
`prepare --firmware-sha <SHA>` sets an expectation without claiming it was observed.

## Manual evidence and prerequisites

Record actual observations, not example nominal values. The JSON template lists
probe points and methods. `record` timestamps the entry and retains its previous
version in `manual_history`. Example syntax (replace the value and paths with
your independent DMM evidence):

```powershell
python Firmware/tools/pc_bringup.py record --session Firmware/build/first-board.json --check 3v3 --status PASS --value 3.31 --unit V --instrument "DMM model/serial/range" --operator "Rodrigo" --evidence "bench/3v3-photo.jpg" --notes "Actual UFLASH.8 to .4 measurement; current limit and fitted state in bench log"
```

PASS requires method, instrument, operator, timestamp, evidence path and rationale.
Rail PASS checks also require an actual finite voltage in the A05 Stage 2 window:
5V_SYS/5V_A 4.75..5.25 V, 3V3/W25Q supply 3.0..3.6 V, VMID 1.35..1.95 V.
These are existing engineering gates, **not precision specifications**. For a
failure use STOP and retain the observed value. Qualitative checks use the dated
notes/evidence for the operator's interpretation; the tool cannot verify a photo
or authenticate an operator's assertion. Use `--simulated` for test entries;
they cannot satisfy real serial gates.

Before `snapshot`, these manual checks need real PASS evidence: `assembly`, `R02`,
`R05`, `power`, `safe_boot`, `uart_cable`, `j_pwr_5v`, `5v_a`, `3v3`,
`w25q_supply`, `modules`. Any manual STOP prevents serial opening. This does not
require completed metrology or calibration. Resolve omitted battery sensing as
a documented blocker; never fake the ADC or bypass permits.

- **R02:** keep **R_TFT_LED open** until the module LED input/current/logic
  requirements are independently verified. PB0 cannot drive an unknown raw LED
  load. The intentional 1×9 TFT/module agreement does not resolve this reminder.
- **R05:** before soldering/powering Bluepill, verify the 40-pin alignment, +5V,
  +3V3, GND, backup VBAT, accessible SWD, regulator, HSE and PA11/PA12 usage.
  An STM32F103C8T6 marking alone does not establish module equivalence.
- Record K2/QK2/RK2B/RK2PD/DK2 and TVS/link DNP; R0_BANK, R_BYP_VM and
  R_BYP_VEXC fitted. Inspect purchased 5V buzzer type/current/drive compatibility.
  Identify external charger/protection/boost/cell and power sequencing.

## First serial use after earlier physical gates

Use a **3.3V logic** USB-UART adapter, not RS232/5V logic. J_UART: pin1 GND,
pin2 MCU TX → adapter RX, pin3 MCU RX ← adapter TX. No adapter supply, DTR, RTS
or reset connection. One reviewed power source; no native USB. SWD VTref senses
target power, never supplies it. Driven UART/SWD can backfeed an unpowered target.

Load conventional BRINGUP via SWD while preserving external Flash. Start this
listener before the **manually authorized** safe power-on/reset so it receives
the full boot banner. The tool never resets hardware. DTR/RTS are deasserted
before opening, but adapter glitches remain possible: leave those wires absent.

```powershell
python Firmware/tools/pc_bringup.py snapshot --session Firmware/build/first-board.json --port COM5 --boot-timeout 30 --timeout 3 --out Firmware/build/first-snapshot
```

Declared PRODUCT, BRINGUP_CAL or UNKNOWN profiles are refused. BRINGUP_CAL speaks
binary PLC1; use its existing capture/install clients only at the later approved
stage. A short PRODUCT SAFE_BOOT banner, binary bytes, missing/unknown banner,
wrong hardware or SHA mismatch sends **no command**. Unknown firmware versions
are also refused (audited version: 0.1.0). An already-running board
without a fresh banner cannot be identified by this toolkit; do not add blind
probes. A new connection needs a new passive banner. No automatic retry/reset.

## Automatic commands and report meaning

Source: `app_bringup_console.c` exact-match dispatch/status writers;
`bsp_diagnostics.c` detailed banner; `bsp_uart.c` USART1 configuration.

| Only allowed command | Parsed evidence |
| --- | --- |
| lab fault status | Hex latched fault mask |
| lab charger status | ABSENT / PRESENT / UNKNOWN |
| lab sensors status | Raw VMID/residual/battery/NTC, MCU voltage estimates, validity, age, NTC temperature validity |
| lab safety status | Blocker, flags, reported measurement permission |
| lab range status | Commanded FSM/requested/current range |
| lab adc status | Auxiliary ADC busy/channel/last status |
| lab flash info | Detection, part, JEDEC, capacity; no memory access test |
| lab mvp status | Clock, Flash/display, safety, K1/range, OSL state, residual/battery, last error |
| lab cal status | Schema/model/hardware, active slot/sequence/count, workflow, partition and A/B validity metadata |

The banner precedes peripheral initialization. The collector retains `app_shell.c`
startup lines, waits for its first `safety_block:` loop marker, then listens
passively for one second before requests. This settling window grants no readiness
or permission. Runtime safety/display/button lines remain evidence; dangerous
safety transitions cause STOP even if earlier responses were fault-free.

Exactly one bounded request is outstanding. The console has no response IDs or
prompt, so unexpected/stale/duplicate output, malformed fields, partial write,
reset or timeout ends collection without retry. Timestamped asynchronous log
lines are retained as warnings; unknown output is not guessed. Timeouts are at
most 60 s, read calls 50 ms, lines 256 B, RX 64 KiB, events 4096, evidence input
2 MiB. A reset invalidates the session result. Earlier parsed lines remain
historical evidence, not readings from the restarted board.

The bundle contains `report.json`, `report.md`, command/RX timestamped
`transcript.json`, exact `uart-rx.bin`, readable `uart-rx.txt`, hashes and the
unchanged manual session evidence. No large generated bundle belongs in Git.

PASS means the **specific documented observation**, not board approval. STOP
means halt active work and investigate the failed gate. UNKNOWN means identity,
format, sensor readiness or specification cannot be trusted; resolve evidence
before proceeding. WARNING records charger-independent risks, e.g. blank OSL,
missing display or unexpected supported Flash variant. NOT_TESTED remains pending.
Overall priority is STOP → UNKNOWN → NOT_TESTED → WARNING → PASS. Exit 2 is
STOP/input failure, 3 UNKNOWN; exit 0 means a bundle/preparation completed without
those errors, **not physical validation**. Reports always retain
`physically_validated=false` / REQUIRES_BENCH_VALIDATION. No status authorizes K1.

A disabled range normally reports BLOCKED_RANGE; this is expected at first idle
startup. PRESENT charger, nonzero fault, residual UNSAFE/SATURATED, active K1/range
or inconsistent safety flags cause STOP. BUSY auxiliary ADC can be a normal sweep;
invalid/stale sensor evidence is UNKNOWN. Blank OSL remains explicitly uncalibrated.
The ADC uses assumed VDDA=3.300 V; **it does not measure the 3.3V rail**. NTC is
board temperature, not DUT temperature. Commanded SAFE is not relay contact feedback.

## A05-Y04 analog clamp evidence

U4/U5 MCP6002 outputs run from 5V_A; BAT54S clamps can inject into 3V3 through
the series ADC resistors. Firmware cannot eliminate this unmeasured electrical
effect. Do not separately energize the AFE with an invalid/unpowered 3V3 rail.
The template keeps all these independent fields pending:

1. `powerup_waveforms` / `powerdown_waveforms`: simultaneous 5V_A/3V3 scope traces,
   supply state, fitted parts, scales/probes/timebase and current limit.
2. `signal_backfeed`: any rail rise with regulator inactive, signal source/adapter
   and reviewed connection state. Stop on unexpected powering; no automatic test.
3. `analog_adc_voltages`: DMM output and corresponding ADC input voltage, actual
   resistor value/tolerance and rail readings, with explicit instrument uncertainty.
4. `bat54s_conduction`: specifically authorized **later capture only**, authorization
   reference and trace; PASS requires `--authorization-reference` (or the matching
   JSON field). No automatic saturation/excitation/capture is implemented.
5. `3v3_stability`: rail stability under exactly the recorded tested conditions.

Hantek DSO2C10 grounds are common/earth-referenced: connect only to actual GND
(J_PWR.3/J_UART.1/verified GND). **Never VMID or other signal nodes. Never float
protective earth.** CH1−CH2 uses tips on the two nodes with both grounds at GND,
only after common-mode/input ratings and ground arrangement are checked. Scope
amplitude is waveform evidence, not a precision metrological reference. No live
external DUT voltage, charged component, insulation or unapproved DC test is included.

Optional calculation from actual separately recorded measurements:

```powershell
python Firmware/tools/pc_bringup.py clamp-estimate --vout 4.00 --vadc 3.50 --resistance 1000 --voltage-uncertainty 0.01 --resistance-uncertainty 10
```

Those numbers are **illustrative, not measured**. Estimate `(Vout−Vadc)/R`; the
interval uses ±voltage uncertainty for each measured node and ±resistance
uncertainty in ohms, with worst-case endpoints. It estimates series-resistor
current, which may also include capacitor/ADC currents. It proves neither diode
conduction nor rail safety and cannot turn a pending clamp check into PASS.

## Reproduce tests and memory checks

```powershell
python -m unittest discover -s Firmware/tests/tools -p test_pc_bringup.py -v
python Firmware/tools/product_factory_matrix.py --host --out Firmware/build/b01-host
python Firmware/tools/product_factory_matrix.py --out Firmware/build/b01-arm
```

The host matrix includes both PRODUCT modes, Debug/Release and curves OFF/ON,
with the existing C calibration fixture active. The ARM matrix includes PRODUCT,
BRINGUP and BRINGUP_CAL plus A01 size/profile/symbol gates. Firmware is unchanged;
missing physical diagnostics (contact feedback, actual rails/current, clamp
conduction, GPIO/reset waveforms) remain manual rather than consuming BRINGUP Flash.
