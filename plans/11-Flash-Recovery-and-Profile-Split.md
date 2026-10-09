# 11 — Flash Recovery, Target Boundaries and Code Audit

STATUS: NOT_STARTED — INDEPENDENT AGENT WORK MAY BEGIN
PRIORITY: P0. Do not grow the PRODUCT image while this phase is incomplete.
BASELINE: 2026-10-09 main 8abc1b3; documented PRODUCT 64,096 B / BRINGUP 65,052 B.

## Objective and non-negotiables
Recover measured Flash margin without regression of the fail-safe state, acquisition timing, OSL integrity, 33 condition keys, W25Q recovery, click measurement, or bilingual essential results. Desired PRODUCT <=57,344 B (56 KiB). Interim PRODUCT build gate <=61,440 B (60 KiB), with 65,536 B absolute silicon limit. These are targets, not new build assertions. 16 KiB preferred accounted PRODUCT RAM; keep hard/physical RAM constraints. Do not tune thresholds merely to conceal regressions.

## A. Collect a reproducible size baseline, before editing
F11-01. Fresh checkout, git submodule update --init --recursive, record toolchain version and exact SHA. Build stm32-debug, stm32-release, stm32-bringup, host-debug/release. Save ELF/MAP/size JSON, nm --size-sort, objdump -h/-d, gcc .su, compiler/linker command lines to out-of-tree or ignored artifact directory.
F11-02. Generate CSV/Markdown with linked functions, source/object attribution, .text/.rodata/.data size per symbol, biggest static strings, duplicated constant tables, redundant veneers, libs (newlib, libgcc, HAL) and archive-member inclusions. Distinguish discarded unused sections from linked but reachable low-value features. A large .c file on disk is NOT proof of large linked Flash.
F11-03. Enable opt-in --print-gc-sections, -Wl,--cref and preferably a deterministic map parser/diff (no new obligatory external proprietary tool). Compare all profiles, LTO on/off, -Os, supported -Oz only if current GCC supports it; record exact bytes and test results. Verify string pooling and ffunction-sections/fdata-sections in BuildOptions. Do not change default compiler/toolchain without comparing semantics.
F11-04. Add Firmware/tools/flash_attribution.py or similar, producing machine-readable per-section/per-object profile diff and a human-readable ranked opportunity table. CI retains per-commit baseline + regression delta. Never use source-line count as bytes saved.

## B. Ownership hypotheses to test using actual map/nm (NOT confirmed dead code)
| Candidate | Evidence / why inspect | Safe disposition only after call-graph + tests |
| --- | --- | --- |
| app_shell.c (55 KB source) | PRODUCT and BRINGUP orchestration mixed in large TU, substantial resource-PC-link and diagnostics wrappers | extract a thin production shell, move lab-only commands behind exclusive source sets, deduplicate adapter callbacks without changing safety ownership |
| ui_product.c (61 KB source) | many menu/diagnostic/format paths and repeated per-line building | simplify repeated render dispatch, move strings/optional help to W25Q resource catalogue, externalize service-only screens; retain mandatory recovery and bilingual results |
| app_product.c (53 KB source) | UI/state orchestration and measurement/wizard adapters | remove duplicate state projections and redundant formatting; test interaction contract |
| measurement_calibration.c (50 KB source) | OSL runtime, record serialization and diagnostics interwoven | split lean lookup/apply/CRC, product calibration solve/write and host-only inspection; preserve A/B and schema v2/model v4 compatibility |
| app_calibration_wizard.c, app_calibration_workflow.c | comprehensive PRODUCT 33-condition procedure | reduce state storage/formatting, investigate common validation helpers; moving all OSL to PC requires owner approval and must not block PRODUCT startup without replacement |
| measurement_engine.c (46 KB source) | multi-attempt policy, classification, scoring, return choice | profile linked footprint, use tables/shared predicates where smaller, preserve explicit quality/qualification and <=6 attempt bounds |
| app_resource_update.c + app_pc_link_protocol.c | can look like optional communication | PRODUCT still needs blank/corrupt W25Q recovery and Resource Pack upload, so RETAIN minimal crash-safe transfer; move any verbose service protocol to BRINGUP/PC |
| measurement_cal_curve*, curve_store*, product_load_curve_store | reader is included before qualification/transfer path exists | measure cost of optional FEATURE_SUPPLEMENTARY_CURVES=OFF in normal PRODUCT until physical qualification; prohibit silent acceptance of qualified flags |
| measurement_dc.c / app_calibration_campaign.c / app_bringup_console.c | experiment and console | verify absent from PRODUCT symbol table; split BRINGUP feature families |
| float, approximations, helpers, diagnostic strings | Cortex-M3 soft-float cost and shared math | compare exact size and numerical residuals across C targets; do not replace complex math by inaccurate shortcut to save bytes |
| UI font/image/source JSON | already external W25Q pack, not MCU Flash | verify only fallback/recovery glyphs in MCU; never add boot art, catalogues, curve plot assets to internal Flash |

