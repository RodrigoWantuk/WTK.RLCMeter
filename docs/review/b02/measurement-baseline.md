# B02.0 — analog expectations and affordable calibration baseline

**All numbers below are calculations or cited device specifications, not measured
Rev.1 results. REQUIRES_BENCH_VALIDATION.** See [assembly decisions](assembly-decisions.md)
and [manual bench sequence](bench-card.md). Original source/export checks pass;
firmware/PCB/build inputs are unchanged, so no new ARM matrix is run. B01.1's
[linked sizes](../b01/B01.1-sizes.csv) remain the baseline (BRINGUP 65448 B,88 B margin).

## Rails, idle and analog paths

Source-derived: RA 4.7 Ω feeds +5V_A from +5V_SYS; nominal reservoir 220 µF+10 µF+1 µF+
100 nF gives RC≈1.09 ms, **not a guaranteed rail ramp** (loads/regulator/input supply
dominate). Analog drop is 4.7 Ω×measured AFE/gate current. RVMID1/2 10 kΩ/10 kΩ gives
VMID_RAW=(+3V3)/2; CVMID1/2 10 µF+100 nF with 5 kΩ Thevenin gives τ≈50.5 ms/10–90%≈111 ms.
Capacitor DC-bias/tolerance and probes change this. Buffered VMID is U4.1 via
R_BYP_VM to U4.2/RADC2.2; not “half the 5 V supply.”

**OFF is different from NEUTRAL:** bsp_excitation_off stops TIM1 and drives PA8 LOW;
FILT3/VEXC approach 0 V. NEUTRAL is 50%duty 450 kHz, average≈(+3V3)/2. During a valid sine,
VEXC has that bias plus AC. With range disabled and K1 SAFE, RET has no guaranteed
DC bias (only leakage/input/protection paths); don't demand all return outputs
equal VMID or assume OFF prevents HG saturation. With selected range and neutral,
RET tends toward VEXC; with matched passive LOAD and K1 MEASURE it lies between VEXC
and VMID. Residual empty terminal nodes ideally tend toward VMID through their bias
networks, but firmware must decide fresh safe residual status.

## Loaded reconstruction and actual firmware amplitude

RF1–3=5.1 kΩ, CF1–3=1 nF to GND, U4 input approximately unloaded:
`s=j2πfRC`, **H=1/(1+6 s+5 s²+s³)**. This is not `(1+s)^−3`.
KCL matrix `[[2+s,-1,0],[-1,2+s,-1],[0,-1,1+s]] · [V1,V2,V3] = [Vin,0,0]`
independently reproduces the closed-form values below (<1e−12 complex difference).
Component tolerances/loading/U4 response remain outside this exact nominal ladder.

| Frequency | Ladder amplitude / loss | Ladder phase vs averaged PWM | Predicted VEXC AC RMS, encoded 100 /500 mV settings at 3.3 V |
| --- | --- | --- | --- |
| 100 Hz | 0.999867 / −0.00116 dB | −1.10° | ~100.8 /499.2 mV |
| 1 kHz | 0.986906 / −0.114 dB | −10.94° | ~99.5 /492.7 mV |
| 10 kHz | 0.512455 / −5.81 dB | −75.56° | ~51.6 /255.9 mV |

Amplitudes include the source-code integer CCR fundamental/ZOH and a **typical
one-pole 1 MHz unity buffer estimate**, not a guaranteed output. hw_excitation uses
45 samples/cycle, ARR159 (160 ticks), center 80 and peak Q8=1755/8777, rounded to ±7/
±34 ticks. Actual duty table fundamental at 3.3 V is 100.787/499.279 mVrms after ZOH;
the 100/500 names are requested classes, **no measured-frequency amplitude servo**.
Sample hold contributes≈−4° relative to an ideal table-point sine; DMA/start timing
adds its own phase. Therefore ladder phase is not absolute burst phase. Typical
U4 lag adds≈0.006/0.057/0.573° at 100/1 k/10 k. DSP uses observed complex Vs=VEXC−VMID;
do not “correct” this loss by changing PWM during this task.

