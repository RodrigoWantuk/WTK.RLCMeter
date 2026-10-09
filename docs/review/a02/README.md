# A02.1 — Optional supplementary-curve runtime

STATUS: COMPLETE — IMPLEMENTED_TESTED_HOST / COMPILES_TARGET.
Physical qualification: REQUIRES_BENCH_VALIDATION; the PCB is not assembled.

Evidence date: 2026-10-09. Started from clean, fetched and fast-forwarded `main`
at **5bcfba229e0a7cbcb0725a1a7c17418b0c1b72b8**, the confirmed merge of PR #8.
Recursive submodules were initialized before analysis. Branch:
`codex/a02-curve-profile-isolation`. Measured firmware implementation:
**0dabd8aebe117ce1b2300445b087a00d8736fe51**. Later commits update evidence/tooling,
not firmware behavior. Binary hashes identify the exact measured artifacts.

## Result and boundary

PRODUCT Release now defaults to **61,580 B Flash / 14,976 B accounted RAM**.
The measured saving is **2,516 B Flash / 64 B RAM**, exactly the A01 probe result.
This is useful recovery, but does not by itself fit all remaining mandatory work.

- CMake's `WTK_ENABLE_SUPPLEMENTARY_CURVES` defaults OFF for all targets and
  generates the corresponding 0/1 configuration constant. ON selects the original
  `measurement_cal_curve.c`, `measurement_cal_curve_frame.c`, and
  `measurement_cal_curve_store.c`; OFF does not compile those sources.
- `app_shell.c` gates the PRODUCT curve store, boot load, W25Q read adapter and
  post-OSL read/apply path with that same capability. The OSL process call remains
  outside the gate. No alternative math, timing, safety or storage policy is added.
- `measurement_cal_supplementary_supported()` reports build support.
  `measurement_cal_apply_curve()` returns `MEASUREMENT_CAL_CURVE_NOT_SUPPORTED`
  when OFF, without dereferencing inputs or modifying the result. Existing enum
  values remain unchanged; the new value is appended and is not persisted.
- A fresh `measurement_cal_process_block()` clears `supplementary_applied`.
  Only successful enabled application sets it. OSL qualification remains distinct
  from supplementary application. Current UI does not advertise curves as applied.
- The three optional modules and the enabled apply body are otherwise unchanged.
  Headers, WCRV v1 format, reserved A/B sectors, schema v2/model v4 OSL records,
  workspace ownership and recovery protocols remain compatible. No records are
  erased or migrated. Future PC provisioning can reuse the ON runtime.

The unsupported stub and inline capability query contribute **0 additional linked
Flash and 0 RAM** in these PRODUCT images: they are unused/removed by LTO and GC.
No new runtime state was added. Re-enabling costs **2,516 B Flash and 64 B RAM**
relative to Release OFF; installation/provisioning is a separate future cost.

## Stored-data behavior

All rows assume the ordinary OSL prerequisites are satisfied. Missing/invalid OSL
still blocks calibrated PRODUCT measurement through the existing boot gate.

| W25Q / measurement state | Curves ON | Curves OFF |
| --- | --- | --- |
| Qualified valid compatible WCRV, matching key, in domain | Load selected A/B frame; apply after OSL; set `supplementary_applied` | Do not inspect/read WCRV; OSL-only; applied flag false |
| Missing records or missing condition | OSL-only; inactive store or read `NOT_SUPPORTED` | Same OSL-only output; capability unsupported |
| Unqualified frame, even if newer | Reject it; use an older valid qualified frame if available | Do not inspect; records preserved |
| Stale OSL sequence or CRC | Reject at load / return `NOT_SUPPORTED` at lookup; OSL-only | Do not inspect; records preserved |
| Corrupt frame/CRC/commit marker at boot | Try other valid slot; otherwise inactive and OSL-only | Do not inspect; records preserved |
| Corrupt record or read failure after activation | Preserve existing `CALIBRATION_UNAVAILABLE` / error behavior | No curve read occurs |
| Out-of-domain impedance | Skip optional correction; successful OSL result retained | OSL-only |
| Other apply error / nonfinite correction | Preserve existing `CALIBRATION_UNAVAILABLE` / error behavior | Explicit apply request returns `NOT_SUPPORTED`, result unchanged |
| Previously provisioned records after OFF then ON firmware | Revalidate original qualified flag, sequence, CRC and condition key; no migration | Leave both reserved sectors intact |