## C. Profile architecture
F11-05. Replace 'all common TUs for all targets' with explicit narrow source-set capabilities. Proposed PRODUCT_MIN (must remain normal release), BRINGUP_IO (pins, W25Q, TFT, safety), BRINGUP_ANALOG (PWM, ADC/DMA, RAW, oscilloscope diagnostics), BRINGUP_CAL (OSL calibration evidence, slot inspection/export), BRINGUP_DC (1 MOhm pilot only). Compile only one bringup profile per image; keep source ownership and tests coherent. Names/presets may be adjusted after reading the actual build.
F11-06. BRINGUP binaries each remain within 65,536 B physical and reserved RAM. Their lack of a full PRODUCT UI is expected; all images must retain safe boot, charger interlock, residual safety, watchdog, abort and safe teardown whenever K1 could be driven. No 'debug bypass safety' profile.
F11-07. PC replaces large calibration-campaign history, solver experimentation, interval optimization, graphs, scope records, serial diagnostics parsing and candidate-file construction. It cannot replace deterministic low-level capture, safety, local protocol readback, nor normal standalone product usage.
F11-08. Mark compile-time feature flags in generated config; write automated nm/profile tests for forbidden symbols (e.g. BRINGUP raw commands in PRODUCT, DC pilot in PRODUCT). CI proves no unreachable unsafe path is accidentally activated and excludes transient debug strings.
F11-09. Retain transactional W25Q A/B and the resource recovery receiver as narrow PRODUCT modules. Preserve shared 3,072 B workspace ownership; do not allocate a second ADC frame or full TFT framebuffer.
F11-10. Consider flash-only 'asset updater' as separate provisioning firmware only if factory has reliable SWD and owner agrees that blank W25Q recovery need not work on the ordinary device. Default position: keep production UART recovery until owner explicitly changes contract. An SPI Flash is not execute-in-place code memory on this STM32.

## D. Incremental optimization experiments — one isolated PR per experiment
F11-11. Function/data section GC, build flags and LTO: establish effectiveness before guessing savings. Keep optimized Debug symbols; test exception unwind requirements.
F11-12. Consolidate identical parsers, string formatters, status-name switch tables, duplicated hex/CRC helpers only when linkage/code-size reports show savings. Avoid merging unrelated serializers that risk wire-format drift.
F11-13. Re-evaluate printf/format dependencies; prefer constrained integer renderers and resource texts where actual map shows libc pull-in. Do not break non-finite/error labels.
F11-14. Split the PRODUCT curve reader under a disabled-by-default flag pending calibration qualification; measure positive and negative effects on complete image. If qualified user data already exists, plan version compatibility and migration rather than silently deleting behavior.
F11-15. Prioritize cold recovery screens and advanced menu help for data-driven rendering, but verify external W25Q failure path can still display minimum recovery information.
F11-16. Move long computations that produce device-independent lookup tables to host, serialize bounded coefficients to W25Q. Keep per-attempt interpolation, CRC, numeric finite validation, condition matching and safety decision on device.
F11-17. Reduce ADC/math structure duplication by ownership/aliasing only if safe after ISR/DMA lifecycle audit and sanitizer tests. Maintain 2,048 B documented stack reserve and .su stack audits.
F11-18. Re-audit compiled HAL/LL modules for unused source files and duplicate custom peripheral helpers; module removal needs hardware regression proof.

## E. Testing and acceptance
1. Automated size matrix for PRODUCT Debug/Release and each new bringup image, reproducible without local absolute paths; JSON history and rankings included in PR.
2. Must demonstrate >=2,656 B recovery to meet proposed 60 KiB interim gate, and >=6,752 B recovery to hit 56 KiB long-term target, relative to 64,096 B historical baseline. The exact build result, not the historical figure, governs regression accounting.
3. 40 CTest historical tests are not the finish line: full current host Debug+Release CTest, Python tests, Wokwi lint/scenarios when credentials available, ARM builds, nm symbol contracts, record compatibility golden vectors and power-cut tests.
4. Verify UI start/menus/measurement/error/backlight, empty/corrupt W25Q recovery, OSL commit/cancel/brownout, safe GPIO reset, K1/range transition, charger inhibit, DMA timeouts and aborts. Real electrical coverage remains REQUIRES_BENCH_VALIDATION.
5. For each patch report: prior/new linked Flash and accounted RAM for each profile, bytes saved or spent, map top contributors, number of behavioral changes, tests, confidence, rollback plan.
6. Stop the phase if any optimized math changes numerical output beyond validated tolerance, if ERROR becomes plausible numeric, if emergency SAFE latency increases without bench check, or if unqualified curve becomes installable.

## Suggested parallel owners
A: build/size instrumentation (F11-01..04), earliest task.
B: audited product-vs-bringup source split (F11-05..09), after report baseline.
C: menu/format and storage footprint PRs (F11-11..18), after attribution table.
D: cross-platform regression owner; independently audits safety and persistent compatibility.

## Completion criteria
Measured real link savings, safe product functioning, tracked explicit residual Flash debt, no drift in measurement model or qualifying claims. Mark COMPLETE only when exact target size gate and regression evidence are in CI. Otherwise report IN_PROGRESS with net bytes recovered and detailed remaining candidates.
