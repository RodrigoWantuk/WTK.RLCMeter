# 14 — PC Calibration Workbench, Brazilian Reference Kit and Oscilloscope

STATUS: IN_PROGRESS — OSL CLI AND DESKTOP MVP IMPLEMENTED_TESTED_HOST / REQUIRES_BENCH_VALIDATION
PRIORITY: P1. A usable Windows PC utility is the standard calibration UX; Python CLI remains testable backend.

## First-board OSL execution — October 25, 2026

The owner requested engineering OSL using earlier DC checks while the Hantek is
unavailable. Scope-dependent qualification remains NOT_TESTED; the desktop B01
gate is unchanged and is not approved. The supported capture backend retains
all embedded permissions. No release qualification or energized-DUT scope is added.

Six nominal 1% LOAD candidates were inventoried at the direct DUT terminals;
three band identifications remain provisional. The owner requires a fresh DMM
reading before each LOAD attachment. Final test leads require a new calibration.
The owner additionally requires a fresh battery check before proceeding to each
new resistor value. Read only while SAFE/idle, preserve the measured voltage and
state, and request charging instead of starting that group if battery is LOW
(nominal threshold 3.50 V), CRITICAL or otherwise not ready. Never estimate charge
percentage or remaining runtime from this uncalibrated voltage. Charger interlock
continues to prohibit capture while charging. A stale/missing check must be repeated.
The verified BRINGUP_CAL boot reports hardware 0x10002/model 5 and blank storage.

The first authorized OPEN sweep stopped at its first condition (10 ohm, 10 Hz,
100 mVrms) after the independent 20-second deadline. No complete observation was
accepted or installed. A no-reset RAM snapshot showed one accepted block followed
by a second transaction with PERMIT error, hardware IDLE, and session cancellation.
The firmware returned to SAFE with zero latched faults. The historical deployment
and failed attempt remain preserved under Firmware/build/calibration-20261025.

Bounded corrective scope: app_cal_capture_shell.c waits cooperatively for real
auxiliary ADC freshness before issuing the next permit, under the existing service
deadline. app_calibration_session.c recognizes a drained inactive hardware abort
without treating it as a successful block. Regression belongs in
test_app_calibration_workflow.c. Validate host tests and affected embedded builds
before deployment; preserve W25Q and re-identify the exact binary before resuming.
Physical OPEN/SHORT/LOAD capture and independent holdouts remain incomplete.

The corrected runtime then completed ten hardware blocks without permit/runtime
errors but rejected OPEN stability. RAM statistics exposed relative-to-zero
endpoint testing and comparison of unaligned common source phase. A later UART
request was also lost during synchronous processing. The next bounded correction
is documented in docs/07-Calibration-and-Validation.md: nonzero endpoint scales,
source-amplitude repeatability, common-phase-aligned exported means, and replies
held until the repeat workflow drains. Add near-zero/excessive-dispersion controls
and independent-block phase rotation to the real C-backed capture fixture.
Validate all affected images and preserve each rejected physical attempt. Do not
reinterpret software acceptance as metrology qualification or mark scope checks PASS.

The next physical OPEN attempt reached six accepted blocks / workflow COMPLETE
with zero hardware errors, but immediate publication was blocked because the
auxiliary sensors had not finished refreshing. The service must retain BUSY
and defer publication under the original capture deadline, with fresh safety
required and explicit cancellation/timeout coverage. No pending result is valid
or installable before that gate passes.

After that correction, the first physical OPEN artifact was accepted with six
stable repeats. A following request still collided with synchronous DSP, so the
real PC client now observes the advertised capture deadline plus a one-second
quiet margin before polling, with a cancellation checkpoint and pre-START timeout
validation. Synthetic fixtures skip wall-clock waiting. The first accepted frame
is preserved byte-for-byte; resumption uses the unchanged owner-confirmed fixture.

