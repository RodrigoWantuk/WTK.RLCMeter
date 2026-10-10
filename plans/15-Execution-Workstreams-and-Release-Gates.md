# 15 — Executable Work Program, Agent Assignments and Release Gates

STATUS: IN_PROGRESS — ORCHESTRATION READY; PHYSICAL CAPABILITY/ERROR TARGETS UNAPPROVED
DEPENDS: plans/10-14. Not a license to modify safety or publish unqualified numbers.

## Master product objective
Deliver a compact, standalone, safe Rev.1 AC RLC instrument, with click measurement, justified R/X/|Z|/phase and conditional C/L/ESR/Q/D, explicit uncertainty/qualification per quantity, serviceable bilingual UI, PC-provisioned persistent OSL and W25Q recovery. In-device Open/Short fixture recalibration is optional/preferred if small enough; full 33-key OSL UI is not mandatory. Advanced metrology experiments, calibration curve fit, DC and scope work are PC/BRINGUP capabilities until bench qualified. Avoid an otherwise feature-rich device that cannot be installed, calibrated or trusted.

## Workstreams and ownership
| ID | Workstream | Dependency | Deliverable | Blocking? |
| --- | --- | --- | --- | --- |
| W0 | code-map/profile baseline | none | symbol/object-linked size evidence; reproducible builds | YES: no new PRODUCT payload |
| W1 | on-board electrical audit | none | schematic/PCB/BOM/as-built pad-level mapping, supply/ground and limits | YES for electrical changes |
| W2 | source/profile split and low-risk Flash cleanup | W0 | separately linked target code, before/after map and test evidence | YES for new MCU features |
| W3 | acquisition/analog bench suite | W1 | captured waveforms, channel skew and noise, exact qualified condition map | YES for accuracy claims |
| W4 | uncertainty and curve math on PC | can parallel W0/W1 | synthetic interval tests, parsimonious model comparison, fit/holdout reports | NO for baseline OSL instrument |
| W5 | minimal affordable reference kit | owner inventory + W1 | kit/fixture IDs with printed and measured tolerance, source records | YES for physical qualification |
| W6 | PC calibration workbench | W4 for solver, can mock earlier | live/shadow connection, curve charts, scope import and evidence database | NO for baseline standalone instrument |
| W7 | guarded DC only in bringup | W1 + W3 | safe 1M pilot hardware measurements and current/error budget | NO for AC MVP; YES for DCR/LEAK publication |
| W8 | qualified overlay protocol and UI bounds | W2+W3+W4+W6+sign-off | W25Q A/B transaction, OSL binding, verified error metadata | YES for curve deployment |
| W9 | product release qualification | W0-W3+W5 | release matrix and stable image | YES for product qualification |

## Sequence and executable agent tickets

### Sprint A — immediate, no board needed

A01 **STATUS: COMPLETE (evidence/tooling)** — see the
[2026-10-09 report](../docs/review/a01/README.md), based on synchronized main
`9553fa84780b9a91a25065c3e2f8d7d5305bda80`. Fresh Debug/Release/BRINGUP Flash:
60,964 / 64,096 / 65,052 B. Corrected RAM: 14,592 / 15,040 / 14,588 B.
The existing size script double-counted reserved stack; limits are unchanged.
The isolated curve experiment saves 2,516 B, not yet a production change.
Both 40-test host configurations pass; Python/parser, ARM, size/profile and Wokwi
lint evidence is in the report. Simulation/bench and hosted CI are separate gates.
Recommend A02 curve capability isolation before A06's reviewed boot/provisioning
migration. W0 evidence exists; W2 recovery and W9 product qualification remain open.

