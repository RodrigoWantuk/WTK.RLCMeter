# Flash attribution tooling

Python 3.9+ and GNU Arm binutils are sufficient; no firmware runtime dependency
or online service is introduced. Run from the repository root.

## Capture an existing supported build

```powershell
$sha = git rev-parse HEAD
python Firmware/tools/collect_flash_evidence.py Firmware/build/stm32-release --profile stm32-release --git-sha $sha
```

This captures raw `size -A`, `nm --print-size --size-sort --line-numbers`,
`objdump -h`, `objdump -d`, Ninja command lines and JSON/CSV/Markdown beneath
`<build>/forensics/`. It checks against existing size JSON. Old metadata with the
known NOLOAD double-count is preserved and explicitly labeled. Optional
`--compact-out <path>` writes a compact versionable summary; keep full raw
artifacts and binaries outside Git.

For cross references, discarded sections and LTO stack frames:

```powershell
cmake --preset stm32-release -S Firmware -B Firmware/build/a01-diagnostic -DWTK_FLASH_FORENSICS=ON '-DCMAKE_C_FLAGS_RELEASE=-g1 -DNDEBUG'
cmake --build Firmware/build/a01-diagnostic --parallel 6
python Firmware/tools/collect_flash_evidence.py Firmware/build/stm32-release --profile stm32-release --git-sha $sha --source-build-dir Firmware/build/a01-diagnostic
```

The same preset and source SHA must be used. The collector compares the entire
BIN first and refuses source-line enrichment if any programmed byte differs.
BRINGUP's twin uses `stm32-bringup` and
`'-DCMAKE_C_FLAGS_MINSIZEREL=-Os -g1 -DNDEBUG'` instead. PRODUCT Debug already has
source lines. LTO temporary paths and local roots are removed from reported owners;
raw files retain exact tool output for investigation. Source-line attribution
identifies the origin of emitted functions and includes their inlined callees.

## Analyze captured text offline and compare profiles/commits

```powershell
python Firmware/tools/flash_attribution.py --map Firmware/build/stm32-release/WTK.RLCMeter.map --nm Firmware/build/stm32-release/forensics/nm.txt --sections Firmware/build/stm32-release/forensics/sections.txt --bin Firmware/build/stm32-release/WTK.RLCMeter.bin --profile stm32-release --git-sha $sha --json-out Firmware/build/analysis/release.json --csv-out Firmware/build/analysis/release.csv --markdown-out Firmware/build/analysis/release.md
python Firmware/tools/collect_flash_evidence.py Firmware/build/stm32-bringup --profile stm32-bringup --git-sha $sha --baseline Firmware/build/stm32-release/forensics/attribution.json
```

Use `nm-source.txt` instead of `nm.txt` for previously verified twin enrichment.
`--baseline` works on either CLI. Deltas are **candidate minus baseline**: negative
means recovery. JSON includes total Flash, load span/gaps, remaining physical
capacity, sections, disjoint object/module contributions, library families,
archive selection lists, symbol aliases/overlap flags, printable string candidates,
identical constant candidates and parser warnings. CSV contains alternate
section/object/module/runtime/symbol/string views plus object/module deltas.
Do not sum alternate views or symbol sizes. Markdown ranks the most useful entries.

Unsupported/malformed required data returns exit code 2; Flash input/output
section disagreement and out-of-silicon/overlapping loads are rejected.
Missing input ownership becomes `UNATTRIBUTED`, not a guessed module name.
Printable string scanning is restricted to `.rodata`, ASCII runs >=4 bytes plus
NUL; UTF-8 strings, suffix pooling and pointer/data coincidences mean this is a
candidate ranking, not a complete literal inventory. Duplicate-table scanning
compares >=8-byte constant symbols; it excludes overlapping aliases.

## Reproduce the isolated curve cost probe

Use the baseline checkout from the report. The following preparation copies only
firmware source/build-support files to a new ignored directory. It does not edit
the working firmware. The patch removes the optional overlay call sites only.
Save logs under the experiment directory. Do not program this cost probe onto a
device as a qualified image.

```powershell
$taskSource = Join-Path (Resolve-Path Firmware/build).Path 'a01-curves-source'
if (Test-Path -LiteralPath $taskSource) { throw 'Choose a fresh experiment directory' }
New-Item -ItemType Directory -Path $taskSource | Out-Null
foreach ($name in @('src','config','cmake','tools','CMakeLists.txt','CMakePresets.json')) {
  Copy-Item -LiteralPath "Firmware/$name" -Destination $taskSource -Recurse
}
git apply --check --unidiff-zero --ignore-space-change --directory=Firmware/build/a01-curves-source docs/review/a01/experiments/curves-off.patch
git apply --unidiff-zero --ignore-space-change --directory=Firmware/build/a01-curves-source docs/review/a01/experiments/curves-off.patch
$taskVendor = (Resolve-Path Firmware/third_party/st).Path
cmake --preset stm32-release -S $taskSource -B Firmware/build/a01-curves-off "-DWTK_STM32_CMSIS_CORE_ROOT=$taskVendor/cmsis_core" "-DWTK_STM32_CMSIS_DEVICE_F1_ROOT=$taskVendor/cmsis_device_f1" "-DWTK_STM32F1_HAL_DRIVER_ROOT=$taskVendor/stm32f1xx_hal_driver"
cmake --build Firmware/build/a01-curves-off --parallel 6
python Firmware/tools/collect_flash_evidence.py Firmware/build/a01-curves-off --profile curves-off --git-sha "${sha}+curves-off.patch" --baseline Firmware/build/stm32-release/forensics/attribution.json
```

Rollback: return to the ordinary preset/build directory. No working firmware
source was modified and no default feature was changed. Keep the patch and its
SHA association with the report if retaining the experimental artifacts.

Other experiments use fresh `-B` directories and exactly one codegen override:

- Oz: `-DWTK_PRODUCT_OPTIMIZATION_LEVEL=-Oz`.
- No LTO: `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF` (expected physical link failure).
- No inline-once: `'-DCMAKE_C_FLAGS=-mcpu=cortex-m3 -mthumb -fno-inline-functions-called-once'`
  (expected PRODUCT size-gate failure).

Diagnostics may add `-DWTK_FLASH_FORENSICS=ON`; byte equality was independently
established for this instrumentation. Do not infer semantic qualification from
codegen build success. Never enlarge linker memory or change gates for a probe.

## Tests

```powershell
python -m unittest discover -s Firmware/tests/tools -v
python Firmware/tools/run_virtual_tests.py --check-only
python Firmware/tools/run_virtual_tests.py --lint-only
```

The fixture tests cover GNU archive paths with spaces, wrapped input sections,
discarded sections, LTO paths, aliases, overlapping ownership, merged strings,
initialized-data LMAs, NOLOAD RAM, load gaps, malformed/missing input, output
identity and diagnostic-twin refusal. The CI workflow builds the base and
candidate using the same installed compiler and retains per-commit JSON plus
deltas. Source ownership stays unknown in stripped CI images unless a verified
diagnostic twin is supplied; object/section totals still reconcile.