OPEN is now complete at all 33 conditions, with six accepted repeats per key,
SAFE teardown and no new protocol errors in the quiet-window resumption.
See [physical checkpoint and current nine-image bundle](../docs/review/bringup-2026-10-25/OSL-Open.md).
SHORT/LOAD, reference confirmation at attachment, candidate generation,
installation and independent holdouts remain pending. This does not complete P14
or any electrical/accuracy qualification gate.

The first SHORT sweep also accepted 33/33, but the OPEN/SHORT path intersection
was empty for three keys. Preserve [the first SHORT/path audit](../docs/review/bringup-2026-10-25/OSL-Short.md).
A raw zero-return regression reproduced the ordinary return-SNR gate rejecting
known SHORT. The bounded correction exempts only SHORT from a nonzero return
minimum, retaining clipping, source, denominator, repeatability and all permits.
HG gain overlap retains its nonzero SNR gate. Validate the raw zero/negative
controls and embedded budgets, deploy the new identity, and reacquire both
standards under that same image before LOAD. Do not mix the prior identities.

The corrected source `7017e5e0` passed all 44 CTests and nine embedded builds.
Its verified BRINGUP_CAL deployment completed a new 33/33 SHORT sweep with both
return paths accepted at every key, six stable repeats, SAFE teardown and no new
protocol errors. See [corrected SHORT checkpoint](../docs/review/bringup-2026-10-25/OSL-Zero-Short.md).
This cohort still requires OPEN and LOAD under the same image; the earlier
source-063c7378 observations remain historical only. No candidate is generated
or installed, and physical qualification remains incomplete.

The subsequent same-image OPEN sweep is complete: 33/33 accepted conditions,
six stable repeats per key, SAFE teardown and unchanged zero protocol errors.
The [66-capture endpoint checkpoint](../docs/review/bringup-2026-10-25/OSL-Same-Image-Endpoints.md)
verifies matching identity, canonical CCO1 metadata/CRC/SHA and a common return
path at all 33 OPEN/SHORT pairs. This is necessary acquisition compatibility,
not a completed OSL fit: HG-only triplets still need actual same-key overlap.
LOAD captures and serialized solver checks remain pending, followed by separate
installation and independent holdouts. The owner has been asked for the fresh
10-ohm DMM reading before the first LOAD attachment. No qualification gate changed.

The owner repeated 10.5 ohm on the Minipa 200-ohm range and explicitly confirmed
the nominal 10-ohm resistor attached alone. All three 100-mVrms LOAD keys passed
six repeats, both return paths, clipping zero, observed HG overlap and the strict
same-image OSL/float32 fit checks. See [first LOAD checkpoint](../docs/review/bringup-2026-10-25/OSL-Load-10.md).
Progress is 69/99 observations, 3/33 complete conditions. Nominal 10 ohm +/-1%
remains an uncertain fit anchor, not an exact lead-corrected DMM reading. Fresh
pre-group battery readings were 3.71824 V before 10 ohm and 3.71663 V before
100 ohm, both OK. A post-capture read-only STATUS timeout increased the protocol
counter to 1; a paced read-only recovery confirmed SAFE/idle and zero faults.
It did not replay START or change accepted artifacts. The owner was asked for
the fresh 100-ohm reading/attachment. No complete candidate or installation yet.

The owner reported "100R exatos, está conectado". All six 100-ohm LOAD keys
passed acquisition and strict same-image OSL/float32 checks; progress is now
75/99 observations and 9/33 complete conditions. Saturated HG paths were rejected;
the 10-kHz/100-mVrms fit used valid HG with actual same-key overlap evidence.
Fresh batteries were 3.71341 V before this group and 3.71502 V before requesting
the 1-kohm reading/attachment, both OK. No faults or protocol-counter increase
occurred during the group. See [100-ohm checkpoint](../docs/review/bringup-2026-10-25/OSL-Load-100.md).
Four reference groups, independent validation, candidate generation and
installation remain pending. Printed nominal/tolerance remains the uncertain
fit anchor; the fresh DMM display does not establish traceable accuracy.

