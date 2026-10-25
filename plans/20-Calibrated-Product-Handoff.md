# 20 — Calibrated PRODUCT handoff without another resistor campaign

STATUS: IN_PROGRESS — PHYSICAL_PRODUCT_SCHEDULING_REGRESSION

The owner requests the next step after persistent sequence 1, without another
resistor round, plus the actual PRODUCT/C/L/autorange status. Reuse the verified
33-condition calibration and existing resource pack. Do not assert qualification,
enable Live/DC/polarized excitation, or change acquisition/DSP/calibration math.

The tested 46df1fbb PC-provisioned PRODUCT image was programmed and read back
exactly. A controlled boot loads all 33 records, active sequence 1, resources OK,
display fault 0, safety faults 0, K1 SAFE, range/excitation OFF and session count
0. Presentation is SAFETY_BLOCKED because the shell forwards the actual disabled
range into a gate that requires READY before it can schedule range preparation.
The factory outer gate also mistakes expected residual invalidation during ADC
ownership/requalification for a reason to abort that same hardware transaction.
An additional read-only snapshot confirmed raw blockers 0x0C, valid VMID,
residual UNKNOWN/count 0, and residual age 79 ms against the unchanged 50-ms
limit. Slow TFT work interleaves the cooperative auxiliary ADC group. Defer
normal TFT steps while an auxiliary group is active or residual requalification
is pending. Fresh unsafe/invalid states still allow warning rendering. This
changes scheduling only, not ADC channels, sample timing or safety thresholds.

Separate application scheduling from physical measurement permission. The shell
reports the actual disabled-range state and the existing residual-refresh hint.
Only these expected blockers are tolerated for scheduling; actual unsafe
residual, charger, supply and fault flags still block. While a measurement session
owns its transaction, its hardware FSM handles range/ADC transitions and fresh
single-use permits. UI never controls GPIO or creates a relay permission. Wait
cooperatively at PERMIT_ISSUE for actual fresh auxiliary evidence, for at most
1,000 ms from that attempt's start. Expiry falls through to the unchanged hardware
permit check and SAFE abort; no samples, thresholds or permit TTL are changed.

Expected files: app_product scheduling helper/inputs/controller; app_shell
adapter; actual safety-evaluator/ADC-state-machine host regressions; this plan,
status documentation and a source-bound delivery. Keep BSP/hardware/DSP and
stored records unchanged. Test standard and PC-provisioned PRODUCT host profiles,
build all nine strict UID-bound embedded variants and check linker budgets.

Program only a verified internal-Flash image, preserving W25Q calibration and
resources. Observe PRODUCT boot, sequence/count, READY and SAFE outputs without
issuing a measurement. Then ask the owner to connect the display with power off,
leave terminals empty, and confirm splash and the ready/menu UI. No resistor or
C/L measurement is required for this handoff. C/L accuracy, small inductors,
polarized capacitors, final test leads and broader safety qualification remain
REQUIRES_BENCH_VALIDATION. Document current autorange limits (Click, six attempts,
1 kHz initial and at most one second-frequency check, no automatic 10-Hz search).