**A01 Size forensics (agent: embedded build)**. Read 11. Generate exact reproducible Flash/map/top-symbol reports on the latest SHA, with command/version capture and 3 profile comparisons. Add test checking report parser and size budgets. Avoid speculative refactors. Done: report ranked by *linked bytes*, 10 most promising opportunities, conservative save estimates and explicit not-yet-measured status.
**A02 Compile-time ownership (agent: embedded architecture)**. Read A01 report and plan 11. Propose minimal PRODUCT/BRINGUP_IO/BRINGUP_ANALOG/BRINGUP_CAL/BRINGUP_DC target source sets. Implement narrow option switch only after proving no safety or W25Q recovery regression. Keep all mandatory headers/contracts; automate forbidden symbols and test each image. Done: valid MCU builds, positive measured linked-byte savings, and a forward product capacity forecast for remaining MUST features; no arbitrary 56/60KiB pass/fail.

**A02.1 STATUS: COMPLETE — IMPLEMENTED_TESTED_HOST / COMPILES_TARGET.**
See [measured capability boundary and completion forecast](../docs/review/a02/README.md).
`WTK_ENABLE_SUPPLEMENTARY_CURVES=OFF` excludes optional curve sources and PRODUCT
entry points. Release saves 2,516 B Flash / 64 B RAM; enabling restores that cost
and passes the existing size gate. OSL schema/model, boot prerequisites, full wizard,
safety and W25Q recovery remain unchanged. Host ON/OFF Debug/Release and ARM profile
matrix pass. This does not complete all of A02 or physically qualify the instrument.
The next review is A06's safe PC OSL provisioning migration, not wizard removal alone.

**A06.1 STATUS: IMPLEMENTED_TESTED_HOST / SYNTHETIC_END_TO_END.**
The [PC OSL foundation](../docs/review/a06/README.md) implements a Python backend/CLI,
99-capture electrical fixture, schema-v2/model-v4 frames, simulated provisioning and
actual C solver/codec/store comparisons. Host Debug/Release ON/OFF, Python and ARM
profiles pass. Isolated Release probes measure a 3,744-B removable remainder after
retaining acquisition AND installation APIs; a future protocol cost is still estimated. Real-device
capture/install dispatch, physical qualification and boot/wizard migration are open.
The full wizard and safety/boot gates remain unchanged. A06 is not complete; proceed
through the [A06.2 checklist](../docs/review/a06/A06.2-checklist.md).

**A06.2c-prep STATUS: IMPLEMENTED_TESTED_HOST / COMPILES_TARGET.**
The opt-in factory PRODUCT excludes full wizard/session/fitting sources and LOAD
selection/rendering. It accepts only full usable W25Q OSL, preserves Resource Pack
recovery and ordinary safe measurement, and blocks blank/corrupt media. The real
C-backed 99-capture → PC solve → BRINGUP_CAL install → reset → PRODUCT controller
measurement workflow passes. Eight host combinations and twelve ARM images pass;
factory Release is 48,496 B OFF / 50,992 B ON against 62,052 / 64,476 B legacy.
See [evidence and remaining budget](../docs/review/a06/A06.2c-prep-report.md).
Legacy remains default; permanent wizard retirement and W3/W9 physical gates remain
open. No PCB or safety-threshold changes and no metrology qualification are claimed.
**A03 Calibration model unit analysis (agent: metrology mathematical)**. Read plan 13. Run synthetic OSL, scalar, two-axis and 12-coefficient WCRV candidates with independent FIT vs VALIDATION; compute rank, singularities, false accuracy claims, interval conflicts, float32 drift. Write recommendation with images/CSV on PC only. Done: evidence explaining which coefficient family can be identified by which reference standards. Do not change embedded model yet.
**A04 PC UX scaffold (agent: desktop)**. Read plan 14 and existing Firmware/tools. Implement offline reference inventory, OSL import, capture SHA inspection, condition/curve chooser, uncertainty-interval plots, suggested E12/E24 points and editable reference tolerance. Use test fixture CSV/JSON; don't write W25Q. Done: screenshot/automated GUI model test and no MCU Flash increase.
**A05 STATUS: SOURCE/FABRICATION AUDIT DELIVERED / PHYSICAL SIGN-OFF OPEN.**
The [pre-assembly audit](../docs/review/a05/README.md) checks the real epro2,
schematic/PCB PDFs, BOM and Gerber/probe/drill exports on merged PR #12 main.
149 references / 434 pads / 115 connected nets reconcile; 30/33 requested MCU pins
are carrier-connected. Seventeen extraction/reconciliation tests pass. Findings:
6 RED / 11 YELLOW / 7 GREEN / 3 UNKNOWN. The population table holds the unidentified
MCU/TFT modules, direct LED link, incompatible buzzer and unverified MOSFET lots.
K2 DNP/R0_BANK populated and TVS/link DNP agree with topology/firmware; U4 bypass
links are required. Connector documentation is corrected, firmware/PCB unchanged.
W1 has reproducible source/export evidence; manufactured-board continuity, exact
external modules, RED resolution, full native DRC, assembler sign-off and all W3
bench measurements remain open. No physical qualification or power-on approval.

