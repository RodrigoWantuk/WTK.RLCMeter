# A01 — Linked Flash baseline and product completion budget

STATUS: COMPLETE — A01 evidence/tooling only. Plan 11 remains IN_PROGRESS.

Follow-up: [A02.1](../a02/README.md) implements the curve capability boundary and
provides the [current forward budget](../a02/feature-budget.csv). A01 figures below
remain historical evidence; do not subtract the curve saving again from A02's baseline.

Evidence date: 2026-10-09. Baseline: `main` at
`9553fa84780b9a91a25065c3e2f8d7d5305bda80` (merged PR #7).
The clean checkout was fetched, fast-forwarded from `8abc1b3`, and its recursive
submodules initialized before reading plans or analyzing firmware. Plan 16 and the
updated plan index are included. Work branch: `codex/a01-flash-forensics`.

**The current mandatory-product trajectory does not fit without recovery.**
Release has only 1,440 B physical Flash headroom. A temporary, isolated removal of
the optional supplementary-curve reader saves **2,516 B**, but that alone does not
cover the central completion estimate. Combining that measured opportunity with an
**unverified** 5,000 B gross OSL-workflow relocation and 6,408 B of remaining work
(including replacement provisioning and contingency) forecasts **62,988 B**.
This is a plausible path, not a release-fit guarantee. The high-cost scenario is
71,436 B and does not fit. Do not add new PRODUCT features until recovery lands.

No firmware source, calibration gate, math, persistent schema, safety behavior,
qualification flag, memory limit or default optimization was changed by A01.
The committed experiment patch is evidence for a copied source tree only.

## Actual baseline memory

All values are bytes, measured from fresh builds, not copied from historical plans.
The silicon capacities are 65,536 Flash and 20,480 SRAM.

| Output section / quantity | PRODUCT Debug | PRODUCT Release | BRINGUP |
| --- | ---: | ---: | ---: |
| `.isr_vector` | 236 | 236 | 236 |
| `.text` | 58,640 | 61,760 | 56,440 |
| `.rodata` | 2,032 | 2,044 | 8,324 |
| `.ARM` unwind index | 8 | 8 | 8 |
| initialized `.data` (Flash and RAM) | 48 | 48 | 44 |
| **Stored Flash / BIN length** | **60,964** | **64,096** | **65,052** |
| **Physical Flash remaining** | **4,572** | **1,440** | **484** |
| `.bss` | 12,496 | 12,940 | 12,496 |
| `.noinit` | 0 | 0 | 0 |
| stack/heap output section | 2,048 | 2,052 | 2,048 |
| **Accounted RAM** | **14,592** | **15,040** | **14,588** |
| **Physical RAM remaining** | **5,888** | **5,440** | **5,892** |
| old size script's RAM report | 16,640 | 17,092 | 16,636 |
| ELF file size (not programmed Flash) | 1,477,120 | 99,604 | 94,752 |

The stack floor is 2,048 B, the heap floor is zero, and Release includes 4 B
alignment in `._user_heap_stack`. All three Flash load spans equal their section
sums; no inter-section load gaps exist. Alignment *within* output sections is
already included. Debug sections, symbol tables and ELF file padding are not
programmed. Debug's non-allocated sections alone occupy 1,339,249 B.

### Existing RAM-report defect corrected

GNU Berkeley `size` reports Release BSS as 14,992 B: `.bss` 12,940 plus the
2,052 B NOLOAD stack section. `firmware_size.py` used to add the stack a second
time. It would also double-count nonempty `.noinit`. The correction uses the
explicit linker output sections, retains the raw aggregate as
`size_bss_aggregate_bytes`, and tests nonempty `.noinit` and alignment.
This recovers **zero physical RAM**; it corrects the report. The 17 KiB PRODUCT,
18 KiB BRINGUP and 20 KiB silicon limits remain unchanged. The independent
attribution tool agrees with the corrected totals. Baseline JSON preserves the
old reported total and its duplicate count rather than rewriting historical data.

The historical 63 KiB PRODUCT Flash gate is still 64,512 B (416 B Release margin).
Debug retains its existing unbudgeted post-build mode plus the physical linker
limit; an explicit PRODUCT-budget invocation also passes. No gate was loosened.

## Toolchain, flags and reproducibility

| Tool | Version |
| --- | --- |
| Arm compiler | Arm GNU Toolchain 14.2.Rel1, GCC 14.2.1 20241119 |
| GNU linker/binutils | 2.43.1.20241119 |
| CMake | 4.4.4 |
| Ninja | 1.13.2.git.kitware.jobserver-pipe-1 |
| Python | 3.9.5 |
| Host compiler | MSVC 19.44.35228.0, Visual Studio 17 2022 generator, x64 |
| Wokwi CLI (lint only) | 0.26.1 |

Common ARM flags: `-mcpu=cortex-m3 -mthumb -std=c17 -flto=auto
-fno-fat-lto-objects -ffunction-sections -fdata-sections -fstack-usage` plus the
repository warning set. PRODUCT appends `-Os -fno-unwind-tables
-fno-asynchronous-unwind-tables`. Debug starts with `-g3`; Release with
`-g0 -DNDEBUG`. BRINGUP is MinSizeRel, `-Os -DNDEBUG`, with LTO. Link uses the
unchanged 64 KiB linker script, `-nostartfiles --specs=nano.specs
--specs=nosys.specs -Wl,--gc-sections -Wl,--print-memory-usage -Wl,-Map=...`.
No fast-math or numerical substitute was tested or enabled.

String pooling is effective: `.rodata.str1.1` is merged/relaxed, and exact constant
symbol scanning finds no duplicate >=8 B tables in PRODUCT. This does not prove
the absence of partial/common data sequences. BRINGUP has one identical-symbol
candidate group; it is not PRODUCT savings. Nonzero veneers were not found.
CMSIS/HAL/LL integration is header-only in these targets; there are no separately
linked HAL driver objects to remove. Inlined LL operations remain necessary code
inside application/BSP symbols. Archive inclusion lists indicate why a member was
selected, not that its entire original object survived LTO and GC.

Commands below run from the repository root. Build-directory overrides preserve
supported preset settings and avoid existing local build caches. On Windows the
first Ninja host configure could not find a host compiler in PATH; selecting the
installed Visual Studio generator resolved it without installing a compiler.

```powershell
git status --short
git branch --show-current
git fetch origin --prune
git pull --ff-only origin main
git submodule update --init --recursive
git rev-parse HEAD
git switch -c codex/a01-flash-forensics

cmake --preset host-debug -S Firmware -B Firmware/build/a01-baseline/host-debug-vs -G "Visual Studio 17 2022"
cmake --build Firmware/build/a01-baseline/host-debug-vs --config Debug --parallel 6
ctest --test-dir Firmware/build/a01-baseline/host-debug-vs -C Debug --output-on-failure
cmake --preset host-release -S Firmware -B Firmware/build/a01-baseline/host-release-vs -G "Visual Studio 17 2022"
cmake --build Firmware/build/a01-baseline/host-release-vs --config Release --parallel 6
ctest --test-dir Firmware/build/a01-baseline/host-release-vs -C Release --output-on-failure
python -m unittest discover -s Firmware/tests/tools -v

foreach ($preset in @('stm32-debug','stm32-release','stm32-bringup')) {
  cmake --preset $preset -S Firmware -B "Firmware/build/a01-baseline/$preset"
  cmake --build "Firmware/build/a01-baseline/$preset" --parallel 6
}
```

The baseline was built before tool changes, at the exact SHA above. To reproduce
that baseline later, use a clean checkout of that SHA and invoke the new tools
from the A01 checkout against its artifacts. Firmware embeds its Git SHA;
subsequent commits therefore change the version string, even with unchanged code.
Current-commit builds must be labeled with their own SHA.

See [tool invocation and experiment instructions](tooling.md). Full ELF, BIN, MAP,
size JSON, nm, disassembly, command lines, configure/build/test logs, CSV, Markdown
and attribution JSON remain under ignored `Firmware/build/a01-baseline/` and
`Firmware/build/a01-candidate/`. Only compact snapshots, commands and the
experiment patch are versioned. BIN hashes are in each snapshot; ELF/MAP hashes
identify these local artifacts and are not promises of path-independent builds.
The new CI workflow retains same-toolchain base/candidate reports and deltas for
30 days. Its Ubuntu toolchain is independently versioned; do not compare its
absolute sizes to this Windows baseline as if the compilers were identical.

## Linked attribution and profile comparison

The parser reconciles ELF output sections against the map and partitions address
ranges before totaling objects/modules. Symbols and strings are alternate views,
never additional Flash. Exact symbol aliases are grouped; different overlapping
symbol extents are flagged and never summed into totals. Out-of-range relaxed
input contributions become `UNATTRIBUTED`. Random LTO and machine paths are
normalized. Incomplete/missing/corrupt required inputs fail explicitly.

Release/BRINGUP `-g0` images lack useful source lines. Diagnostic twins use `-g1`
and have **identical BIN bytes**, checked before matching each symbol's
address/size/name to its DWARF source. The collector refuses a different BIN.
Even with DWARF, function origins are not exclusive feature costs: LTO inlines
callees from other modules into `app_step`, `Reset_Handler`, etc. They are useful
investigation locations, not claimed removable totals.

| Largest Release emitted-function origins | Linked bytes |
| --- | ---: |
| `app_shell.c` | 18,397 |
| `ui_product.c` | 5,108 |
| `startup_stm32f103c8tx.c` (includes inlined main/init) | 4,494 |
| `measurement_engine.c` | 3,758 |
| `app_calibration_session.c` | 3,604 |
| `app_calibration_wizard.c` | 3,604 |
| `measurement_calibration.c` | 3,076 |
| `measurement_dsp.c` | 2,504 |
| `UNATTRIBUTED` in module view | 2,326 |
| `hw_aux_sensors.c` | 1,808 |

Largest actual functions: `app_step` 7,620; `Reset_Handler` 4,492;
`prepare_line` 3,692; `app_calibration_session_step` 3,532;
`app_calibration_wizard_step` 3,000; `product_auto_process_block` 2,304.
The latter includes inlined post-OSL code; no standalone curve symbol does **not**
mean that the curve feature costs zero.

Release object totals: LTO partitions 62,307 B, libgcc 1,632 B, newlib-nano
150 B, linker alignment 4 B, untrustworthy relaxed-map ownership 3 B.
Libgcc is single-precision ABI arithmetic/conversion/comparison. No linked
`printf`, `scanf`, floating formatting, double-precision ABI helpers or libm
payload was found. Newlib's five contributing members are strlen (24 B including
8 B unwind index), memcmp (32), memmove (50), memset (16), and memcpy (28).