The owner repeated 992 ohm and confirmed the 1-kohm reference attached alone.
All six LOAD keys passed acquisition and strict same-image float32 OSL checks,
with 1X selected for every fit and clipped HG rejected at 500 mVrms. The
[1-kohm checkpoint](../docs/review/bringup-2026-10-25/OSL-Load-1000.md) preserves
81/99 observations and 15/33 complete triplets. Fresh batteries were 3.71502 V
before this group and 3.71179 V before requesting the 10-kohm reference, both OK.
Protocol counter stayed at 1 and fault mask at zero. Nominal 1 kohm +/-1% remains
the uncertain reference anchor; no exact lead correction or DMM uncertainty is
invented. Three LOAD groups, independent validation and installation remain pending.

The owner repeated 9.99 kohm and confirmed the 10-kohm reference attached alone.
All six LOAD keys passed acquisition and strict same-image float32 OSL checks.
The [10-kohm checkpoint](../docs/review/bringup-2026-10-25/OSL-Load-10000.md)
preserves 87/99 observations and 21/33 complete triplets with explicit path and
clipping evidence. Fresh batteries were 3.70857 V before this group and 3.70696 V
before requesting the 100-kohm reading/attachment, both OK. Protocol counter
stayed at 1 and fault mask at zero. Nominal 10 kohm +/-1% remains the uncertain
fit anchor. The 100-kohm and 1-Mohm LOAD groups, independent validation,
candidate generation and installation remain pending.

The owner repeated 99.8 kohm and confirmed the 100-kohm reference attached alone.
All six raw LOAD observations are preserved, but only five fit checks pass.
At 100 kohm / 10 kHz / 100 mVrms, OPEN and LOAD are HG-only, SHORT has both paths,
and no standard has observed HG overlap. The strict solver rejects this triplet;
do not substitute nominal gain, invalid 1X ratios or changed quality flags.
The first sweep stopped after preserving that raw observation and rejection.
Only the previously unattempted 10-kHz/500-mVrms key was then captured separately
under the unchanged confirmed fixture; it passed with observed same-key overlap.
No rejected START was replayed. The [100-kohm checkpoint](../docs/review/bringup-2026-10-25/OSL-Load-100000.md)
records 93/99 raw observations, 27 complete triplets, 26 solvable fits and one
blocked fit. This disproves the bench assumption that a common OPEN/SHORT path
alone is sufficient to finish every OSL key. It does not establish a component
fault. Investigate gain observability/coverage within the current metrology
contracts; keep the blocked key incomplete rather than asserting qualification.
Fresh batteries were 3.70696 V before this group and 3.70374 V before asking for
the 1-Mohm reference, both OK. Protocol counter stayed at 1; safety faults zero,
SAFE/idle confirmed. Six 1-Mohm LOADs, blocked-key resolution, independent
validation, candidate generation and installation remain pending.

The owner freshly measured the 1-Mohm reference as 996 kohm and confirmed direct
attachment. All six raw captures passed acquisition; four fits passed and both
10-kHz fits remain blocked by absent observed HG overlap. The same-image campaign
now has all 99 observations, 33 triplets, 30 solvable fits and three blocked keys:
100 kohm/10 kHz/100 mVrms and 1 Mohm/10 kHz at both amplitudes. The full candidate
builder rejects this coverage; no candidate or installation is produced.

After explicit owner confirmation of empty terminals, exactly those three OPEN
keys were captured again once, with the same HG-only/no-overlap result. Keep the
original 99 artifacts unchanged and the diagnostic frames separate. A SAFE/idle
no-reset snapshot of the final 1-Mohm/10-kHz/500-mVrms LOAD measured relative
complex standard deviations: 1X Z 8.3275%, HG Z 0.9996%, HG transfer 7.5451%,
against the unchanged 2% workflow criterion. The source amplitudes vary only
0.1929%/0.1544%. These observations explain rejection but do not identify a
component fault or qualify accuracy. See [complete capture/diagnostic checkpoint](../docs/review/bringup-2026-10-25/OSL-All-99.md).
Battery was 3.70535 V before the 1-Mohm group and 3.70374 V afterward, OK. Safety
faults remain zero and protocol counter unchanged at 1. Investigate raw DMA,
timing/noise and path observability before selecting a corrective implementation;
do not invent gain, silently mix paths or relax the full-set installation contract.

