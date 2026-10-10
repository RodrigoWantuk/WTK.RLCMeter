# B01.0 — first power-on diagnostic toolkit

Status: **IMPLEMENTED_TESTED_HOST / REQUIRES_BENCH_VALIDATION**.
No assembled PCB, physical UART test, electrical measurement or qualification is
claimed. This toolkit records evidence; it does not approve power-on or MEASURE.

Starting main: `032371d0293adc225cae8d2e36894117fcbd1c0d`; PR #15 was verified
merged before source inspection. The original checkout was clean, main was
fast-forwarded after fetch/prune, submodules initialized, and work isolated on
`codex/b01-first-power-toolkit`. Tools: Arm GCC 14.2.1 (20241119), CMake 4.4.4,
MSVC 19.44.35228.0, Python 3.9.5; optional PC serial dependency pyserial 3.5.

## Delivered behavior

- [pc_bringup.py](../../../Firmware/tools/pc_bringup.py): offline `prepare`,
  provenance-preserving `record`, read-only `snapshot`, port enumeration,
  synthetic `fake`, deterministic byte `replay`, JSON/Markdown/raw UART reports,
  and an explicitly non-qualifying measured-voltage resistor-current estimate.
- [PC_BRINGUP.md](../../../Firmware/tools/PC_BRINGUP.md): Windows examples,
  first-use gates, connector order, instrument distinction, limits and recovery.
- [Source-derived fixture](../../../Firmware/tools/fixtures/bringup_rev1.json)
  and [56 new tests](../../../Firmware/tests/tools/test_pc_bringup.py).
  Generated sessions/bundles stay in ignored build output, not Git history.
- Plans 09A/15 and the existing bring-up entrypoint now link executable collection;
  B01 physical safe boot remains incomplete.

The immutable allowlist is exactly `lab fault status`, `lab charger status`,
`lab sensors status`, `lab safety status`, `lab range status`, `lab adc status`,
`lab flash info`, `lab mvp status`, `lab cal status`. Each exact dispatch branch
and its status writer was inspected in `app_bringup_console.c`; regression checks
verify the source branches/writers, banner fields and key output literals.
`bsp_uart.c` confirms USART1 PA9/PA10, 115200 8N1. `bsp_diagnostics.c` supplies
the passive detailed identity. No guessed help/identity command is added.
The banner prints before peripheral initialization; collection also retains
`app_shell.c` startup diagnostics and waits for its first `safety_block:` loop
marker plus a passive one-second settle. This avoids mistaking startup `charger:`
output for a command response. Runtime safety/display/button lines are retained
and dangerous safety transitions preempt a previously fault-free snapshot.

Real mode needs declared BRINGUP, preceding physical evidence, and its passive
complete version-0.1.0 Rev1 banner. PRODUCT, BRINGUP_CAL, unknown version/profile,
short SAFE_BOOT banner, wrong hardware/SHA and binary data are refused. Opening
an already-running board without a banner cannot establish identity; no blind
probe or automatic reset is used. The PC listener must precede a separately
authorized manual power-on/reset. DTR/RTS are deasserted before opening and their
wires must remain absent. Real COM configuration was checked with mocks; pyserial
3.5 also passed a nine-command localhost synthetic socket-transport smoke. Port
enumeration was exercised without opening a physical COM device.

No erase/program/selftest, range/relay change, excitation, ADC/calibration capture,
installation, reset, override or `--force` exists in the automatic path. The
console has no request IDs or prompt: requests are serialized and unexpected/stale
output, duplicates, partial writes, reset or timeout terminate the snapshot.
The final response also has a bounded quiet check for late duplicate output.
No second request is sent after protocol ambiguity; reconnect needs fresh identity.

Responses retain command timestamps, raw bytes/hashes, parsed observations,
interleaved logs and failure reasons. Bounds: 256-B lines, 128-B reads, 64-KiB RX,
4096 events, 2-MiB evidence inputs, 50-ms real read timeout, maximum 60-s configured
deadlines. Replay re-parses original RX/TX, ignores cached parsed values and retains
original connection/time failures. Bundles are SIMULATED for fake/replay evidence;
manual instrument/operator/time/path/value/rationale and previous entry history
remain intact. All reports set `physically_validated=false`.

PASS is scoped to the reported condition. STOP halts active work; UNKNOWN cannot
establish identity/validity and requires investigation; missing checks are
NOT_TESTED, never PASS. Expected disabled-range blocking is recognized without
issuing a permit. Charger present, unsafe residuals, faults, active K1/range and
inconsistent permission flags cause STOP. Blank OSL is explicit WARNING/uncalibrated.
No-fault, SAFE_BOOT or active OSL is not physical safety/accuracy evidence.

## Reproduced validation

