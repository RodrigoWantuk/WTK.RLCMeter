# B01.1 — safe SWD deployment / first-board handoff

`pc_firmware_deploy.py` is a Windows 10/11 / Python 3.9+ standard-library CLI.
SWD deployment writes only the selected internal application image. Option bytes,
OSL, external W25Q and hardware qualification remain outside its write scope.
Physical qualification remains **REQUIRES_BENCH_VALIDATION**. Fake SWD results are SIMULATED.

Optional [per-unit NTC factory builds](NTC_FACTORY_PROFILE.md) use
`--ntc-factory-profile` and manifest schema `WTK_FIRMWARE_2`. The profile and
float32 table are bound to the BIN and explicit MCU UID; older tools reject the
new manifest. Generic images remain schema 1. Rebuilding a unit without its
profile restores the nominal NTC table. No external W25Q write is involved.

## Supported programmer

Pinned **xPack OpenOCD 0.12.0-6 Windows x64**, reporting
`xPack Open On-Chip Debugger 0.12.0+dev-01850-geb6f2745b-dirty (2025-02-07-10:08)`.
The suffix is the upstream release build label, not this repository's dirty state.
Download the [release](https://github.com/xpack-dev-tools/openocd-xpack/releases/tag/v0.12.0-6)
ZIP and `.sha` on a connected PC, verify SHA-256, extract and retain the complete
directory for offline use. ZIP SHA-256 verified in this run:
`c57724f87219bafde78f61b54fb7f303f456fed9826a3d0ee2d54d461ad58020`.
No programmer executable is committed or bundled. OpenOCD is an external GPLv2
tool; retain its supplied licensing/notices when redistributing that package.

The pinned `interface/stlink.cfg` supports ST-Link V1/V2/V2-1/V3 variants listed
in that package. Exact script hashes/version are checked. Select an adapter serial
explicitly; supported serial spelling is ASCII letters/digits/dot/underscore/hyphen.
This script selects the native `st-link` driver and requires `dapdirect_swd`;
`hla_swd` belongs to the separate HLA driver and fails before initialization with
this pinned script. ST-Link/V1 and V2 firmware older than V2J24 do not support
the native driver. No automatic HLA fallback or adapter firmware update is performed.
Programmer logs retain their original bytes; target-marker parsing accepts both
Windows CRLF and LF line endings while still rejecting duplicate identity markers.
The pinned OpenOCD also supports the legacy ST-Link/V2 26-byte USB string
descriptor through its existing `stlink_usb_get_alternate_serial` callback. It
represents the 12 low bytes of that descriptor as 24 uppercase hexadecimal digits,
including zero bytes. Use that exact alternate serial for `--adapter-serial`;
Windows may display only a truncated prefix. This is the upstream selection
mechanism, not stripping bytes, accepting the prefix, or selecting the first probe.
Verify the actual descriptor and the
[pinned implementation](https://github.com/openocd-org/openocd/blob/eb6f2745b/src/jtag/drivers/stlink_usb.c#L3107)
before using this representation. A legacy descriptor establishes neither genuine
ST hardware nor target compatibility. Other unusual serial encodings, other
OpenOCD releases/configs, clones/unrecognized IDs, 128 KiB reports, protection or ambiguous
geometry STOP; no automatic alternatives. No serial validation, target geometry,
physical evidence, or programming authorization gate is waived.
Standard ST-Link driver/USB installation and actual adapter/VTref wiring still need
the owner's verified adapter model. A cheap connector marked 3.3V may be a **power
output**, not VTref; never assume it is a target-voltage sense input.

## Electrical prerequisites and connection

Follow [A05](../../docs/review/a05/bringup-checklist.md) and [B01](PC_BRINGUP.md).
R02: R_TFT_LED open until verified. R05: inspect Bluepill 40-pin alignment,
5V/3V3/GND/VBAT/regulator/HSE/PA11/12 and SWD. Record part markings/module photographs;
DBGMCU ID 0x410 is shared across STM32 medium-density parts and is **not proof of
genuine STM32F103C8T6**. UID/Git hashes are bindings, not cryptographic authentication.

- Board off/discharged for wiring: SWDIO→PA13, SWCLK→PA14, verified GND→GND.
- VTref, only if the verified adapter provides a real sense input, senses target
  3V3; it does not supply the board. No debugger supply, Bluepill USB, UART power,
  DTR/RTS, charger or unrelated power paths. Record the sole independently powered,
  current-limited supply and stable rail measurements before debugging.
- NRST is optional only according to the verified adapter/module wiring and reviewed
  bench procedure. This tool does not silently change reset configuration to recover
  a failing link. BOOT0 should be physically verified for ordinary Flash boot.
- No DUT/charged capacitor. Before **every** debug operation, independently confirm
  K1 de-energized/NC SAFE, RANGE_EN/excitation off, no acquisition or W25Q transaction.

**Probe/verify are non-programming, not electrically passive.** OpenOCD `init;
reset init` can reset/halt the CPU and change outputs. The upstream examine-end
handler writes DBGMCU_CR to freeze watchdogs while halted; Flash programming uses
SRAM and a driver loader. This is a reviewed programming/debug stage, never evidence
that watchdogs/interlocks have been qualified. Do not attach during MEASURE or a
Flash transaction. The tool never resumes/boots automatically: its last reported
target state is halted; actual post-disconnection behavior needs bench observation.
Disconnect the debugger and arrange a separately authorized controlled boot
or power cycle with the UART listener ready. Recheck actual SAFE behavior afterward.
If failure/timeout leaves the state ambiguous, keep fixtures unchanged and power
down through the reviewed procedure; never assume a killed host process made GPIO safe.
A05-Y04 analog clamp/backfeed risk remains pending until actual independent traces.

## Prepare the images today

From repository root, using clean source for physically deployable artifacts:

```powershell
python Firmware/tools/pc_firmware_deploy.py doctor --openocd C:/tools/openocd/bin/openocd.exe --scripts C:/tools/openocd/openocd/scripts
python Firmware/tools/pc_firmware_deploy.py build --profile BRINGUP --out Firmware/build/handoff-bringup
python Firmware/tools/pc_firmware_deploy.py build --profile BRINGUP_CAL --out Firmware/build/handoff-cal
python Firmware/tools/pc_firmware_deploy.py build --profile FACTORY_PRODUCT --out Firmware/build/handoff-product
python Firmware/tools/pc_firmware_deploy.py inspect --profile FACTORY_PRODUCT --manifest Firmware/build/handoff-product/image/manifest.json
```

Each build uses a NEW output directory, CMake/Ninja and the actual preset:
stm32-bringup, stm32-bringup-cal or **stm32-factory-release**. Curves are explicitly
OFF; factory mode is ON only for factory PRODUCT. Defaults/legacy presets are not
changed. Legacy fallback remains independently buildable with stm32-release/debug.
For the owner-requested empty-fixture relay diagnostic, `build --profile BRINGUP
--k1-bench-test` explicitly selects `WTK_BRINGUP_K1_TEST=ON` and excludes the
experimental DC pilot for Flash space. All ordinary builds explicitly set it OFF.
The manifest's `bringup_k1_test` field must match the generated feature flag;
legacy manifests mean OFF. This option never authorizes a physical relay pulse.
Follow [the dedicated operator procedure](K1_BENCH_TEST.md) after controlled boot.
Build outputs must be ignored (Firmware/build) or outside the repository; don't
introduce generated source changes during compilation. Source/dirty fingerprint is
checked before/after build. Dirty artifacts are inspectable but refused for physical
programming. No `--force` or dirty-image override exists.

The image directory contains manifest.json, firmware.bin/.elf, generated build
config and linker snapshot. The manifest includes full Git SHA, source dirty/status/
fingerprint, preset/profile/factory/curves, compiler/CMake/cache provenance, version/
expected boot interface, timestamp, linked Flash/accounted RAM/stack, BIN/ELF hashes
and 64KiB/20KiB addresses. Inspect reconstructs BIN from ARM ELF load segments,
checks SP/reset vectors, linker/configuration, sizes and hashes. Renaming a legacy
image cannot make it factory PRODUCT. A valid seal detects alterations, not producer
authenticity; use a trusted locally generated handoff. Older pinned clean artifacts
are allowed if their declared configuration/bytes match, not silently rebuilt.

## Dry run and controlled first target query

```powershell
python Firmware/tools/pc_firmware_deploy.py program --dry-run --profile BRINGUP --manifest Firmware/build/handoff-bringup/image/manifest.json --openocd C:/tools/openocd/bin/openocd.exe --scripts C:/tools/openocd/openocd/scripts --adapter-serial YOUR_STLINK_SERIAL --out Firmware/build/program-plan
```

Dry run never launches OpenOCD or accesses a target, including a fake target. It
prints exact argument arrays, environment DATA, programmed range and page-aligned
erase scope. Build dry run also avoids compiler execution. Runtime paths are never
constructed into shell/Tcl code; they enter fixed scripts as environment values.

After physically reviewed wiring/power/SAFE stage:

```powershell
python Firmware/tools/pc_firmware_deploy.py probe --session C:/bench/b01.json --board BOARD_ID --operator OPERATOR --supply "Reviewed independent current-limited supply" --ack-safe-outputs --ack-debug-halt-watchdog --openocd C:/tools/openocd/bin/openocd.exe --scripts C:/tools/openocd/openocd/scripts --adapter-serial YOUR_STLINK_SERIAL --out Firmware/build/probe-001
```

Record the reported UID. Device ID/core, factory size word, one stm32f1x bank at
0x08000000/65536B and readout/option/write-protection status must match. A supported
adapter's reported voltage is captured when present; absent voltage is UNKNOWN,
not a fabricated 3.3V reading. Adapter voltage does not replace independent DMM
rail evidence. Protection causes STOP with no unlock/option-byte write.

## Explicit program + exact readback

The command requires selected profile/manifest, B01 session/report, board identity,
operator/supply, previous stage, target UID and four acknowledgements. Initial
BRINGUP requires assembly/R02/R05/power/rail/module PASS evidence; later images also
require reviewed safe_boot. Actual SAFE programming outputs are separately
acknowledged each time, so initial blank-board programming does not require a
fictional prior successful firmware boot. B01 STOP/UNKNOWN diagnostics block work;
synthetic/replayed evidence cannot authorize the real backend. B01 is never modified.

Read the exact `firmware.bin` SHA from inspect. Approval phrase:
`PROGRAM <PROFILE> <first 12 BIN-SHA digits> <24-hex target UID>`.
It binds the reviewed action to the actual image/UID; `yes` is insufficient.
Example syntax (replace all capital placeholders with actual reviewed evidence):

```powershell
python Firmware/tools/pc_firmware_deploy.py program --profile BRINGUP --manifest Firmware/build/handoff-bringup/image/manifest.json --session C:/bench/b01.json --board BOARD_ID --operator OPERATOR --supply "Reviewed sole current-limited source; measurements in B01" --previous FACTORY_BLANK --expected-uid TARGET_UID --ack-safe-outputs --ack-debug-halt-watchdog --ack-no-app-bootloader --ack-internal-flash --accept-internal-flash "PROGRAM BRINGUP BIN_SHA_PREFIX TARGET_UID" --openocd C:/tools/openocd/bin/openocd.exe --scripts C:/tools/openocd/openocd/scripts --adapter-serial YOUR_STLINK_SERIAL --out C:/bench/deployment-001
```

The no-app-bootloader acknowledgement requires owner knowledge that the internal
Flash is the Rev.1 application area, not an existing application bootloader.
The STM32 ROM bootloader/option bytes are never written. If this cannot be established,
STOP. Recovery needs a separately reviewed stage, not an erase/unlock bypass.

Fresh probe validates target/reported voltage before any program invocation; the
program process repeats UID/geometry/protection checks before erase. **Each occupied
1KiB page is erased individually**, including all 64 pages for BRINGUP, then
`flash write_bank 0 ... 0` writes without an erase flag. This avoids the STM32F1
driver's whole-bank mass-erase optimization. This IS page erase, not "no erase";
the last partial page's unused bytes are erased. No mass erase/unprotect/option
write, external SPI access, reset-run, auto capture/install or GPIO command exists.

Readback dumps all 64KiB: the image prefix is compared byte-for-byte and hashed;
pages outside the approved erase scope must remain identical to the before snapshot.
Programming, readback, boot, UART identity and application acceptance are separate.
Success proves only internal Flash bytes; boot/application stay NOT_TESTED.
Unique output directories are atomically reserved; no blind retry or duplicate job.
Failure/partial write/missing completion/timeout always produces FAILED_OR_UNKNOWN
with available raw transcript/dumps, never a successful deployment.

Read-only verification (also resets/halts; separately reviewed stage):

```powershell
python Firmware/tools/pc_firmware_deploy.py verify --manifest Firmware/build/handoff-bringup/image/manifest.json --expected-uid TARGET_UID --session C:/bench/b01.json --board BOARD_ID --operator OPERATOR --supply "Reviewed independent current-limited supply" --ack-safe-outputs --ack-debug-halt-watchdog --openocd C:/tools/openocd/bin/openocd.exe --scripts C:/tools/openocd/openocd/scripts --adapter-serial YOUR_STLINK_SERIAL --out C:/bench/verify-002
```

## Profile transitions and subsequent evidence

1. **BRINGUP:** verify internal bytes, prepare pc_bringup.py listener before manual
   boot. Record the full actual banner/diagnostics. Do not auto-execute active commands.
2. **BRINGUP_CAL:** after reviewed initial safe boot, program the separate image,
   preserve W25Q, manually boot, then existing pc_cal_capture.py identify/status.
   The profile has PLC1 identity, not the text console. Use the OSL workbench only
   after its physical operator gate. Capture/install remain separately authorized.
3. **Factory PRODUCT:** program with `--previous BRINGUP_CAL`, `--installation
   C:/bench/installation.json --cal-readback C:/bench/installed.bin`. Existing
   canonical validator checks schema2/model5/all33 keys, actual sequence/CRC,
   installation success/readback hashes and MCU UID. Synthetic installation cannot
   authorize physical PRODUCT. JSON consistency is **not a fresh W25Q check or an
   accuracy certificate**; owner independently reviews provenance before switching.
   Optional `--resource-evidence` retains an external report/path/hash, but cannot
   invent a Resource Pack query or readiness status; missing resources remain UNKNOWN.
   PRODUCT only reports a limited SAFE_BOOT banner; verify remaining readiness and
   standalone passive operation on its actual UI/bench plan. No native USB switch.

The same program command substitutes profile/manifest/previous stage and an image-
specific approval each time. Nothing automatically advances the stage.

```powershell
python Firmware/tools/pc_firmware_deploy.py report --deployment C:/bench/deployment-001/deployment.json --observed C:/bench/bringup-report.json --out C:/bench/observed-boot.json
python Firmware/tools/pc_firmware_deploy.py report --deployment C:/bench/deployment-001/deployment.json --programmer-transcript C:/bench/actual-openocd.log --out C:/bench/transcript-sidecar.json
```

Report imports actual B01/PLC1 identities through their existing field contracts,
without converting them to application PASS. Unrecognized native programmer logs
are retained with hash/UNKNOWN rather than guessed. Reports do not prove target
authenticity, waveform safety, OSL physical accuracy or Resource Pack readiness.

## Failure/recovery and offline tests

Preserve deployment.json/.md, transcripts, before/full/image dumps and manifests.
Do not resume a partly written image. Power/SAFE/adapter/NRST/SWD must be reviewed
again. A fresh inspect/probe/verify identifies what is known; a deliberately reviewed
RECOVERY_REVIEWED program uses a NEW destination and explicit image/UID approval.
It erases only required individual pages. If identification, protection, power or
halt remains ambiguous, STOP; no indiscriminate erase, unlock or alternate geometry.
W25Q/calibration was not accessed by the toolkit. Independently check its contents
after actual controlled boot; fake preservation tests are not physical proof.

`--backend fake --fake-device Firmware/build/fake-swd` provides deterministic internal
Flash orchestration, always SIMULATED. It needs explicit fixture/B01 acknowledgements,
but never an adapter. Never use its reports as physical prerequisites. Unit/integration
tests exercise real ELF outputs and the production Tcl with API mocks under actual
OpenOCD's Jim interpreter **without target/adapter init**; native hardware is untested.

```powershell
$env:WTK_DEPLOY_IMAGES=(Resolve-Path Firmware/build/test-images)
$env:WTK_CAPTURE_BRIDGE=(Resolve-Path Firmware/build/host-release/tests/Release/wtk_pc_cal_capture_bridge.exe)
$env:WTK_TEST_OPENOCD='C:/tools/openocd/bin/openocd.exe'
python -m unittest discover -s Firmware/tests/tools -p test_pc_firmware_deploy.py -v
```

Generated image, dump, programmer binary and bench evidence directories stay ignored
or outside Git. OpenOCD executable/source/config provenance and all remaining hardware
gates are recorded in the [B01.1 evidence report](../../docs/review/b01/B01.1-report.md).