The owner additionally requested an NTC comparison using a contact culinary
thermometer, with its model intentionally not requested or recorded. The owner
will hold it at the same thermal point as the NTC and report a stable reading.
One-point comparison/trim does not determine Beta or certify the full curve.
Reference temperature, paired raw NTC evidence and persistent correction are
pending. The current lookup table has no existing runtime NTC trim interface;
design any required versioned storage explicitly before changing firmware.

The first external reading was 23.8 degC at the owner-described PCB thermal point.
A paired SAFE/idle no-reset snapshot reported NTC raw 1958, 1.577875 V,
91623.76 ohm and 27.13364 degC (sample age 664 ms). This is a 3.33364-degC
comparison difference, not yet a justified correction: thermal contact may differ
between the probe, PCB and bead. The owner was asked for direct isolated-bead
contact, approximately 60 seconds of stabilization, and another reference reading.
No NTC correction has been applied and no thermometer model is recorded.

Direct isolated-bead contact gave a corrected external reference of 23.5 degC.
The paired firmware reading was 27.27030 degC, raw 1952, 91087.27 nominal ohm,
age 236 ms, difference 3.77030 degC. The owner could not read a resistor marking;
the purchased thermistor listing says only "NTC 3mm 100k", so B3950 is not
confirmed. Do not fit Beta from the two different-contact cold comparisons or
silently install a fixed offset as a full-curve calibration.
See [cold NTC evidence](../docs/review/bringup-2026-10-25/NTC-Comparison.md).
The owner was asked to gently warm the bead and contacting probe together to
approximately 30–35 degC, stabilize, and report the next reference. Two actual
temperature points can fit effective R25/Rtop and Beta from raw divider ratios;
this avoids pretending Rtop or R25 was independently measured. Reference
uncertainty and broader curve qualification remain unresolved. Persistent
runtime calibration still requires an explicit versioned storage/API design.

A04.1 delivers the [guided OSL campaign](../Firmware/tools/PC_OSL_CAMPAIGN.md):
six-reference inventory, explicit passive-fixture confirmations, atomic resumable
CCO1 evidence, real C-backed 99-observation workflow, existing 33-key solver and
canonical candidate/report export. Installation remains a separate authorized
utility. [Software evidence](../docs/review/a04/README.md) does not complete the
broader GUI, plots, scope import, independent holdouts or physical qualification.

A04.2 supplies the [Qt desktop OSL subset](../Firmware/tools/PC_OSL_WORKBENCH.md):
reference forms, 33-key progress, human-confirmed captures, serial worker,
resumption/recapture, B01 import/operator gate and candidate export without installation.
[Desktop evidence](../docs/review/a04/A04.2-report.md) distinguishes C-backed simulation,
local Windows packaging and unchanged firmware from pending physical qualification.
Plots, scope import and the broader workbench remain outside this MVP.

A04.3a implements the [offline Oscilloscope tab](../Firmware/tools/PC_SCOPE_WAVEFORM.md):
generic explicit CSV mapping/scaling, bounded plots/zoom, numerical rail/excitation/
clamp/RET presets, uncertainty inputs and source-hashed B01 sidecars. Native Hantek
layout remains unverified/unsupported pending a real export; no SCPI or physical
qualification. [Software evidence](../docs/review/a04/A04.3a-report.md) records
headless tests, Windows packaging and zero firmware delta. P14-08's generic CSV
subset is tested; native adapters, real scope measurements and broader plots remain open.

B02.2 updates the active software contract to 10 Hz / 1 kHz / 10 kHz, schema-v2/
model-v5 (same OSL equations). The [preflight](../docs/review/b02/B02.2-report.md)
corrects VMID-referenced residual permission and supplies synthetic endpoint
feasibility. Earlier 100 Hz/model-v4 descriptions below are historical. No physical
qualification, energized-DUT permission or large-capacitor approval is granted.