At 450 kHz the ladder magnitude≈0.00032346 (−69.8 dB). A 50% ideal 3.3 V square's
first carrier harmonic is 2×3.3/π volts peak: FILT3≈0.680 mVpeak before U4; the crude
1 MHz buffer model predicts≈0.620 mVpeak at VEXC. Higher harmonics, modulated sidebands,
ground pickup and actual buffer/PCB behavior differ. A flat trace cannot prove this
small ripple absent: use a **separate** AC-coupled high-rate zoom (e.g.1 µs/div,
≥50 MSa/s,20 MHz bandwidth initially, both channels if comparing), record probe/noise
floor; smallest voltage ranges/probe attenuation impose worse accuracy. Do not infer
450 kHz ripple from low-rate 100 Hz records. Total RMS includes DC; sine-only
`Vrms_ac=Vpp/(2√2)` is valid only after checking distortion.

## Return gain, clipping and probe loading

HG equation: `VHG=VMID+G(RET−VMID)`, `G=1+68k/4.7k=15.4681`.
Approximation `G/(1+jfG/1 MHz)` gives:

| f | Typical-model HG amplitude ratio | HG phase vs RET |
| --- | ---: | ---: |
| 100 Hz | 15.4681 | −0.089° |
| 1 kHz | 15.4662 | −0.886° |
| 10 kHz | 15.2863 | −8.79° |

RET_1X unity-buffer lag is≈−0.006/−0.057/−0.573°; relative HG-to 1X lag is about
−0.083/−0.829/−8.22°. These are engineering predictions based on typical GBW,
not calibrated gains or pass bands. RADC 1 kΩ/CADC 1 nF adds another single-pole:
gain/phase at 10 kHz ≈0.998032/−3.60°, plus diode/pad/sample-switch effects. The op-amp
has load-dependent swing; neither “rail-to-rail” nor typical 23 mA short current is
a rated distortion-free drive capability. 10 Ω/100 mV SHORT implies 10 mArms/14.1 mApeak
before losses; 10 Ω/500 mV remains forbidden.

At VMID 1.65 V and measured 3V3≈3.3 V, HG's nominal ADC ceiling requires
`|RET−VMID|peak < min(VMID,3V3−VMID)/G ≈106.7mV` (~75.4 mVrms sine), less margin
for offsets/noise. OPEN gives RET≈VEXC:100 Hz 100 mV output predicts HG≈2.20Vpeak about
1.65 V, clipping low and exceeding the ADC upper rail;500 mV is much worse. At 10 k
the attenuated 100 mV OPEN may be within headroom, but phase/loading still need proof.
The bench card therefore chooses this allowed 10 kHz/100 mV/1 kΩ key for the first
authorized OPEN, **after** matched LOAD waveform/headroom checks at 10 kHz; 100 Hz
OPEN remains a separate clamp gate. This does not qualify the complete 33-key set.
Matched 1 k LOAD/1 k RREF at 100 Hz predicts RET≈50.4 mVrms, HG≈0.779 Vrms (~0.55–2.75 V),
which is the low-energy starting operating point. At 10 kHz source amplitude is only
~half, not a design-failure diagnosis by itself.
Nominal matched-load current is ~50 µArms and resistor dissipation ~2.5 µW;
the ~40 mA relay coil, not this DUT load, dominates the added supply current.

Use simultaneous buffered U5.1/U5.7 with matched/compensated probes, remove means,
measure AC ratio and same-cycleΔt (`phase=360fΔt`). Swap probes/channels on the
**same benign circuit** to expose imbalance. Same-channel amplitude comparisons
across stable runs can cancel some gain error, but have no common absolute phase.
Do not measure HG gain by dividing 1.65 V DC-biased RMS values. Probe on RET is an
added DUT load: a 10MΩ probe parallels 1 MΩ to 0.909MΩ even before capacitance; an
illustrative 10 pF at 10 kHz has |Xc|≈1.59 MΩ. Actual probe capacitance must be identified,
not assumed from the scope BNC specification. Remove RET/fixture probes for OSL.

## Y04 — resistor current is not a diode-current certificate

Actual pads for simultaneous comparisons (all grounds GND):