**A05 Safety and physical audit (agent: hardware)**. Read plan 12 and actual
EasyEDA/schematic/fabrication outputs. Done requires hardware review signed by the
actual board assembler and discrepancies resolved before measurement; delivery of
the source audit alone does not satisfy that physical acceptance criterion.

### Sprint B — first physical board evidence
**B01 Digital safe boot**: SWD, power, UART, W25Q JEDEC, TFT/locale, GPIO safe, watchdog, charger interlock, K1/RANGE disabled boot/fault; record video/traces and test logs. Do not attach energized DUTs.
**B02 Source/ADC timing**: PWM carrier, reconstruction at 100/1k/10k, duty amplitudes, harmonic distortion, VMID, RET HG with scope and ADC raw; log probe loading; adjust timing only after backed measurement. Recalibrate/version bump on path change.
**B03 OSL and coarse metrology**: 6 resistor LOAD refs, fixture OPEN/SHORT, verify exact 33 keys where physically safe; bench-disqualify unobservable high Z or clipping conditions, never fake full 33 accuracy.
**B04 Quantities & uncertainty**: held-out resistance, stable C/coil phase sign, error versus reference tolerance, multiple re-seats and repeats, negative R/X/Q/D guards, per-condition error table. No numerical bound without evidence.
**B05 Optional DC feasibility**: one safe 1M bringup pilot, raw/oscilloscope traces, note inability to measure low-ohm DCR; no lower-RREF attempt until separately signed hardware current/dwell/offset plan.
**B06 Scope metadata**: sample CSV import, 2-ch versus 4-ch limitations, ground reference only, differential math, probe capacitance effects on high-Z. Done: reproducible captures linked to OSL and hardware SHA.

### Sprint C — integrate validated calibrator and PRODUCT
**C01 Minimal correction decision**: choose OSL only or sparse single-/two-gain correction based on B04 holdouts, not aesthetic smoothness. Decide if WCRV v1 stays in experimental BRINGUP or migrates to v2; version storage.
**C02 Desktop campaign**: FIT vs VALIDATION, interval constraints, manual custom calibration X point, recommended realistic points, graph uncertainty band, out-of-domain warnings, exported evidence and concise calibration report.
**C03 Runtime numeric error display**: provide a result with a max_error_status on every valid quantity. Display an actually qualified maximum bound only for qualified conditions; otherwise visibly 'not determined' and do not imply certification. Store compact bound values in W25Q. Full uncertainty numerical analysis stays PC side.
**C04 Optional curve installation**: controlled COM transfer to staging, validate physical qualification authorization, inactive A/B slot program, readback, CRC, commit marker last, sequence rollover, forced reset test. Normal PC-generated unqualified candidate cannot be promoted. OSL change invalidates old overlays. Blank W25Q must still recover.
**C05 UX finish**: frequency/excitation/mode (AC vs any future DC), basic and advanced pages, error figure/status, stale result indication, click measurement and coherent state after aborts, bilingual terms in external pack. Live mode only if tested against bounded K1 cycle ownership and relay wear; documentation alone does not prove implemented feature.
**C06 Release regression**: complete current CTest/Python test set, ARM Debug/Release and separate bringup builds, on-board repeatability, power-cut W25Q, faulty resource recovery, unsafe DUT rejection, check no accidental scope/UART dependency during standalone operation. Re-verify per-function linked Flash and stack budget.

