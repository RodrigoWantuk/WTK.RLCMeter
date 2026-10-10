# A04.1 — guided OSL campaign evidence

Status: **IMPLEMENTED_TESTED_HOST / REQUIRES_BENCH_VALIDATION**. PCB unassembled;
no physical UART, electrical capture, safety or accuracy qualification is claimed.

Starting main: `360f4972d3fdecd32f86dfd12ee3259af734c581`. PR #16 was confirmed
merged before inspection. Clean checkout preserved; fetch/prune, main ff-only,
recursive submodule initialization and branch `codex/a04-guided-osl-campaign`.
Tools: Python 3.9.5, optional pyserial 3.5, Arm GCC 14.2.1 (20241119), CMake 4.4.4,
MSVC 19.44.35228.0. No firmware, CMake/linker, protocol, PCB or memory-gate edits.

## Delivered implementation

- [Campaign backend/CLI](../../../Firmware/tools/pc_osl_campaign.py): editable
  six-LOAD inventory; standard/condition plans; explicit confirmations; atomic
  bounded progress; raw CCO1 CRC/SHA, device/firmware/ADC/reference/sequence binding;
  rejection history and non-destructive recapture; existing solver/serializer and
  strict installer candidate validation; Markdown/JSON/audit/raw export.
- [Windows instructions](../../../Firmware/tools/PC_OSL_CAMPAIGN.md) and
  [47 campaign tests](../../../Firmware/tests/tools/test_pc_osl_campaign.py).
  Existing individual capture, solver and installer behavior remains unchanged.
- Plans 14/15 reflect the terminal OSL subset delivered; GUI, scope/curves and
  independent physical holdouts remain open. Generated campaigns stay outside Git.

Default order: 33 OPEN, 33 SHORT, 33 LOAD grouped by six ascending RREF ranges.
Eight fixture setups / seven changes, **99 explicit CAPTURE confirmations**, no
timer-triggered or unattended physical capture. A pause/resume needs confirmation
only for remaining observations. Fixture identity is repeated at every prompt.
Six affordable ordinary 1% LOADs are allowed, with printed intervals retained;
actual values require independent provenance and measured intervals remain optional.
No qualified flags or fabricated uncertainty/capture values.

Fresh identity and safety status precede START. The only serial commands are
IDENTIFY, STATUS, START, RESULT, CANCEL and read-only INSTALL_STATUS for successor
sequence. No install BEGIN/CHUNK/VALIDATE/COMMIT, GPIO/range/relay/reset command.
Cancellation handles a lost START reply without acquisition replay. Communication
ambiguity ends the run; independent firmware timeout still owns safe teardown.
Per-capture quality includes source/return peak amplitudes, usable 1X/HG, clipping,
HG overlap, stable repeats, temperature if available and actual nominal ADC fields.
Degenerate triplets and both-invalid paths cannot complete a condition. A usable
1X record can survive HG clipping; the existing solver owns final path selection.

Progress saves use same-directory flush/fsync/replace, a checksum seal and an
OS-released lock. Each loaded artifact is decoded canonically again. Stale writers
cannot silently remove newer evidence. Explicit replacement retains the old raw
artifact/hash in the audit. The 8-MiB input/output bound fails closed; hashes detect
accidental alteration, not physical authenticity or an attacker recomputing them.
The protocol advertises an eight-character Git identifier, not a full deployed SHA
or boot counter; the owner must retain the exact SWD image SHA in the bench log.

## Reproducible tests and demonstration

```powershell
python Firmware/tools/product_factory_matrix.py --host --out Firmware/build/a041-host
python Firmware/tools/pc_osl_campaign.py simulate --bridge Firmware/build/a041-host/host-debug-factory-OFF-curves-OFF/tests/Debug/wtk_pc_cal_capture_bridge.exe --out Firmware/build/a041-demo-final
python Firmware/tools/product_factory_matrix.py --out Firmware/build/a041-baseline
python Firmware/tools/product_factory_matrix.py --out Firmware/build/a041-final
python PCB/tools/audit_rev1.py --check
python Firmware/tools/run_virtual_tests.py --check-only
python Firmware/tools/run_virtual_tests.py --lint-only
```