| Channel | Amplifier-side tip | ADC-side tip / clamp midpoint |
| --- | --- | --- |
| VEXC | RADC1.2 / R_BYP_VEXC.2 (U4.7 before 0 Ω) | RADC1.1 / DCL1.3 / USTM32.25 PA0 |
| VMID | RADC2.2 / R_BYP_VM.2 (U4.1 before 0 Ω) | RADC2.1 / DCL2.3 / USTM32.26 PA1 |
| RET1X | RADC3.1 / U5.1 | RADC3.2 / DCL3.3 / USTM32.27 PA2 |
| RETHG | RADC4.1 / U5.7 | RADC4.2 / DCL4.3 / USTM32.28 PA3 |

Positive path:5 V amplifier→RADC→ADC midpoint→BAT54S upper diode→3V3; the lower
diode returns negative excursions to GND. `I_R(t)=(VOUT−VADC)/Ractual` also supplies
CADC displacement current and ADC sample current; it is **not automatically external
diode current or net injected 3V3 current**. Near steady clamp plateau one can infer
possible upper conduction, but separation needs circuit/current evidence. Record
fitted R tolerance and channel/probe errors. Conservative estimate uncertainty:
`δI ≲ (δVOUT+δVADC)/R + |I|·δR/R`; differences of two large voltages can have very
large relative error. Channel/probe swap/zero checks help; no fictitious error bound.

Illustrative assumptions only: VOUT5.0 V,3V3=3.3 V,Vf 0.30 V,R1 k →1.40 mA and 1.96 mW
in R; rail 0 V with the same live output →4.70 mA/22.1 mW perpath. Four hypothetical
live 5 V outputs could supply 18.8 mA total before other loads. Actual output swing,
Vf(I,T), internal clamp sharing and all four channel states matter. BAT54S datasheet
maximumVf at 0.1/1/10 mA is 0.24/0.32/0.40 V at its specified conditions, **not fixed 0.3 V**.
The documented current numbers do not approve partial-power operation.

3V3 passive loading is small: VMID divider≈0.165 mA, NTC divider≈0.0165 mA at 25°C;
CS/WP/HOLD pulls mostly draw no DC when HIGH. Running MCU may plausibly absorb a
1–fewmA injection, but its consumption collapses during supply decay, reset/stop or
disconnection. **No regulator part was supplied; current sinking is unestablished**.
Use `Crail·dV/dt = Isource_reg + Iinjected − Iloads − Ishunt`. A source-only regulator
reduces its sourcing; it cannot guarantee rail clamping. VMID/COUT stored energy,
floating RET/HG saturation and partial-power transients can cause trouble **with
excitation OFF**. Firmware cannot remove the amplifier supply/diode path.

