# Audit evidence and host-only reproductions

These files support [the production audit](../README.md). They are deliberately
outside the firmware build/test tree and do not modify or access the device.
Baseline source is `842276976c7f44a5fcb3b7ed287325428f01668d`.
`manifest.json` hashes every other file in this evidence directory.
The local `.gitattributes` preserves evidence bytes across platform checkouts.
Archived text uses LF line endings, normalized before fingerprinting.
Source fingerprints distinguish the captured Windows worktree bytes from the
baseline Git blob bytes, since source checkout newline rules can differ.

## Recorded checks

| Evidence | Meaning |
| --- | --- |
| factory-ctest.log | Existing factory host suite, 50/50 entries PASS |
| standard-ctest.log | Existing standard host suite, 48/48 entries PASS |
| settings_contract.c / repro-results.log | Unchanged settings service, three confirmed defects |
| product_idle.c / repro-idle-results.log | Actual SAFE-idle RANGE blocker defeats 15-s timeout |
| CMakeLists.txt / replay-ctest.log | Standalone host audit probes; expect the audited bugs |
| lead_math.py / lead-compensation-math.json | 108 synthetic double-precision composition cases |
| sizes.c / sizes.s / memory-audit.json | Arm GCC ABI sizes and current deployed memory accounting |
| strict-profile-sizes.json | Prior strict build matrix; not rerun by this audit |
| source-hashes.json | Exact hashes of the principal source files underlying the findings |

The probe callbacks are explicit synthetic host I/O. In particular,
`settings_contract.c` emulates the real driver's accepted-start BUSY contract;
it does not directly talk to W25Q or demonstrate physical Flash endurance.
The sequence-corruption case uses the existing accepted-OK fake convention to
isolate it from the earlier BUSY stall. The idle probe reuses the current host
product fixtures/library with real idle safety fields. No production-only fake
sample path was added.

**A successful audit probe asserts that a defect still exists at the baseline.**
When implementing the fixes, convert these into normal correctness regression
tests; do not preserve the faulty behavior merely to keep these probes passing.

## Reproduce on the audited Windows/MSVC setup

From the repository root, create a factory PRODUCT host build if needed:

```powershell
cmake -S Firmware -B Firmware/build/audit-host-factory -G "Visual Studio 17 2022" -A x64 -DWTK_TARGET_PLATFORM=host -DWTK_PRODUCT_FACTORY_PROVISIONED=ON
cmake --build Firmware/build/audit-host-factory --config Debug
ctest --test-dir Firmware/build/audit-host-factory -C Debug --output-on-failure
cmake -S docs/review/production-audit-2026-10-27/evidence -B Firmware/build/audit-probes -G "Visual Studio 17 2022" -A x64 -DFACTORY_HOST_BUILD="$PWD/Firmware/build/audit-host-factory"
cmake --build Firmware/build/audit-probes --config Debug
ctest --test-dir Firmware/build/audit-probes -C Debug -V
python docs/review/production-audit-2026-10-27/evidence/lead_math.py
```

The recorded replay used the existing factory build at
`Firmware/build/calibration-20261025/product-scheduling-host-factory` and explicitly
selected that path. Standard host testing used the sibling `ntc-host-tests`
directory. Assertions are enabled in the audit probes. The probes were compiled
with MSVC `/W4 /WX`; C17 is requested. The math script uses Python standard
library only and can run without the embedded toolchain.

`sizes.s` is compiler output from `sizes.c` with Arm GNU GCC 14.2.1, Cortex-M3
Thumb, the installed factory generated configuration/includes and CMSIS headers.
These are **Arm target ABI sizes**, not MSVC 64-bit `sizeof` guesses. Type sizes
include nested objects; do not add all entries together as separate allocations.
`memory-audit.json` distinguishes physical capacity from current hard budgets.

The existing embedded matrix and deployed image are independently recorded in
[serial OK delivery](../../../../Firmware/releases/2026-10-27/serial-ok-control/README.md).
Actual 128-KiB first-unit erase/program/reset/execution evidence is in
[the capacity experiment](../../../../Firmware/releases/2026-10-27/extended-flash-capacity/README.md).
Neither experiment nor deployment was repeated for this source audit. No new
claim of physical accuracy, timing, electrical safety qualification or polarized
capacitor support follows from the host checks.
