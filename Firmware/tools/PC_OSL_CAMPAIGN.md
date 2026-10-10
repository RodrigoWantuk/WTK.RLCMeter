# A04.1 — guided PC OSL campaign

`pc_osl_campaign.py` wraps the existing PLC1 capture client, CCO1 artifact checks,
model-v4 OSL solver and canonical installer validator. Python 3.9+ standard
library supports preparation, reports and C-backed simulation. Physical COM
access needs `pyserial` on Windows. **The PCB is unassembled; all physical
capture, electrical safety and accuracy remain REQUIRES_BENCH_VALIDATION.**

## Prepare the references without a board

From the repository root, keep actual evidence under ignored `Firmware/build/`
or an external backed-up bench directory:

```powershell
python Firmware/tools/pc_osl_campaign.py references --out Firmware/build/references.json
# Edit references.json with the actual fixture IDs, resistor information and tolerances.
python Firmware/tools/pc_osl_campaign.py references --inventory Firmware/build/references.json
python Firmware/tools/pc_osl_campaign.py plan --inventory Firmware/build/references.json
```

The generated inventory has six nominal LOAD categories (10 / 100 / 1k / 10k /
100k / 1M ohm), **unknown fixtures and missing tolerances**, no measured values.
It deliberately fails validation until the operator identifies OPEN, SHORT and
LOAD fixtures, published tolerance, temperature and frequency assumptions.
Manufacturer, part and notes may remain unknown. Ordinary 1% metal-film resistors
are usable inputs; there is no requirement for expensive standards.

Example for one entry after inspecting a real resistor, with no independent
measurement: `published_tolerance_pct: 1`, `actual_impedance_ohms: null`,
`measured_interval_ohms: null`, `measurement: null`. Enter a descriptive
`fixture_id`, assumptions (e.g. ambient temperature not measured; nominal
resistance used across 100Hz/1kHz/10kHz, AC behavior not characterized), and notes.
Nominal `[1000, 0]` ohm remains an uncertain fit anchor, not measured truth.

For an independently entered actual value use `[real, imaginary]` ohm and a
`measurement` object containing nonempty `instrument`, `operator`, `date`,
`evidence`. Optional `measured_interval_ohms: [low, high]` is a resistance
interval containing that actual value strictly inside it. Keep uncertainty
nonzero. The inventory preserves both published and measured evidence. Without
a measured interval the backend conservatively retains the entire printed
nominal tolerance when re-centering at an entered value. A measured interval is
used only when explicitly supplied, never inferred from DMM digits. Complex
references use an explicit radial tolerance and still need independent phase /
frequency evidence for any future qualification. No reference or frame gets
QUALIFIED flags.

## First physical use after A05/B01 gates

Complete the [staged assembly/power/analog checks](../../docs/review/a05/bringup-checklist.md)
and [B01 evidence](PC_BRINGUP.md) before calibration. In particular, R02 keeps
R_TFT_LED open until its load is known; R05 requires physical Bluepill header,
power/VBAT/SWD/regulator/HSE/PA11/12 inspection. A05-Y04 5V analog-output clamp
injection into 3V3 remains an independent electrical risk. Firmware does not
eliminate it. No energized DUT, live external source, charged component or
unapproved DC experiment is allowed. Scope grounds connect to actual GND only,
never VMID; do not float protective earth.

Load **BRINGUP_CAL with installation capability enabled** through SWD, preserving
external W25Q. Conventional BRINGUP text commands and PRODUCT are not campaign
targets. Use a 3.3V-logic UART adapter, 115200 8N1: J_UART.1 GND, .2 MCU TX to
adapter RX, .3 MCU RX from adapter TX. Leave supply/DTR/RTS/reset wires absent.
Driven UART/SWD can backfeed an unpowered target; connect only in the reviewed
power arrangement. The tool deasserts DTR/RTS before opening and never resets
real hardware. Port discovery reuses the existing utility:

```powershell
python -m pip install pyserial
python Firmware/tools/pc_cal_capture.py ports
python Firmware/tools/pc_osl_campaign.py start --inventory Firmware/build/references.json --campaign Firmware/build/board1-osl.json --port COM5 --operator Rodrigo
```

Device identity and UNCALIBRATED/CALIBRATED record status are explicit. A valid
record is not physical qualification. A default campaign requests OPEN at all
33 keys, SHORT at all 33 keys, then LOAD grouped by ascending RREF, each with
100Hz/1kHz/10kHz and supported 100/500mVrms amplitudes. The forbidden
10-ohm/500mVrms keys are absent. This takes **eight fixture setups, seven fixture
changes, and 99 explicit CAPTURE confirmations** in an uninterrupted campaign.
The operator can leave OPEN/SHORT attached across each group; these are 99
simple confirmations, not 99 independently composed CLI commands. Resuming or
recapturing adds confirmations only for the observations requested again.
Use `start --order condition` for 33 OPEN/SHORT/LOAD triplets instead.

Every prompt states standard, fixture, LOAD ID/center/interval, range, frequency
and amplitude. Type `CAPTURE` only after confirming the exact passive fixture.
Anything else pauses. Never reconnect a fixture while acquisition is busy.
Before each START, fresh IDENTIFY and STATUS must match the campaign and report
idle/safe/no fault; only the expected disabled-range blocker is allowed. Embedded
permissions remain authoritative and qualify residual voltage during capture.
No PC GPIO/relay/range command or safety override exists.