The current [ST datasheet](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
(DS5319 Rev 20 §§5.2,5.3.12,5.3.18) advises avoiding normal injection; stress current
limits are not qualified ADC conditions. It warns about negative injection affecting
other analog channels, while noting allowed positive injection does not affect ADC
accuracy under its characterization conditions. That does **not** validate our
external-rail rise, clipped waveform or powered-down state. ADC nominal input range
is 0..VDDA, metrology sampling is 7.5 cycles at 12 MHz (~0.625 µs), and software ADC scale
3.3/4095 is nominal, not a measured 3V3 supply. Observe actual rail/sensors, clipping
and waveform errors; don't assert every small positive current necessarily causes
ADC error or that stress limits prove safety.

Manual diagnosis: ordinary startup/shutdown with +5V_A/+3V3 pair first; then
authorized matched LOAD output/ADC pair; then separate output/rail run. Inspect for
rail rise/rebound, ADC exceeding measured rail, sensor drift and saturation before
authorizing OPEN. Do **not** disconnect the regulator to deliberately induce
backfeed, increase excitation to find a failure, or combine separate runs as 3CH.

Contingencies **only after a demonstrated problem and reviewed evidence**:

| Order | Physically effective option / constraints | Quantitative consequences to check before adoption |
| --- | --- | --- |
| 1 | Restrict amplitudes/fixtures to observed nonclipping paths; firmware permission remains unchanged | Helps energized HG headroom, cannot cure OFF/power sequencing; cannot fake missing 33 keys or disable sampling/clipping flags. A software change would be a separate justified PR |
| 2 | Existing-footprint resistor revision, not blanket “larger R solves it” | Example 4.7 k would reduce assumed 1.4 mA to 0.298 mA, resistor power 0.417 mW, but 1 nF pole 33.9 kHz yields 10 kHz gain 0.959/−16.45° vs 0.998/−3.60° and slower recharge. ST table 48 allows 5.9 k external source at 7.5 cycles/14 MHz under stated conditions; actual 12 MHz/local 1 nF/multiplexed loading must be retested.10 k exceeds that conservative reference. Keep all affected channel phase/settling/load effects and redo OSL; does not stop rail injection outright |
| 3 | A reviewed load/shunt on existing 3V3/GND pads, or supply-sequencing adapter | Example 330 Ω load sinks 10 mA/dissipates 33 mW at 3.3 V: could absorb one saturated path's powered-rail estimate, not four unpowered paths and not a fixed-voltage clamp. A true rated shunt requires measured worst-case sum, threshold/tolerance, dissipation, regulator reverse behavior and startup proof. No permanent value/device is selected here |

Do not remove BAT54S, add an unidentified diode clamp, convert AFE to 3V3 or bypass RA as
an untested cure. Regulator/diode/op-amp substitutions require exact pinout/rating,
stability, leakage/capacitance and metrology review, not another PCB revision.

## Decoder/reference path expectations

U2: HC238 at 3V3; E1/E2 grounded, E3=PB8 enabled HIGH. U3 sinks active output and
pulls the BC807 base down through 4.7 k; BC807 emitter+5V_A then feeds gate via 100 Ω.
RGS 10 kΩ pulls gate to paired common source when off. Nominal selected gate is near 5V_A
minus transistor drops; at source~1.65 V effective VGS is nearer 3.3 V, **not 5 V**.
Unselected back-to-back body diodes block direct DC only when off; leakage/capacitance
and unpowered-IC behavior are not zero or proven isolation. Verify each state.

| RREF / decoder address A2A1A0 | U2 active pin / U3 sink pin | Paired MOSFETs |
| --- | --- | --- |
| 10 Ω /000 | .15 / .16 | QSEL1/2 |
| 100 Ω /001 | .14 / .15 | QSEL3/4 |
| 1 kΩ /010 | .13 / .14 | QSEL5/6 |
| 10 kΩ /011 | .12 / .13 | QSEL7A/B |
| 100 kΩ /100 | .11 / .12 | QSEL8A/B |
| 1 MΩ /101 | .10 / .11 | QSEL9A/B |

RANGE_EN LOW precedes **every** address change (code 2 ms deadtime/5 ms settle);
6/7 are unused, not extra ranges. K1 SAFE connects TEST_HI/.13→SAFE_HI/.11 and
TEST_LO/.4→SAFE_LO/.6; MEASURE connects .13→RET/.9 and .4→VMID/.8. A nominal 125 Ω, 5 V
coil is≈40 mA/200 mW plus driver losses; do not raise supply limit just to make it
operate. The NC 94 kΩ bleeder is an external DUT load, not intrinsic leakage. Actual
contacts/release/bounce and whole-board current are physical evidence.

## Hantek is the principal analog instrument, with stated limits

[Manufacturer DSO2C10 specifications](https://www.hantek.com/products/detail/17182):
100 MHz/2CH, maximum 1 GSa/s single or 500 MSa/s dual; native 8-bit (high-resolution
processing is not improved absolute gain accuracy); normal/average gain±3% on
10 mV/div..10 V/div **at BNC**, ±4% on 2/5 mV/div. A 10× probe makes 100 mV/div at tip
equal 10 mV/div at BNC. Timebase lists 25 ppm, also 50 ppm for intervals>1 ms; use the
separate single-shot Δt bound `±(1 sample interval+100ppm·reading+0.6ns)` rather than
claiming 25 ppm phase accuracy. Probe compensation, channel skew, crossing noise and
manual cursor selection also matter. Scope is 1 MΩ±2%||13 pF±3 pF at BNC; actual probe
loading is additional/different. At 1 kHz,1 µs timing uncertainty alone is 0.36°; at
10 kHz it is 3.6°. Relative techniques reduce some systematic error, not all.
Use independent DMM DC sanity without assuming its accuracy exceeds the scope.
No scope generator on C10; no external injection required.

## Affordable OSL and independent verification

Start functional testing with one 1 kΩ 1% resistor/SHORT strap/OPEN fixture. For full
PC OSL use six distinct labeled metal-film LOADs 10/100/1 k/10 k/100 k/1 MΩ,1/4W,1%
(0.1% optional, never mandatory). Retain printed interval `[0.99R,1.01R]`, actual
measured center only when accompanied by instrument/method/date/uncertainty, fixture,
temperature and frequency assumptions. Do not narrow tolerance from scope digits.
High-Z AC parasitics/contact/lead corrections are separate unresolved errors.

Minimal held-out kit: **3.3 kΩ 1%** (different from fit LOAD), **100 nF film 5%** and
**10 mH inductor with published tolerance/DCR/current rating** (10–20% acceptable
for functionality). At 1 kHz their ideal reactances are−1592 Ω/+62.8 Ω; capacitor is
first compared with 10 kΩ RREF, coil with 100 Ω RREF at 100 mV only after respective waveform
headroom checks. Nominal C/L are intervals and frequency-dependent models, not ESR/Q
standards; no precision LCR required. More resistor holdouts 33/330/33 k/330 kΩ and
10 nF C0G may be added later, not a first-power shopping blocker.

Hantek establishes source frequency/phase, gain overlap, clipping, transients and
repeatability. Passive LOADs anchor impedance scale; OPEN/SHORT alone do not. Full 33
valid triplets/99 accepted records are a PRODUCT storage requirement, **not proof
all 33 are physically observable**. Use existing campaign/solver/install workflows,
retain UNQUALIFIED and nominal ADC provenance; scope probes removed from DUT/RET.
Suggested first repeat check: five fixed matched LOAD captures in place, three
reattachments; record spread, do not turn it into maximum-error certification.
Electrical functionality, valid OSL correction, repeatability, interval agreement
on independent parts and absolute-accuracy qualification are separate conclusions.

## Blank manual worksheet — no results prefilled

Board/order/Gerber identity ____  Operator/date ____  Firmware SHA/profile ____
Module/regulator ____  Supply/current limit ____  Probe models/settings ____
DMM/model/specification ____  Ambient/NTC ____  Fitted/DNP/substitutions ____

| Observation | Actual reading / settings / uncertainty source | PASS / STOP / HOLD / NOT_TESTED; reason |
| --- | --- | --- |
| Unpowered rails, R05/header, K1NC/NO, R02 open, required links | ____ | ____ |
| +5V_SYS / +5V_A / +3V3 / idle current | ____ | ____ |
| Startup and decay order / overshoot / backfeed | ____ | ____ |
| VMID_RAW / VMID / OFF VEXC/RET1X/HG | ____ | ____ |
| Battery-sense fixture nodes / firmware freshness / charger/residual | ____ | ____ |
| PB9/PB8/PA8 reset/boot / W25QID / SAFE contact | ____ | ____ |
| 100 Hz /1 kHz /10 kHz source AC RMS/pp/f/phase/settings (separate rows in notes) | ____ | ____ |
| 450 kHz ripple / sample rate / noise floor (or unresolved) | ____ | ____ |
| Matched LOAD1X/HG AC ratio / same-run delay / clipping | ____ | ____ |
| Y04 output/ADC difference, fitted R, estimated I_R; **not diode current** / rail stability | ____ | ____ |
| Authorized OPEN key / evidence review / rejected reasons | ____ | ____ |
| First resistor ID/tolerance / R/X/path/provenance / repeat spread | ____ | ____ |
| Full OSL feasibility / installed sequence+CRC / independent holdouts | ____ | ____ |

Validation in this review: `python PCB/tools/audit_rev1.py --check`;
17/17 PCB tests; ladder polynomial versus independent 3-node elimination;
integer CCR/ZOH, HG/ADC pole and battery-divider equations checked numerically.
Manual commands/polarities/finite durations checked against
[console](../../../Firmware/src/app/app_bringup_console.c),
[PWM BSP](../../../Firmware/src/bsp/bsp_excitation.c),
[permissionFSM](../../../Firmware/src/hardware/hw_metrology_measure.c),
[battery policy](../../../Firmware/src/hardware/hw_power.h) and
[PLC1 hardware shell](../../../Firmware/src/app/app_cal_capture_shell.c).
No firmware defect or required firmware expansion was established. Cross-checks
cover 38 explicit probe-pad/net bindings, four clamp paths, six decoder channels,
RC/HG values and 20 local links. Whitespace passes; no full build matrix because
only documents changed. These
checks establish this bench reference, not physical readiness or accuracy.
