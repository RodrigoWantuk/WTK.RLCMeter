# A04.2 — calibration desktop workbench

Python 3.9+ x64 on Windows 10/11. PySide6 **Essentials** provides Qt Core/Gui/Widgets;
no browser, accounts, cloud service or Qt Addons. Only pyserial opens the real port.
The app calls the existing Campaign, capture client, solver and candidate validator.
It never installs calibration. See [license/deployment review](WORKBENCH_LICENSES.md).

The read-only **Oscilloscope** tab adds explicit generic CSV import, waveform plots,
analog presets and B01 sidecars. See [waveform operation and limits](PC_SCOPE_WAVEFORM.md).
Native Hantek DSO2C10 layout verification remains pending an actual owner export.

## First run from the repository

```powershell
python -m venv Firmware/build/workbench-env
Firmware/build/workbench-env/Scripts/python.exe -m pip install -r Firmware/tools/requirements-workbench.txt
Firmware/build/workbench-env/Scripts/python.exe Firmware/tools/pc_osl_workbench.py
```

Alternatively activate the environment and run `Firmware/tools/run_workbench.cmd`.
Set `WTK_PYTHON` to the environment's python.exe if launching the script directly.
For an offline setup, download wheels with `pip download -r ... -d wheels` on a
connected Windows machine, transfer that directory and install with
`pip install --no-index --find-links wheels -r ...`. The running app needs no network.

## Learn the workflow without a board

Build the existing native C fixture from Firmware:

```powershell
cmake --preset host-debug
cmake --build --preset host-debug
cd ..
Firmware/build/workbench-env/Scripts/python.exe Firmware/tools/pc_osl_workbench.py --bridge Firmware/build/host-debug/tests/Debug/wtk_pc_cal_capture_bridge.exe
```

1. Choose **Simulation**, then **Synthetic inventory**. Enter an operator identity.
2. **New campaign** saves a canonical campaign JSON outside Git, preferably under
   `Firmware/build/`. Standard order minimizes changes: 33 OPEN, 33 SHORT, 33 LOAD
   grouped by six RREFs; eight fixture setups and **99 individual confirmations**.
   Condition order is also available before campaign creation.
3. **Connect** identifies the blank C-backed BRINGUP_CAL fixture. Each confirmation
   checkbox + **Capture once** acquires exactly one observation. Completion never
   starts another capture. The entire session is conspicuously SIMULATED.
4. Select matrix cells to inspect source/return peaks, path validity, clipping,
   repeats, temperature when reported, ADC provenance and the original SHA-256.
   OSL cells show the existing solver's path, separation and coefficients when a
   triplet is complete. REJECTED cells retain reasons; no invalid triplet counts.
5. Close at any time, including after capture 42. Closing during acquisition asks
   for cancellation and waits for the single worker to drain/close. **Open / resume**,
   reconnect and continue at 43. Accepted artifacts/hashes survive application exit.
6. At 99/99 and 33/33, **Generate candidate & reports** creates a NEW directory:
   candidate.bin (2760 bytes), captures.json, campaign.json, report.json, report.md.
   The candidate is schema v2/model v4, UNQUALIFIED and **NOT INSTALLED**.

No physical mode auto-confirm, timer-triggered acquisition, force/reset or installation
action exists. Test automation supplies confirmations only from the test harness.
The portable bundle includes the same native C fixture and selects it automatically.

## References and recovery

**References** edits six LOAD forms: IDs, fixture, nominal/actual complex ohms,
published tolerance, optional independent resistance interval, instrument/operator/
date/evidence, temperature/frequency assumptions, manufacturer/part/notes. Enter
both optional complex coordinates, including zero imaginary part for a resistor.
Units remain ohms; RREF categories are fixed. Validation and effective tolerance
come from `references()`, including conservative nominal-interval recentering.
Missing tolerance/assumptions remain missing, never silently defaulted to 1%.
Measured values need provenance; neither kind of interval grants accuracy qualification.
Load/save uses the existing inventory format. A bound inventory is read only:
disconnect, **Close campaign**, edit/load another inventory and create a NEW campaign.

Select a standard cell and **Recapture selected** to explicitly retry/replace it,
then confirm the displayed fixture. Prior accepted raw evidence remains in the
backend audit trail. Rejections, cancellation and solver errors preserve progress.
After ambiguity there is no automatic retry. UNKNOWN means keep the fixture unchanged,
reconnect and verify SAFE after the independent firmware timeout. Closing the window
can wait for bounded serial requests/cancellation; it never terminates a worker mid-request.
Repeated Capture clicks cannot queue another request. Disconnect is disabled during
capture; cancel/drain first. A campaign with changed UID, reported firmware/ADC identity,
active sequence or damaged artifacts is rejected rather than migrated or overwritten.

## Physical connection and evidence gate

The board is unassembled: **REQUIRES_BENCH_VALIDATION**. First complete A05/B01
inspection and current-limited power work with conventional **BRINGUP**, then deploy
**BRINGUP_CAL** separately over SWD preserving W25Q. Their Git identities may differ.