```powershell
python -m unittest discover -s Firmware/tests/tools -p test_pc_bringup.py -v
python Firmware/tools/product_factory_matrix.py --host --out Firmware/build/b010-host
python Firmware/tools/product_factory_matrix.py --out Firmware/build/b010-baseline
python Firmware/tools/product_factory_matrix.py --out Firmware/build/b010-final
python PCB/tools/audit_rev1.py --check
python -m unittest discover -s PCB/tests -v
python Firmware/tools/run_virtual_tests.py --check-only
python Firmware/tools/run_virtual_tests.py --lint-only
```

Host Debug/Release × legacy/factory × curves OFF/ON: **336 CTest invocations**
pass (legacy 40/43; factory 41/44). Full Python with each configuration's real
C fixture active: **259 tests/configuration, 2072 successful invocations**,
zero failures/skips. The 56 new tests cover allowlist/injection, exact commands,
unknown modes/identity, boot banners, byte splits/CRLF, delayed/interleaved output,
timeouts/reconnect, partial writes/packets, reset/disconnection, stale/duplicate
replies, sensor ages/invalidity, faults/charger/Flash absence, bounded failures,
metadata/provenance, report bytes, deterministic replay and manual clamp authorization.
Existing real C solver, installation/readback/PRODUCT runtime, Resource Pack,
UI and safety regressions remain. The C-backed recovery sweep still exercises
66 store transitions and 125 partial-NOR interruption points per suite.

All **12 ARM images** pass build, physical/project size and source/symbol gates.
Hardware extraction/doc consistency and **17 PCB tests** pass. `git diff --check`
passes. Wokwi local checks, custom-chip compilation and CLI lint pass. Smoke was
attempted but **not executed: WOKWI_CLI_TOKEN is absent**. No physical bench result
or Wokwi scenario pass is implied. Hosted CI status is separate from local tests.
PR #16's Flash-forensics and Virtual-Hardware jobs did not start: both GitHub
annotations report **account locked due to a billing issue**. This external
account blocker is separate from passing local tests and the absent Wokwi token.

## Fresh memory evidence — no embedded changes

[sizes.csv](sizes.csv) records before/after linked bytes for all 12 images.
All deltas are **0 Flash / 0 accounted RAM**. The matched starting-SHA baseline
and after-host-tool builds produce **12/12 byte-identical programmed .bin files**.
Debug ELF containers can differ due to out-of-tree debug paths; those are not
programmed firmware differences. Rebuilding another Git commit updates its banner
metadata. No embedded source, CMake/linker input, calibration/protocol, PCB or
memory gate changes. Accounted RAM includes the existing stack reservation.

| Profile / curves | Before = after Flash B | Before = after RAM B | Stack B |
| --- | ---: | ---: | ---: |
| Legacy Debug OFF / ON | 58,700 / 61,312 | 14,536 / 14,600 | 2048 |
| Legacy Release OFF / ON | 62,044 / 64,468 | 14,984 / 15,048 | 2052 |
| Factory Debug OFF / ON | 51,728 / 54,208 | 14,832 / 14,896 | 2052 |
| Factory Release OFF / ON | 52,648 / 55,124 | 15,040 / 15,104 | 2052 |
| BRINGUP OFF / ON | 65,448 | 14,596 | 2048 |
| BRINGUP_CAL OFF / ON | 31,944 | 12,744 | 2048 |

BRINGUP retains **88 B** physical Flash margin. New functionality runs on PC.

## Open physical gates and first use

Prepare the session today; use fake/replay to learn the report without a board.
After actual A05 unpowered inspection and current-limited rail/safe-boot/cable
checks, record their dated evidence, deploy BRINGUP over SWD, set the expected
firmware SHA/profile and start `snapshot --port COM5` before a reviewed manual boot.
Read STOP/UNKNOWN items first. The next permitted step is resolving those gates,
not active capture. Detailed commands are in PC_BRINGUP.

- **R02:** R_TFT_LED remains open until TFT LED input/drive is characterized;
  the intentional 1×9 connector does not prove PB0 load compatibility.
- **R05:** physically verify Bluepill 40-pin/header, +5V/3V3/GND/backup VBAT,
  SWD, regulator, HSE and PA11/12 before fitting/power. Reminder never auto-closes.
- Check mandatory R0_BANK/U4 bypass links, K2/driver/TVS DNP, actual transistor
  lots/orientation, 5V buzzer drive and identified charger/protection/boost/cell.
- **A05-Y04:** actual 5V_A/3V3 start/stop traces, backfeed, output/ADC pin voltages,
  specifically authorized later BAT54S-conduction capture and rail stability.
  MCU ADC values assume 3.300V; no actual 3V3 rail channel exists in this console.
  Resistor-current estimates are not proof of clamp conduction or electrical safety.
- Scope grounds remain actual GND only; no VMID clip, floating earth, energized
  DUT, unapproved DC or insulation experiment. Measure physical K1 contacts,
  range/PWM safe outputs, interlock behavior and watchdog/reset recovery separately.

Smallest next blocker: record the unpowered R02/R05/module/population inspection,
then the A05 current-limited supply/safe-boot evidence. No additional firmware
diagnostic is needed to begin that work; contact feedback, actual rail/current and
clamp evidence remain unavailable through UART and must stay manual.