The existing load-at-boot policy is retained; this task does not add hot installation
or reload. After an OSL set changes, a mismatched old curve is skipped, not silently
requalified. Qualification still requires independent physical evidence.

## Fresh measurements

The before-change run completed host Debug/Release CTest and Python tests before
any task edits. All three ARM baselines reproduce A01 exactly. Same local compiler:
Arm GNU 14.2.Rel1 / GCC 14.2.1, binutils 2.43.1, CMake 4.4.4, Ninja
1.13.2.git.kitware.jobserver-pipe-1; Python 3.9.5. Host: MSVC 19.44.35228.0,
Visual Studio 17 2022 x64 generator. Existing optimization, LTO and linker limits
are unchanged. `WTK_FLASH_FORENSICS=ON` supplies LTRANS stack evidence.

| Image | Flash before | Flash after | RAM before | RAM after |
| --- | ---: | ---: | ---: | ---: |
| PRODUCT Debug OFF | 60,964 | 58,396 | 14,592 | 14,536 |
| PRODUCT Release OFF | 64,096 | 61,580 | 15,040 | 14,976 |
| PRODUCT Release ON | 64,096 | 64,096 | 15,040 | 15,040 |
| BRINGUP default OFF | 65,052 | 65,052 | 14,588 | 14,588 |

Release OFF leaves **3,956 B** to silicon and **2,932 B** to the existing 64,512 B
PRODUCT gate. ON leaves 1,440 B / 416 B respectively and is supported by both
gates. Debug OFF also passed an explicit PRODUCT budget check. BRINGUP remains
within its existing gate; its application never had the PRODUCT curve path.
No larger MCU, stack reduction or relaxed memory gate is used.

| Release section | ON / before | OFF | OFF minus ON |
| --- | ---: | ---: | ---: |
| `.text` | 61,760 | 59,240 | -2,520 |
| `.rodata` | 2,044 | 2,048 | +4 |
| `.isr_vector` | 236 | 236 | 0 |
| `.ARM` | 8 | 8 | 0 |
| `.data` | 48 | 48 | 0 |
| `.bss` | 12,940 | 12,876 | -64 |
| `.noinit` | 0 | 0 | 0 |
| stack/heap reservation including alignment | 2,052 | 2,052 | 0 |

BIN lengths equal stored Flash totals; load gaps are zero. RAM counts each NOLOAD
section once. [All profile sections](sections.csv) and compact [baseline](baseline/)
/ [candidate](candidate/) snapshots include checksums and compiler metadata.

## Code, module and stack evidence

Release ON's entire BIN is identical to baseline after replacing only the 12-byte
Git revision field at offset 63,790. This verifies the compiled enabled numerical
path as well as the unchanged golden-vector tests. BRINGUP has the same memory;
its only other byte differences are eight pointers to the same string `"1"`, now
pooled into the revision string's suffix. [Binary comparison](binary-equivalence.json)
records exact hashes and offsets; no other differences were found.

Byte-identical `-g1` twins enrich ON/OFF symbol origins. These are emitted function
origins including inlined callees, not isolated feature sizes. Major OFF deltas:

| Emitted symbol / source origin | Delta bytes |
| --- | ---: |
| `Reset_Handler` (includes inlined boot curve loading) | -860 |
| `product_auto_process_block` | -810 |
| standalone `measurement_derive_quantities` | -628 |
| `product_curve_flash_read` | -84 |
| `storage_layout_partition` | -52 |
| `product_active_osl_crc32` | -28 |

Derived quantities still execute through retained/inlined OSL code; disappearance
of a standalone symbol does not mean the feature was removed. Source-origin totals
include app shell -934 B, startup -860 B, DSP -636 B and storage layout -52 B,
with smaller positive and negative changes from LTO/inlining/alignment. Library
families remain unchanged. [Module deltas](release-module-delta.json),
[raw symbol deltas](release-symbol-delta.csv), and
[LTO-private-name-normalized deltas](release-symbol-delta-normalized.json) retain
the details without counting renamed clones as unrelated removals/additions.