Connect a 3.3 V USB-UART: J_UART.1 GND, .2 MCU TX to adapter RX, .3 MCU RX to
adapter TX, 115200 8N1. No adapter power/DTR/RTS/reset wires; opening deasserts
DTR/RTS before the port opens. Native USB on Rev.1 is unavailable. Pick **Physical**,
refresh COM ports, select the port, enter operator identity, open/new campaign and Connect.
Identification/status queries are read only; capture stays disabled until review.

**Import B01 session / report** preserves original SHA, metadata, manual evidence
and diagnostic statuses inside the canonical campaign under `desktop_evidence`.
The app shows all missing prerequisites. Simulated or replayed evidence cannot satisfy
physical prerequisites. `physically_validated=false` alone is neither rejection nor
approval; real provenance and individual conditions matter. Manual STOP anywhere
and diagnostic STOP/UNKNOWN block approval. The required physical PASS entries are
the existing B01 serial gates plus VMID, PB9/K1, PB8/RANGE_EN, PA8, PA15,
power-up/down rail waveforms, backfeed, paired analog/ADC voltages and 3V3 stability.
Board photo/fabrication/population/DNP, module and instrument identities must be present.
Keep missing evidence NOT_TESTED; do not edit nominal values into measured fields.

**Review physical prerequisites** records a separate named/datetime operator decision:
same inspected board identity, full deployed BRINGUP_CAL SHA matching its reported
prefix, bench interlock/clamp-risk authorization reference, explicit acknowledgements
of assembly, interlocks, analog risk, passive fixture and SWD transition. This records
an operator's scoped decision, **not an automated safety certificate**. It is required
again after reconnection or failed capture. Firmware still checks its own fresh permits,
charger/residual/K1/range/excitation safety, timeout and cancellation.

- **R02:** keep R_TFT_LED open until TFT LED current/logic is independently verified.
- **R05:** inspect Bluepill 40-pin alignment, +5V/+3V3/GND/VBAT/SWD/regulator/HSE/PA11/12.
- **A05-Y04:** 5 V MCP6002 outputs/BAT54S can inject into 3V3. Scope grounds only to
  actual GND, never VMID/RET/TEST. Do not float earth. Firmware cannot eliminate injection.
  Later authorized BAT54S-conduction capture is intentionally not a prerequisite to
  itself; its unresolved risk and permitted capture scope require explicit review.
- No energized DUT, external injected voltage, DC experiment or physical accuracy claim.

Eight-character firmware identity and UID are bindings, not cryptographic authentication.
Nominal ADC scale is explicitly NOT calibrated. Keep full deployed SHA in the bench log.
No native USB firmware switch or installed-candidate claim is implied.

After independent review, use the SEPARATE existing installer, after querying its
successor sequence again (the GUI only displays a command):

```powershell
python Firmware/tools/pc_osl_install.py status --port COM5
python Firmware/tools/pc_osl_install.py install --port COM5 --candidate "C:/bench/output/candidate.bin" --report "C:/bench/installation.json" --readback "C:/bench/installed.bin"
```

## Headless tests and Windows packaging

```powershell
$env:QT_QPA_PLATFORM='offscreen'
$env:WTK_CAPTURE_BRIDGE=(Resolve-Path Firmware/build/host-debug/tests/Debug/wtk_pc_cal_capture_bridge.exe)
Firmware/build/workbench-env/Scripts/python.exe -m unittest discover -s Firmware/tests/tools -p 'test_pc_osl_workbench*.py' -v
Firmware/build/workbench-env/Scripts/python.exe Firmware/tools/pc_osl_workbench.py --smoke-test
```

Full regression: use that environment's Python with `product_factory_matrix.py --host`;
it runs eight CTest/Python combinations with each real C fixture. ARM comparison uses
`product_factory_matrix.py --out Firmware/build/a042-arm`. GUI tests skip explicitly
if Qt is absent; the recorded complete matrix installs the pinned runtime and has no skips.

For a portable Windows **directory**, build the Release C fixture with static MSVC CRT:

```powershell
cmake --preset host-release -S Firmware -B Firmware/build/workbench-native '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded'
cmake --build Firmware/build/workbench-native --config Release --target wtk_pc_cal_capture_bridge
Firmware/build/workbench-env/Scripts/python.exe -m pip install -r Firmware/tools/requirements-workbench-build.txt
Firmware/build/workbench-env/Scripts/python.exe Firmware/tools/package_workbench.py --bridge Firmware/build/workbench-native/tests/Release/wtk_pc_cal_capture_bridge.exe --out Firmware/build/workbench-package
```

Run `dist/WTK-Calibration/WTK-Calibration.exe`, keeping its entire directory together.
The script does not delete prior bundles. No onefile extraction or browser is used.
Qt DLLs remain dynamically linked and replaceable. Offline startup/cleanup can be
checked with `--smoke-test`; no serial request is made. Generated binaries are ignored.
License/source obligations and release-signing/clean-machine validation are separate
from a local successful bundle build; see the license review before external distribution.