Largest constants: emergency `g_glyphs` 344 B, a compiler switch table 200 B,
NTC table 168 B, measurement I/O table 128 B, sine table 90 B. The biggest
printable NUL-terminated candidate is the 28 B hardware identifier. Normal
localized texts, fonts and art already reside in W25Q.

Release minus Debug: **+3,132 B Flash, +448 B RAM**. This is not solely an
optimization comparison: Release includes resource recovery and rich-image code,
while Debug omits those by default and uses a different log level.
BRINGUP minus Release: **+956 B Flash, -452 B RAM**. Its console origin contributes
15,666 B and `.rodata` is 8,324 B; PRODUCT lacks console/campaign/DC symbols.
BRINGUP lacks product UI/application symbols, as the existing profile guard
confirms. A02 profile splitting is valuable for BRINGUP capacity but cannot be
claimed as PRODUCT recovery for code already absent there.

## Ten investigated opportunities

Ordered by the measured linked region under investigation, not source-file size.
Rows overlap through inlining/features and **must not be added**. Except the curve
experiment, savings are unverified engineering estimates in bytes. Gross means
code removed before replacement; net subtracts replacement support. Zero is a
valid outcome. Regression names refer to existing host modules plus required
future integration/bench tests.

| Rank / source and linked evidence | Reachability and action | Gross / net saving | Risks; required regression; decision |
| --- | --- | --- | --- |
| 1. `app_shell.c`: 18,397 origin bytes; `app_step` 7,620 | Superloop calls product, safety, storage, UI and recovery; adapters are function-pointer roots. Investigate shared wrappers/dispatch, retain ownership. | 0–1,024 / 0–768 estimate; not the whole shell | Safety teardown/quiet/ISR ownership, recovery UX. Product, session, flash-access, recovery tests and bench timing. No policy change authorized; no broad rewrite. |
| 2. Wizard/session/solver origins: 3,604 + 3,604 + 692 = 7,900 | Boot/manual wizard calls session, workflow and solver (`app_product.c:1279`, wizard `:236/:625`). Relocate full campaign/solve to PC; retain device capture, validation, A/B. | 3,000/5,000/7,000 gross scenario estimates; replacement 1,000/1,800/3,000; central net 3,200, not measured | Highest calibration/provisioning/UX risk; boot gate, 33 keys, OSL golden vectors, cancellation, power cuts, old-slot retention, PC readback. A06 migration sign-off before behavior change. |
| 3. `ui_product.c` 5,108 plus app controller 1,784; `prepare_line` 3,692 | Every render selects a line/page; menu and wizard branches remain reachable. Simplify common formatting and optional service presentation. | 256–1,024 / 128–768 estimate | Never replace errors with numbers or lose bilingual blank-W25Q recovery. UI text/font/image/format, product navigation/stale/cancel tests; review UX. Wizard portions overlap #2. |
| 4. `Reset_Handler` 4,492 | Calls main, which LTO inlines with initialization. Retain safe initialization; inspect codegen rather than deleting apparent startup bulk. | 0 / 0 recommended; no profitable build flag found | Boot/reset/watchdog/safe GPIO regression and bench. No startup or memory-initialization change approved. |
| 5. `measurement_engine.c` 3,758 | Automatic session submits bounded attempts, scores and classifies. Investigate repeated predicates only. | 0–384 / 0–256 estimate | High measurement/qualification risk. Autorange six-attempt bounds, OPEN/SHORT, clipping, valid/invalid RLC, confidence tests; metrology review. |
| 6. `measurement_calibration.c` 3,076; candidate commit 1,256 elsewhere | Measurement lookup/apply and store validation/serialization remain reachable. Split ownership only if a real dead path emerges; retain runtime and persistence. | 0–384 / 0–256 estimate, overlaps #2 | Schema/model/CRC/finite/key and transactional compatibility. Golden frame, missing/corrupt/incomplete/old-slot tests. Format or boot-gate change needs approval. |
| 7. Supplementary curves: exact Release delta **2,516**; inlined in process/boot paths | Boot loads qualified overlay; post-OSL callback reads/applies it. Compile-time isolate pending qualification, preserving OSL and explicit feature status. | **2,516 gross/net measured for temporary removal**; deployable net estimate 2,300–2,516 after policy glue | Qualification/overlay compatibility risk, not an OSL replacement. Curve pure tests stay; add PRODUCT-disabled and BRINGUP-enabled tests, corrupt/absent overlay and OSL-only parity. A02 reviewed flag required. |
| 8. DSP 2,504 + libgcc 1,632 | Core phasors/complex operations; compiler ABI helpers required. Retain math. Newlib only 150 B, formatting symbols absent. | 0 / 0 recommended | No numerical shortcuts. DSP/OSL float vectors, degeneracy/NaN/near-open tests and bench accuracy. Different approximations need metrology approval. |
| 9. Recovery protocol/update origins 304 + 420; shell step 494, RX 200, decode 272, begin 216, process 120, status 96 = 2,122 selected disjoint symbol/origin bytes | Release recovery entry drives UART frame receiver and shared workspace. Retain minimum receiver; investigate small duplication. | 0–256 / 0–128 estimate | Losing it strands blank/corrupt W25Q. Frame/order/CRC/abort, page boundary, power cut, flash access and recovery UI tests. Moving recovery to separate firmware requires owner approval. |
| 10. `.rodata` 2,044, including emergency font 344 and switch table 200 | Fallback must work without external resources; normal assets already external. Retain safety/font/NTC/sine; investigate redundant technical labels. | 0–128 / 0–96 estimate | Readability and malformed/nonfinite values; bilingual fallback/font and table tests plus physical display. No evidence for kilobytes of strings or unused HAL. |