## Explicit go/no-go thresholds
- No fixed 56/60KiB PRODUCT acceptance target. Current PRODUCT 64,096 B last documented. Do not change the 65,536 B silicon linker limit. Begin with a 4–8KiB linked-code reduction opportunity search, but judge success against a defensible **forward feature budget**: existing bytes - real savings + still-needed MUST feature bytes + integration overhead + maintenance headroom < 65,536 B. Document every estimate and replace with measured link deltas as tickets land. Historic 63KiB project policy stays until reviewed; avoiding end-of-project hacks is more important than a decorative number.
- Accounted RAM <=17KiB hard PRODUCT, prefer <=16KiB. Maintain documented >=2048B reserved stack and test stack usage.
- All BRINGUP variants <=65,536B and <=18KiB accounted RAM by declared budget; don't quietly overlap two diagnostic images as one.
- No product test starts without fresh safety permit; K1 SAFE on every exit; 500 mVrms never on 10Ω; 1M DC pilot BRINGUP-only until approved.
- Every AC parameter indicates test frequency and equivalent model; ESR versus DCR distinction enforced. Invalid/negative loss parameters yield n/a, not plausible numeric.
- Physical qualification gated by independent component intervals, repetitions, scope waveforms and uncorrupted device provenance; each qualified error bound has independent evidence.
- Existing calibration schema/model is treated as compatibility-sensitive. Never silently discard user's active OSL calibration.
- No explicit numeric product 'max ±x%' in domain lacking a defensible upper-bound validation method. Expanded k=2 uncertainty may be displayed only with unambiguous confidence/coverage labeling.

## Mandatory regression inventory
1. Build flags: host-debug/release, stm32-debug/release, all new bringup profiles.
2. Pure: complex division, DFT convention, 3 frequencies, ADC timing skew, HG selection and saturation, OSL degeneracy, shortened frames, unsupported keys, cross-slot order and rollover, float32 serialization, interval fit conflict, insufficient rank.
3. Functional: 33-key OSL wizard/writes, cancel/brownout, resource pack loader/upload, UI language and fallback font, button wake and long OK, measurement busy/cancel and stale numeric validity.
4. Safety: charger active, battery/NTC invalid, charged external DUT residual sensing, ADC/DMA fault, K1 stuck/relays timed out, wrong range disabled, reset during ADC window, W25Q absent/busy, ISR and watchdog.
5. Storage: CRC in each layer, malformed/duplicate keys, boot after cut at every flash-write phase, old slot rollback, OSL/overlay sequence mismatch, corrupt selected overlay no silent alternate measurement.
6. PC: imported scope incompatible units/timebase/probe mapping, fit/holdout separation, tolerance one-sided constraints, manual points off feasible range, negative/zero errors, lost COM and interrupted transfer.

## Deliverables in Git
- docs/review/ hardware inventory and as-built netlist CSV with explicitly unresolved cells.
- Firmware/tools/ flash-map parser plus baseline reports in review evidence (not blindly track generated ELF).
- Firmware/tools/ PC workbench and host validation reports; optional Windows packaging instructions.
- docs/ metrology physical qualification matrix, uncertainty methodology, reference kit worksheet and user-visible error semantics.
- plots/oscillograms/data in controlled evidence directory (large raw captures may be stored in downloadable release artifacts or external durable evidence storage with hashes rather than inflating Git history).
- code patches with safe, narrow PRs and measurable bytes saved or spent.
- product release checklist signed against actual board revision and software SHA.