## 1. Acquisition/procurement philosophy
Design for small Brazilian electronic component stores. Ordinary E12/E24 values, through-hole 1/4W metal-film resistors, C0G/NP0 ceramic/film capacitors, and generic coils should complete first-pass validation. Printed tolerance is accepted as an interval and affects the eventual *best-possible confidence*: an uncertified 1% resistor alone cannot justify 0.1% overall accuracy. A scope measures waveform behavior, not automatically reference impedance or leakage current. Advanced precision/guarded standards can be optional upgrades.

## 2. Baseline kit, prioritized quantities and alternatives
| Group | Suggested values / quantity | Preferred | Acceptable accessible alternative | Use / note |
| --- | --- | --- | --- | --- |
| Absolute primary resistive LOAD | 10Ω,100Ω,1kΩ,10kΩ,100kΩ,1MΩ; at least 1 each, ideally 2 from distinct lots | individual 0.1% metal film with explicit TCR/datasheet, if affordable | 1% metal film at 1/4W or higher with datasheet; measured individually with known-accuracy DMM | Existing OSL nominal ranges; user enters measured value or published interval. Include power check and no LED/resistor-network surprises |
| Independent resistor holdouts | 33Ω,330Ω,3.3kΩ,33kΩ,330kΩ, optional 3.3MΩ; plus 4.7Ω or 47Ω as practical | 0.1..1% | common 1% E12/E24 | Check curves between/near knots; 3.3MΩ/high-frequency high-Z may not be qualified |
| Low-ohm exploratory | 1Ω,2.2Ω,4.7Ω,10Ω (single known standards), shorting copper link | 1% metal-film, current-rated | 5% resistor with larger error; skip precision claim | Scope/DMM physical contact and lead correction dominates 0.1..1Ω; no false four-wire precision |
| Short and open | identical DUT terminal geometry, short copper strap, repeatable open/insulating carrier; 2 each if practical | gold-plated small fixture with mechanical relief | soldered jumpers and rigid test clips | OPEN/SHORT re-seat repeatability can dominate curves; label IDs |
| Stable capacitors | 1nF,10nF C0G/NP0 where feasible; 100nF polypropylene film; optional 100pF C0G | 1..5% known tolerance, clearly specified voltage/frequency | 5..10% film/C0G with tolerance kept; X7R ONLY exploratory | Characterize phase/reactance; nominal C does not calibrate ESR unless ESR separately bounded |
| Higher capacitors | 1µF film if accessible, optional 10µF electrolytic | film with published tolerance | 10µF electrolytic 10..20% as holdout only | Useful at 100Hz/1kHz but dielectric/ESR behavior changes; no reference-class ESR assumption |
| Inductors | 100µH,1mH,10mH; 1 each, with datasheet test frequency, current, Q and DCR if available | shielded/axial through-hole 5..10% test conditions specified | 10..20% common choke as qualitative holdout | No nominal-only Q/ESR claim; protect against biased/saturated coil |
| Thermal/environment | independent thermometer, safe enclosure / ambient stabilized location | thermometer traceable or cross-checked against known reference | known-accuracy room thermometer with explicit rating | Calibrates board NTC estimate, not DUT temperature |
| Instruments | 2+ channel oscilloscope with ×10 compensated probes, digital multimeter, USB-UART 3.3V, SWD tool, current-limited bench supply | 4-channel scope with CSV export and DMM spec | 2-channel scope and manual input with limitations recorded | Scope bandwidth/timebase & probe loading captured; do not substitute scope for verified DC voltage standard |

Procure primary six resistors, simple OPEN/SHORT fixture, DMM/scope wiring first. Then add 33/330/3.3k/33k/330k and C/L controls for independent validation. Do NOT force user to buy all optional kit before the first functioning RLC measurement. The site/price availability must be rechecked at purchase time; do not commit retailer stock as permanent test truth.