## Controlled experiments

Every experiment uses its own build directory. Defaults are unchanged. Removing
or ignoring the temporary directory is the rollback; never flash an experiment as
a qualified product. Full baseline host regressions do not qualify changed
experimental code generation or removed functionality.

| Hypothesis / changed variable | Flash before → after | RAM after | Result |
| --- | ---: | ---: | --- |
| `-g1` source-attribution twins | 64,096 → 64,096; BRINGUP 65,052 → 65,052 | 15,040 / 14,588 | BIN equality; diagnostic metadata only |
| `WTK_FLASH_FORENSICS=ON` (cref/GC log/LTO stack artifacts) | all three unchanged | all unchanged | Builds/size/profile checks pass; BIN equality |
| `-Os` → `-Oz` | 64,096 → 64,096 | 15,040 | No recovery; build gates pass; no new semantic qualification |
| LTO → OFF | 64,096 → **70,768 requested by linker** | linker reports 15,048 | Link fails by 5,232 B; no valid ELF/BIN; keep LTO |
| `-fno-inline-functions-called-once` | 64,096 → 65,196 | 15,048 | Physical link succeeds, existing 64,512 B PRODUCT gate fails; reject |
| Optional curve runtime off in copied source | 64,096 → **61,580** | 14,976 | **2,516 B recovery**; build/size/profile pass; not regression-qualified |