The post-build profile tool now checks both symbol exclusions and membership of
all three curve sources in `compile_commands.json`. OFF has no curve runtime
sources or surviving curve-store/read/interpolation symbols. ON contains all three
sources; no required standalone symbol is assumed under LTO. The compatibility
apply stub is allowed but absent from the final OFF image.

| Release individual static stack frame | Before / ON | OFF |
| --- | ---: | ---: |
| calibration session step | 888 | 888 |
| calibration wizard step | 704 | 704 |
| `app_step` | 448 | 448 |
| `product_auto_process_block` | 440 | 344 |
| `Reset_Handler` | 120 | 104 |

[Stack frames](stack-frames.csv) include other profiles. The reservation remains
2,048 B plus alignment; these are individual frames, not a proof of the maximum
nested/interrupt stack. Watermarking under real operation remains a bench gate.

## Regression evidence and limits

| Check | Result |
| --- | --- |
| Baseline host Debug / Release | 40/40 each |
| Baseline Python | 149 passed |
| Candidate host Debug OFF / Release OFF | 37/37 each |
| Candidate host Debug ON / Release ON | 40/40 each |
| Candidate Python, including composition and read-only capture tests | 155 passed |
| ARM Debug OFF, Release OFF, Release ON, BRINGUP OFF | Build, size and profile checks pass |
| Release ON/OFF `-g1` twins | Both BIN-identical to their normal candidate image |
| Wokwi `--check-only` / `--lint-only` | Pass; custom W25Q chip compiled |
| Full Wokwi scenario execution | Not run: no local `WOKWI_CLI_TOKEN` |
| Real hardware / metrology / stack watermark | Not run: PCB unassembled |
| Hosted CI | Blocked before execution by account billing; zero steps ran |

OFF excludes exactly the three dedicated curve math/frame/store executables;
the full remaining suite runs in both variants. ON retains original identity,
log-interpolation, complex cross-coupling, out-of-domain and nonfinite golden
vectors. Both variants run identical synthetic OSL input and reference expectations,
normal component classification and valid/invalid quantity tests. Added tests check
that fresh OSL clears a stale applied flag; disabled apply returns unsupported and
preserves the entire result; and enabled storage rejects CRC/key mismatches, stale
OSL, corrupt slots, interrupted commits and unqualified data without rewriting it.

The shared suites cover W25Q/A-B persistence, resource recovery, PC-link framing,
flash access ownership, autorange, abort/cancel, range sequencing, charger inhibition,
safety faults, UI and calibration boot/workflow states. Firmware changes do not touch
these policies, the full OSL wizard, ADC/DMA/excitation or serialization. A test-only
range-enum typo was found and corrected during the first ON build; the final matrix
has no failures. Host warnings-as-errors pass. ARM emits only existing serial-LTO
and informational 48 KiB soft-target warnings; hard gates pass.

Separate commits reconcile plan 10 with O01–O07 and replace the old workflow's
`stm32-lab` with `stm32-bringup`. Its exact configure/build commands and local
Wokwi check/lint passed. The forensics workflow now tests both host capabilities
and an enabled PRODUCT image; hosted availability is reported separately.