## 3. Calibration numbers must be chosen for observability, not marketing
- Nominal RREF 10/100/1k/10k/100k/1M; OSL LOAD near the reference. Product's existing on-board wizard offers NOMINAL/E12 LOW/E12 HIGH presets; if physical standards differ, add PC arbitrary known-reference input before using them. Keep track of default presets versus actual calibrated lot.
- For C, target an impedance order near selected reference: |X_C|≈1/(2πfC). At 1kHz, 100nF≈1.59kΩ, 1µF≈159Ω, 10nF≈15.9kΩ, 1nF≈159kΩ; examples are ideal approximate magnitudes, not actual measured C or ESR.
- For L, |X_L|≈2πfL. At 1kHz, 1mH≈6.28Ω and 10mH≈62.8Ω, but series winding R can dominate. Select stable components with matching datasheet frequency and Q/DCR details; only one nominal value is not enough to calibrate independently phase and resistance.
- At high impedance/frequency, test probe capacitance and fixture parasitics before trusting 1MΩ reference / 10kHz. The DUT remains disconnected and discharge verified for fixture changes.
- All test components should have IDs, vendor/part, lot, photograph, manufacturer tolerance, temperature coefficient, maximum rated voltage/power, frequency conditions, optional DMM measurement and its accuracy/date, source of numbers and whether standards are FIT or VALIDATION.

## 4. Oscilloscope capabilities and safe channel mapping
Scope reads waveforms; it should not create measurement voltage on the DUT unless a separate externally authorized experimental interface exists. Scope ground clips of ordinary earth-referenced bench oscilloscopes connect to earth. **Attach all probe grounds only to true circuit GND. Never attach ground clips to VMID, RET, VEXC or TEST_HI/LO** — this can short circuit the analog network. Use two scope GND-referenced inputs and scope math CH1−CH2 for VEXC−VMID, similarly RET−VMID, or a properly rated isolated differential probe. Confirm chassis/USB ground topology before connecting equipment.
- Scope mapping table: CH1=VEXC relative to GND, CH2=VMID relative to GND, MATH=VEXC−VMID. For RET waveform another channel (CH3=RET) or a repeat acquisition with the same fixture/condition. For both simultaneously and reliable phase differences a 4-channel scope is preferable: CH1 VEXC, CH2 RET, CH3 VMID, CH4 optional RET_HG; compare MATH differences and clipping. With two channels avoid claiming simultaneous phase comparison if channels were captured sequentially.
- Record ×10 probe attenuation, compensated-probe status, bandwidth limit, DC/AC coupling, scope ADC vertical scaling/offset, acquisition mode, sample rate, timebase, trigger, high-pass filtering, RMS type (AC only versus AC+DC), calibrated vertical/timebase accuracy and target supply voltage.
- x10 probes commonly load with order-of-magnitude 10MΩ and pF capacitance; at 10 kHz even 10pF has |X| ≈1.59MΩ. Measure perturbation by scope connection/disconnection. This is an **illustrative calculation**, not a verified input capacitance for any particular probe.
- Extract: measured source frequency and jitter, differential sine RMS and peak, DC offset relative VMID, settled waveform THD/harmonic peaks (only if scope bandwidth/SNR permit), 450kHz PWM carrier ripple, transient/settling vs duty change, phase between VEXC and RET, RET HG gain/phase ratio, comparator clipping, K1 and range transitions. Source RMS must not be confused with raw GPIO PWM level.
- Scope phase reference requires simultaneous ground-referenced channels, known interchannel skew, correct polarity and integer cycles. Test with a midrange resistor expected roughly 0°; capacitor negative phase, inductor positive under defined firmware convention. Compare to raw DSP before applying a curve.
- OSL and high-Z captures should be repeated **without** probes to quantify scope loading. If probe changes readings beyond reproducibility, mark scope-assisted calibration invalid for that condition unless its loading is explicitly modeled.
- DMM at appropriate range measures DC supply, physically known reference resistors and offsets (with lead compensation when its accuracy supports it). A commodity DMM cannot certify sub-ohm contact resistance, nanoampere leakage or AC ESR unless explicitly rated/tested.