The curves-off source change is exactly [curves-off.patch](experiments/curves-off.patch).
It removes overlay boot loading and post-OSL dispatch; it does not touch the
OSL boot gate, equations, schema, capture transaction, or persistent storage code.
It is a cost probe, not a finished feature flag or compatible migration.
See [experiment summary](experiments/summary.json) and
[curve snapshot](experiments/curves-off.json).

## Forward product forecast

[feature-budget.csv](feature-budget.csv) is the editable source table. Costs are
incremental internal Flash, not whole-module footprints. Existing standalone AC,
calibrated math, safety, OSL reader, A/B storage, resource receiver and basic UI
are already in the baseline. Their rows reserve completion/fix cost, not duplicate
their existing size. All future increments below are estimates, **not prototype
measurements**. Integration overhead is also estimated and must be replaced by
link deltas as tickets land.

| Remaining MUST_HAVE_PRODUCT work | Low | Central | High |
| --- | ---: | ---: | ---: |
| Standalone AC result/display finishing, including valid Q/D | 128 | 512 | 1,024 |
| Safety/fault recovery fixes discovered in integration/bench | 128 | 256 | 768 |
| Calibrated impedance/RLC finishing | 0 | 128 | 384 |
| Provisioned OSL reading/provenance integration | 0 | 128 | 256 |
| Device-side PC/factory provisioning, capture/receive/verify/commit | 1,000 | 1,800 | 3,000 |
| W25Q resource/calibration recovery integration | 64 | 256 | 768 |
| Usable UI, stale results, cancel/state completion | 128 | 512 | 1,024 |
| Per-result error qualification status and bounded-table reader | 256 | 768 | 1,536 |
| Integration overhead | 256 | 512 | 1,024 |
| Essential maintenance/change contingency | 1,024 | 1,536 | 3,072 |
| **Additional mandatory envelope** | **2,984** | **6,408** | **12,856** |