## Worker briefing template
'Implement ONLY ticket <ID> from plans/15 and its detailed parent plan (11/12/13/14), based on latest main or linked planning branch SHA. Preserve AGENTS.md, safety, MCU memory limits, versioned storage and PRODUCT standalone operation. Before editing, report exact impacted files, current behavior, proof of linked Flash effect and tests. After editing, show commands/results, Flash/RAM delta per profile, artifacts, bench blockers and whether this is host-test-only or physically qualified. Do not optimize by deleting safety or returning ideal uncalibrated results as product measurements.'

## Owner approvals still required
(1) which already fitted PCB and parts; (2) instrument inventory; (3) first product's DC necessity; (4) PC-only advanced calibration accepted; (5) exact UI wording of unknown maximum error; (6) optional price/accuracy tradeoff for 0.1% versus 1% kit. Until answered, A01/A03/A04/A05 and non-electrical parts of A02 may proceed; B/C gated.

## Program completed when
A normal user can power the assembled board, calibrate by an available repeatable workflow, measure a disconnected passive component with meaningful RLC values, see the test conditions and credible error/uncertainty status, recover external assets and cancel safely; the system retains enough verified Flash margin for maintenance, and the PC app can audit/calibrate deeper properties without embedding a laboratory desktop in the microcontroller.

## Owner-approved scope refresh: no assembled board, Hantek DSO2C10, optional local O/S (2026-10-09)

A06 execution status: A06.1 host OSL foundation is merged; A06.2a adds an actual
development STM32 calibration capture service plus PC COM client. All 33 keys are
collected in a synthetic serial campaign using the production C parser/session/DSP.
A06.2b adds actual BRINGUP_CAL candidate installation, bounded transfer, W25Q A/B
commit/readback and usable-slot recovery. The C-backed 99-capture-to-reboot workflow
and interrupted transactions are tested. This is not a release or physical
qualification gate: bench validation and permanent PRODUCT wizard retirement remain
pending. Experimental PRODUCT software migration is implemented above; boot still
requires full usable OSL and valid resources. See
[installation evidence](../docs/review/a06/A06.2b-report.md).
- **Pre-assembly is now P0**: before soldering, A05 must review DNP choices K2/R0_BANK, TVS/link, optional guards and actual TFT footprint/pinout; do not say bringup is physically underway. Suggested baseline only pending schematic: K2 DNP with R0_BANK populated, D_TVS/R_TVS_LINK DNP, active guard DNP unless specific bench plan.
- No purchased reference standard components yet; source affordable 1% film resistor kit and a repeatable fixture first. Do not demand precise capacitor/inductor ESR/Q parts.
- Oscilloscope Hantek DSO2C10: prioritize waveforms, frequency/phase, source distortion, channel gain, time-domain transient evidence, and PC CSV/SCPI import; do not claim its native 8-bit input has better voltage gain accuracy than an honest 1% reference.
- **Reprioritize** calibration migration feasibility ticket A06: measure embedded 33-key OSL wizard linked bytes and implement a design for PC initial OSL provisioning plus compact optional local Open/Short fixture trim; preserve original calibration until external PC candidate verifies and commits. Detailed sign-off required before changing boot calibration gate. A04 includes PC OSL initial provisioning as MUST, not just a graphical curve viewer.
- **New optional workstream** A07/B07 plan 16 transient and DC parameter feasibility: capacitor open-circuit intrinsic leakage vs SAFE 94k external discharge, a known-load RC discharge constant, dielectric-absorption recovery, inductor RL current/energy decay and coil DCR. Engineering note: inductor stores energy in a magnetic field, not electric field. Each feature is tiered by required hardware changes, observable time window, reference uncertainty and safe stimulus. Defer complex/unreliable features without blocking AC release.
- **Fresh size forecast required** each sprint: attach pending PRODUCT must-haves, predicted code cost by measured isolated target builds, and engineering contingency; explicitly reserve time to rewrite/end a nonessential feature before reaching 64KiB. No feature stays PRODUCT solely because it was already coded.
- **Owner-approved numerical result policy**: a physical error limit only for characterized conditions; otherwise clearly 'maximum error uncharacterized'.