## 5. PC workbench architecture and API
P14-01. Retain existing Python pure tools and formats (pc_capture, pc_dc_pilot, pc_cal_frame, pc_campaign_add, calibration_campaign, calibration_curve_frame, calibration frame inspectors). Refactor common parsers into a tested library; maintain CLI compatibility.
P14-02. Add desktop application using Python+PySide6 (open source under applicable deployment/licensing review) or a no-license-cost web-local frontend; choose based on packaging and reproducible offline Windows executable. Never force cloud login, paid services or online retailer access. Backend owns deterministic calculations and binary wire serialization; UI is an independent consumer. If a GUI framework is unavailable, provide temporary local web UI and keep target MCU unchanged.
P14-03. Support device discovery for COM port, 115200 8N1 with version/identity, active OSL sequence/CRC, board revision, temp, range status, fault/interlock, W25Q resource/calibration status. USB-UART on PA9/10, not Blue Pill native USB.
P14-04. Add read-only guarded capture API with bounded binary framing, SHA256 output, request ID, sequence, length, CRC, idle/busy, timeouts, safe-state acknowledgement. Serial data is never an authorization to change K1 without device permit. Existing raw wire frames remain supported and hash-stable.
P14-05. Add campaign manager with disk JSON/SQLite records: project/device/fixture/reference inventory, test conditions, scope traces, raw file checksums, curve IDs, calibration temperatures, operator notes and session signatures. PC stores history and graphs; MCU stores only active compact coefficients and error envelopes.
P14-06. Add task-oriented wizard: (a) start hardware validation, (b) basic OSL, (c) resistor gain curve, (d) capacitive/reactive curve, (e) optional thermal, (f) guarded DC study; ensure no accidental calibration upload. For every step show exactly what instrument pins/scope channels/reference component to connect, measurement X/Y, required current/rating, range/frequency/amplitude, repeat count, criteria, DUT safe disconnected prompt.
P14-07. Interactive plots: raw vs OSL vs candidate correction, points with reference tolerance boxes, error bands, user custom X point, suggested E12/E24 purchases and 'unattainable on this range' explanations, held-out points rendered distinctly, fit rank/condition number, error budget/quality class. Zoom, change X axis between log|Z|/RREF and engineering units, grouped by frequency/range/temp.
P14-08. Scope import wizard: manual entry and CSV/SCPI for make/model profile, channel mapping/calibration/probe settings, source capture timestamp, SHA, sample rate, trace validation, background removal, harmonics and phase. No universal SCPI claims: support generic CSV first, then vendor-specific drivers and tests.
P14-09. Use existing SHA-bound OSL/calibration campaign format and make physical provenance honest: file hashes and CRCs prove byte continuity only, not fixture attachment or genuine board identity. Display evidence level and require human confirmation of actual standard.
P14-10. Transactional overlay installation is **blocked** until owner signs metrology model and qualification recipe: prepare transfer staging, inactive W25Q slot, readback CRC, qualified flag generated only by authorized verified action, OSL invalidation, power-cut rollback. Never let a normal host-generated candidate set its qualified flag.
P14-11. Package reproducibly for offline Windows usage, document pyserial/driver/SWD steps, unit-test CLI and GUI model, provide simulator with fake port/captures to test screens without hardware.