| Scenario | Optimistic total | Central total | Conservative total |
| --- | ---: | ---: | ---: |
| Keep current firmware + mandatory envelope | 67,080 | 70,504 | 76,952 |
| Isolate curves (measured -2,516) + mandatory envelope | 64,564 | 67,988 | 74,436 |
| Curves isolated + OSL relocation gross 7,000/5,000/3,000 + mandatory envelope | **57,564** | **62,988** | **71,436** |
| Last scenario physical margin | 7,972 | **2,548** | **-5,900** |

The OSL gross and replacement ranges are scenario assumptions, not independent
additive opportunities. The central case already includes 1,800 B for the new
device-side service path, 512 B integration overhead and 1,536 B maintenance
contingency. In a completed A06 build, replace gross removal and replacement
estimates with one measured **net** delta, then remove its provisioning row to
avoid double-counting. Keeping the existing wizard alongside its PC replacement
has no demonstrated fit path.

`OPTIONAL_PRODUCT`: local Open/Short trim (512/1,024/2,048 B), Live/graphs
(768/1,536/3,072 B), qualified overlay installation (1,024/2,048/3,584 B beyond
the present reader). If curves have been isolated, reinstating their reader also
requires a new measured budget; 2,516 B is only its current opportunity cost.
These are excluded from mandatory totals. Do not reintroduce them until a complete
mandatory build and maintenance allowance fit both physical and approved gates.