[PR #9](https://github.com/RodrigoWantuk/WTK.RLCMeter/pull/9) is open and unmerged.
The [Flash forensics run](https://github.com/RodrigoWantuk/WTK.RLCMeter/actions/runs/37983342610)
and [Virtual Hardware run](https://github.com/RodrigoWantuk/WTK.RLCMeter/actions/runs/37983342703)
both failed before any job steps executed. Their annotation states: "The job was
not started because your account is locked due to a billing issue." This is an
external execution blocker, not successful hosted validation.

Evidence tooling also received a read-only capture fix: old target-help capture
regenerated baseline build graphs after source edits. Baseline ELF/BIN/map/size and
captured configuration snapshots remained intact. Their post-capture generated
headers/command listings were therefore not used as baseline build provenance.
Baseline Git identity is verified in the actual BIN. New Ninja tool-mode capture
was tested to leave cache, Ninja graph, generated config and ELF hashes unchanged.

## Forward completion budget and next ticket

[Current feature budget](feature-budget.csv) supersedes A01's planning CSV for new
estimates. Remaining MUST increments are unchanged: **2,984 / 6,408 / 12,856 B**
optimistic/central/conservative, including PC provisioning replacement and
maintenance contingency. A02's 2,516-byte saving is already in the OFF baseline;
do not subtract it again. Runtime restoration is a separate measured optional row.

| Forecast | Optimistic | Central | Conservative |
| --- | ---: | ---: | ---: |
| Current OFF + remaining MUST | 64,564 | 67,988 | 74,436 |
| OFF + hypothetical A06 gross recovery + MUST | 57,564 | 62,988 | 71,436 |
| Same A06 scenario + runtime re-enable | 60,080 | 65,504 | 73,952 |

The hypothetical A06 removals are **7,000 / 5,000 / 3,000 B**, not measurements.
Replacement provisioning is already in the MUST increments. Optional curve
installation adds another 1,024 / 2,048 / 3,584 B beyond re-enabling its reader.
The current central trajectory exceeds silicon by 2,452 B. Even the hypothetical
A06 central scenario with curves restored leaves only 32 B physical headroom and
exceeds the current PRODUCT gate by 992 B; it is not a release-ready budget.
[Machine-readable forecast](forecast.json) preserves assumptions and totals.

Next ticket: **A06 — review and implement complete PC OSL provisioning migration**,
including safe first calibration, raw capture/transfer, fit/validation, transactional
receive/readback, recovery from interruption, and standalone normal measurements
after provisioning. Establish the replacement and migration states before removing
the current on-device full OSL wizard. OPEN/SHORT trim must never masquerade as an
absolutely qualified OSL set. Physical safety, error bounds, relay behavior, ADC
timing, SNR and curve benefit remain REQUIRES_BENCH_VALIDATION.

## Reproduction

Run from the repository root; keep baseline and candidate directories separate.
Capture the baseline at its recorded SHA before editing. Host matrix (PowerShell):

```powershell
foreach ($preset in @('host-debug', 'host-release')) {
    foreach ($curves in @('OFF', 'ON')) {
        $dir = "Firmware/build/a02-$preset-$curves"
        $config = if ($preset -eq 'host-debug') { 'Debug' } else { 'Release' }
        cmake --preset $preset -S Firmware -B $dir -G "Visual Studio 17 2022" "-DWTK_ENABLE_SUPPLEMENTARY_CURVES=$curves"
        cmake --build $dir --config $config --parallel 6
        ctest --test-dir $dir -C $config --output-on-failure
    }
}
python -m unittest discover -s Firmware/tests/tools -v
```

ARM: repeat the following with `stm32-debug/OFF`, `stm32-release/OFF`,
`stm32-release/ON`, and `stm32-bringup/OFF` (baseline predates the capability):

```powershell
$preset = 'stm32-release'
$curves = 'OFF'
$dir = "Firmware/build/a02-$preset-$curves"
cmake --preset $preset -S Firmware -B $dir "-DWTK_ENABLE_SUPPLEMENTARY_CURVES=$curves" -DWTK_FLASH_FORENSICS=ON
cmake --build $dir --parallel 6
python Firmware/tools/collect_flash_evidence.py $dir --profile "$preset-$curves" --git-sha (git rev-parse HEAD)
python Firmware/tools/firmware_size.py "$dir/WTK.RLCMeter.elf" --budget product
```

Use `--budget bringup` for BRINGUP. Source twins add
`'-DCMAKE_C_FLAGS_RELEASE=-g1 -DNDEBUG'` in another build directory; pass it to
the collector as `--source-build-dir`. Add `--baseline <attribution.json>` and
`--compact-out <report.json>` to capture comparisons and review snapshots.
Full raw disassembly, nm, map, ELF/BIN, LTRANS `.su`, build/test logs and command
listings remain in ignored `Firmware/build/a02-*` directories; no binary or build
tree is committed. Linux CI uses Ninja for host builds. For virtual checks, run
`python tools/run_virtual_tests.py --check-only` and `--lint-only` from `Firmware/`.