PASS means **accepted acquisition evidence only**, never accuracy. The console
reports peak source/return amplitudes, usable paths, clipping mask, HG overlap,
stable repeat count, board temperature when available and nominal ADC provenance.
The final solver report gives the selected fit path and transfer separation.
The existing ADC scale is nominal 3.3/4095 with zero offsets, explicitly NOT
calibrated; it does not measure the actual 3V3 rail. Clipped HG with usable 1X
can remain useful; both unusable paths, weak source, unsafe/unstable acquisition
and degenerate triplets cannot complete a condition. A physically unobservable
key remains missing, preventing a complete candidate; no record is fabricated.

## Pause, cancellation and recovery

```powershell
python Firmware/tools/pc_osl_campaign.py status --campaign Firmware/build/board1-osl.json
python Firmware/tools/pc_osl_campaign.py inspect --campaign Firmware/build/board1-osl.json
python Firmware/tools/pc_osl_campaign.py resume --campaign Firmware/build/board1-osl.json --port COM5 --operator Rodrigo
# Explicit replacement of previously accepted evidence, with its old artifact retained:
python Firmware/tools/pc_osl_campaign.py resume --campaign Firmware/build/board1-osl.json --port COM5 --operator Rodrigo --recapture 1000 1000 100 LOAD
python Firmware/tools/pc_osl_campaign.py report --campaign Firmware/build/board1-osl.json --out Firmware/build/partial-report.md
```

Each accepted observation is saved with its original 188-byte artifact and
SHA-256 through same-directory fsync/atomic replacement. A checksum seals the
manifest; canonical raw decoding, duplicate detection, inventory binding and
ADC/device equality run again on load. These hashes detect accidental alteration,
not malicious edits by someone who recomputes all checksums, physical attachment
or authenticity. Back up the campaign and referenced instrument evidence.
The small `.lock` file uses an OS-released lock; a PC crash releases ownership.
Stale processes cannot overwrite newer progress. One process owns the COM port.
Do not edit a started campaign; create a new campaign to change inventory/firmware.

Reconnection always identifies the device and negotiates request IDs through the
existing client. UID, reported Git/version, profile, ADC provenance and active /
successor sequence must remain compatible. A changed firmware/device or installed
calibration requires a new campaign. The protocol reports an **8-character Git
identifier**, not a full Git SHA or boot counter; keep the exact deployed SHA in
the independent bench log. No extra identity fields are invented here.

Rejected attempts and explicit replacements remain in the audit history. Missing
observations are retried only after an explicit resume and a new confirmation.
No automatic retry after timeout, disconnect or safety rejection. Ctrl+C during
acquisition uses existing STATUS/CANCEL, including a lost START acknowledgement;
unknown communication cannot prove cancellation. The firmware's independent
20-second deadline remains authoritative. Verify SAFE after reconnection before
touching the fixture or resuming. An interrupted receive is never accepted.

## Completion and separate installation

After all 99 captures, start/resume automatically solves and exports into a new
timestamped candidate directory (or `--out <new-directory>`). To rebuild/report
offline explicitly:

```powershell
python Firmware/tools/pc_osl_campaign.py build --campaign Firmware/build/board1-osl.json --out Firmware/build/board1-candidate
python Firmware/tools/pc_osl_calibrate.py inspect Firmware/build/board1-candidate/candidate.bin
```

The output contains canonical **2760-byte schema-v2/model-v4 candidate.bin**,
solver-compatible captures.json, the complete campaign.json audit snapshot,
report.json with hashes/provenance, and report.md with reference intervals,
per-capture quality and the existing 33-condition solver table. An incomplete
campaign cannot build. Existing output directories/reports are never overwritten;
use a new destination after interrupted export. All candidates remain UNQUALIFIED.

The tool displays the existing installation command, but never executes it.
Read status again and deliberately perform the separate operation when authorized:

```powershell
python Firmware/tools/pc_osl_install.py status --port COM5
python Firmware/tools/pc_osl_install.py install --port COM5 --candidate Firmware/build/board1-candidate/candidate.bin --report Firmware/build/board1-install.json --readback Firmware/build/board1-installed.bin
```

The campaign binds its candidate to the observed successor (1 on blank media;
rollover blocked). The installer independently rejects a changed successor, verifies
device commit and byte-identical readback. Campaign transmission is not installation.
After verified installation/reset, deploy factory PRODUCT through SWD without
erasing W25Q; Resource Pack, boot and measurement safety prerequisites still apply.

## Reproducible synthetic operation

```powershell
python Firmware/tools/product_factory_matrix.py --host --out Firmware/build/a04-host
python Firmware/tools/pc_osl_campaign.py simulate --bridge Firmware/build/a04-host/host-debug-factory-ON-curves-OFF/tests/Debug/wtk_pc_cal_capture_bridge.exe --out Firmware/build/a04-demo
```

Simulation emulates operator confirmations through a test-only C fixture. It
collects 42, saves, resets/reconnects, resumes at 43, collects 99 and validates
33 keys/2760 bytes against the installer. It never installs and confirms the
device remains blank. Raw frames use the real C parser/session/DSP; electrical
samples and NOR hardware remain synthetic. Physical start/resume has no
unattended/yes-all/force/bridge option. All simulated evidence is conspicuously
SIMULATED / SYNTHETIC_NOT_PHYSICALLY_QUALIFIED.

Physical gates remain supply/interlock/K1/range/ADC timing and clipping, effective
HG overlap, fixture repeatability/loading/leakage, real reference intervals and
temperature, independent R/C/L holdouts, W25Q installation/power-loss/readback,
and standalone PRODUCT after SWD deployment. A successful 33-key fit is not an
accuracy certificate. No embedded, PCB or calibration format changes are made.