`PC_ONLY`: fitting, OSL campaign history, uncertainty solvers, scope records and
plots. `BRINGUP_ONLY`: lab console, DC/transient research and extensive diagnostics,
each subject to its own image limits. `EXTERNAL_W25Q_DATA`: normal fonts, artwork,
catalogs, coefficients and qualified bound tables. These data/desktop rows cost
zero **additional PRODUCT program Flash**; necessary embedded readers/protocols
are already included above, not assumed free.

Conclusion: fitting the complete mandatory product is credible **only
conditionally** on recovery and a bounded service design. It is not currently
demonstrated. Demote curves, rich extras/Live, full local OSL fitting and lab
workflows before touching safety, valid-result semantics or recovery. If A06
cannot achieve its net budget, narrow optional UX and reforecast before adding
features; do not expand the MCU assumption or silently raise project gates.

## Validation and outstanding evidence

- Fresh baseline host Debug: **40/40 CTest passed**.
- Fresh baseline host Release: **40/40 CTest passed**.
- Existing Python baseline: **123 passed**; final suite: **149 passed**, including parser,
  deterministic JSON/CSV, malformed/missing data, aliases, LTO/relaxation,
  source-twin identity and RAM-accounting regressions.
- Baseline and final instrumentation ARM Debug/Release/BRINGUP: **all passed**.
  Existing size and profile-symbol checks executed; all three candidate BINs
  are byte-identical to the baseline while HEAD remains the baseline SHA.
- Full raw collector reports reconcile Flash and RAM with sections; diagnostic
  twins are byte-identical. JSON/CSV fixed fixtures are deterministic.
- Wokwi local `--check-only` and `--lint-only`: **passed**. Scenario simulation
  **not run**: `WOKWI_CLI_TOKEN` unavailable. Lint is not simulation evidence.
- Hosted CI is **environmentally blocked**: [Flash forensics run 37944009642](https://github.com/RodrigoWantuk/WTK.RLCMeter/actions/runs/37944009642)
  was refused before any job steps ran. GitHub's annotation says: "The job was not
  started because your account is locked due to a billing issue." The existing
  Wokwi workflow was also refused before execution. No hosted CI pass is claimed.
  The unrelated pre-existing virtual-hardware workflow still
  references the obsolete `stm32-lab` preset; this pre-existing issue is not
  silently fixed in A01.

With LTO, compile-only `-fstack-usage` did not preserve usable `.su` files. The
opt-in forensics link adds `-fstack-usage -save-temps=obj`, producing LTRANS `.su`
files without changing BIN bytes. Largest Release individual frames include
calibration session step 888 B, wizard step 704 B, app step 448 B, and process
block 440 B. These are not a call-chain/ISR stack bound; the reserved 2,048 B floor
is not proof of peak stack adequacy. The remaining SRAM is also headroom, not
permission to consume it without a nested-call audit and bench watermark.

`REQUIRES_BENCH_VALIDATION`: safe boot/reset/K1/range timing and charger/residual
permission; excitation/current/headroom and ADC/DMA timing; accuracy, phase/SNR,
leakage, 33-condition validity and all numeric error bounds; display/SPI quiet
behavior; physical W25Q power-cut recovery; worst-case stack/ISR behavior. The
Rev.1 board is unassembled. No electrical or metrology qualification is claimed.

## Next narrowly scoped ticket

**A02a: compile-time isolate the supplementary curve reader in PRODUCT**,
retaining the host tests and a deliberate service/BRINGUP capability. Target the
measured 2,516 B opportunity, remeasure any flag/status glue, preserve OSL-only
result parity and error/qualification distinctions, and prove resource/OSL
recovery and all three profile gates. This can recover substantial measured
space without redesigning boot provisioning. It is not enough alone for the
central full-product forecast.

Then A06 must design and review PC initial OSL provisioning plus optional local
fixture trim before replacing the existing wizard or gate. Its first acceptance
test is that a blank device can become validly calibrated, survive interrupted
provisioning, and measure standalone with no PC attached.