The C-backed demonstration starts blank, collects 42, atomically saves, resets the
fake device, reconnects/negotiates IDs, resumes at 43 and preserves the first 42
artifact hashes. All 99 produce 33 complete conditions and a canonical **2760-byte
schema-v2/model-v4 candidate, sequence 1**, accepted by the existing installer
validator. The device remains blank and install state IDLE; nothing is installed.
Serial bytes exercise the actual embedded C parser/session/DSP, with synthetic
electrical acquisition. Every generated bundle is SIMULATED / UNQUALIFIED.
The deterministic candidate SHA-256 is
`cb8cc2cf8cb81ca8f95e850a8745164185f232e703fb5f6d412632542b75ae66`;
schema CRC `a2c0e7d2`, whole-frame transfer CRC `c1791d53`. These are distinct
checksums. Offline inspect/build/report and pyserial 3.5 port enumeration pass;
no physical port was opened. COM settings are verified with host mocks.

New tests cover inventory/interval/provenance validation, order/coverage, no false
completion, atomic failure/concurrent writers, raw tampering, wrong firmware/device/
profile/ADC/ref/condition, charger/residual rejection, invalid acquisition, saturation,
weak source, degenerate triplet, timeout/disconnection, canceled/lost-ack capture,
explicit retry and recapture, sequence change, COM settings, reports and C99 resume.
All eight host combinations pass: Debug/Release × legacy/factory × curves OFF/ON.
**336 CTest invocations and 2448 Python test invocations**, no final failures or
skips; Python is **306 tests per combination**, including 47 new campaign tests
with the C fixture active. CTest counts are legacy 40/43 OFF/ON and factory 41/44
OFF/ON in both build types. The final campaign suite was rerun in the five earlier
configurations after a metadata-validation ordering fix; the remaining three
Release full suites include that fix. Existing PC/C solver/golden-vector, Resource
Pack, capture, installer and actual PRODUCT runtime tests remain passing, including
the 66 store-transition / 125 partial-NOR reset-cut sweep per Python configuration.
All twelve ARM builds pass A01 linked-size/profile/symbol gates. Seventeen PCB
reconciliation tests and document consistency checks pass; `git diff --check` passes.

Wokwi local file checks, custom-chip compilation and CLI lint pass. Smoke was
attempted but **not executed: WOKWI_CLI_TOKEN is absent**. No scenario pass or
physical qualification is implied. Hosted CI status is reported separately.
PR #17's Flash-forensics and Virtual-Hardware jobs did not start: GitHub annotations
report **account locked due to a billing issue**. This external account blocker
is separate from the passing local matrix and absent local Wokwi token.

## Fresh memory evidence

[sizes.csv](sizes.csv) records all twelve matched-starting-SHA ARM images. Every
Flash/accounted-RAM/stack delta is **zero**; PRODUCT legacy/factory and curves
OFF/ON remain separately buildable. BRINGUP retains its **88-byte** physical Flash
margin. No new embedded allocation, stack cost or buffer lifetime is introduced.
All twelve matched-starting-SHA programmed binaries are byte-identical. Debug ELF
containers may differ because of build paths; rebuilding at the feature commit
changes Git banner metadata, which is not a new firmware feature or memory cost.

| Profile / curves | Before = after Flash B | Before = after accounted RAM B | Stack B |
| --- | ---: | ---: | ---: |
| Legacy Debug OFF / ON | 58,700 / 61,312 | 14,536 / 14,600 | 2048 |
| Legacy Release OFF / ON | 62,044 / 64,468 | 14,984 / 15,048 | 2052 |
| Factory Debug OFF / ON | 51,728 / 54,208 | 14,832 / 14,896 | 2052 |
| Factory Release OFF / ON | 52,648 / 55,124 | 15,040 / 15,104 | 2052 |
| BRINGUP OFF / ON | 65,448 | 14,596 | 2048 |
| BRINGUP_CAL OFF / ON | 31,944 | 12,744 | 2048 |

## Remaining physical gates

Use A05/B01 evidence first: R02 backlight link open until characterized; R05 module
pinout/power/SWD/HSE inspection; mandatory bypass/bank links and K2/TVS DNP;
identified power modules/buzzer; current-limited rail/safe-boot/charger/residual/K1/
range checks; actual 5V MCP6002/BAT54S clamp injection into 3V3. No scope GND at
VMID, floating earth, energized DUT or unapproved DC. Firmware cannot resolve the
unmeasured analog injection.

Then characterize source/ADC/DMA timing, clipping, effective HG overlap, leakage,
probe/fixture/contact repeatability, actual references and temperature. A physically
unobservable mandatory key must stay unresolved, not get fabricated coefficients.
Validate independent R/C/L holdouts and actual W25Q installation/readback/reset/
interruption; deploy factory PRODUCT through SWD preserving W25Q and test standalone
operation. This software subset neither retires the legacy wizard nor certifies
absolute error. The smallest remaining step is assembly/power evidence, followed
by one safely authorized OPEN capture through this guided tool before a full run.