## 6. Exact lab sequence
S0. Bench photograph, schematic-to-netlist, measure supply+charging interlock, firmware SHA, W25Q healthy, safe boot, GND map.
S1. Open fixture: repeat acquisition at all allowed keys; capture phase, ADC code floors, high-impedance leakage and probe loading; keep plots.
S2. Short fixture: same geometry/temperature and 5+ reattachments; collect raw residual complex impedance pre-OSL, relay/contact repeatability.
S3. LOAD near each RREF: actual resistance interval/value, 100Hz/1k/10k and allowed amplitudes, with supply/current limits; fit existing OSL. Reject outside safe ADC/harmonic windows.
S4. Held-out R values at other q; evaluate relative and additive residual error vs OSL; only enable added correction on conditions where evidence reduces held-out error and does not worsen reattachments.
S5. C0G/film capacitor and coil checks, coherent phase and frequency conditions; verify X sign, Q/D/ESR derivation, n/a gates and temperature compatibility.
S6. Oscilloscope calibration-specific study on 100 Hz/1 kHz/10 kHz source, HG gain/phase, transient and 450kHz residual carrier; scope loading control.
S7. Optional DC 1M pilot with known res + open + short; never lower RREF without current/dwell go/no-go. Record negative results as useful evidence.
S8. Attempt interrupted writes and missing W25Q (with safe test setup); verify old A/B set remains, calibration mismatch invalidates overlay, no stale numeric results.
S9. Final independent validation report and human sign-off, followed by explicit per-quantity/domain uncertainty tables.

## 7. Purchase and owner decisions
Owner should provide existing DMM and scope model; existing 1%/0.1% resistor inventory; whether a precision LCR meter is available (not required); budget and whether optional 0.1% purchase is justified. We do not mandate precision or certified ESR standards to produce a basic usable device. Report a minimal shopping list, OPTIONAL list, approximate Brazilian single-unit part category and supplier alternatives, rather than one hard-to-find BOM brand.

## Acceptance
One-click offline PC evidence import, interactive curve plots and custom/suggested points, explicit reference tolerance and uncertainty, scope procedure safety warning, reproducible RAW hashes, independent holdouts, corruption/rollback tests, and firmware Flash impact measured. No data is labeled calibrated merely because the curve visualization looks smooth.

## Confirmed equipment and acquisition UX (2026-10-09)
- Owner has NO reference component kit yet. Start purchase guidance with a small set of affordable ±1% metal-film resistors, lower-precision available C/L as validation only, and a repeatable OPEN/SHORT fixture. Do not ask for large 0.1%/0.01% shopping list to get basic product working. Show the user separately the purchase cost/expected confidence benefit before recommending upgraded standards.
- Available scope: **Hantek DSO2C10**, two channels, nominal 100MHz, 1GSa/s single or 500MSa/s dual, 8-bit normal samples / digital high-resolution processing up to 12-bit, vertical gain ±3% on regular ranges, ±4% smallest, ±25ppm timebase, CSV/SCPI; **no integrated arbitrary waveform generator in 2C10** (DSO2D10 has it). Official specs: https://www.hantek.com/products/detail/17182. Probe attenuation and impedance must be recorded. Scope ground remains common even when powered from an isolation transformer; do not remove safety earth or claim independent floating channels.
- PC calibrator MUST support a DSO2C10 CSV import profile, metadata parser, units, timebase validation, two-channel differential math via GND reference, waveform/scope accuracy error fields and a safe cross-scope test of PWM 450kHz, the 100Hz/1kHz/10kHz source, HG gain/phase and decay constants when physically observable. Document scope firmware-dependent SCPI commands only after verified command capture; no assumption of undocumented APIs. With 2 channels CH1=VEXC to GND / CH2=VMID to GND yields source differential, but simultaneous VEXC/RET/VMID three-way phase needs reconfiguration or additional instrumentation; don't claim cross-capture phase synchrony.
- Calibrator must provide **full PC OSL** with arbitrary actual standard value/tolerance and full candidate readback/write, plus **optional in-device OPEN/SHORT only** if firmware cost allows. It must guide initial factory/PC provisioning before the standalone device is called CALIBRATED, and unqualified fallback must never be displayed as qualified.
- Include a special transient feasibility evidence page: capacitor intrinsic leakage versus fixture 94k SAFE bleed, external known-load discharge, dielectric absorption, RL energy/current decay. Declare if measured time constant belongs to DUT, fixture or series source path; never call a derived time constant 'natural' when a known resistor dominates it.
