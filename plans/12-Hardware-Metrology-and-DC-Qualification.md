# 12 — Hardware/Metrology Requalification, DC and Capability Map

STATUS: IN_PROGRESS — A05 SOURCE/FABRICATION AUDIT DELIVERED; PHYSICAL INSPECTION, ASSEMBLER SIGN-OFF AND BENCH EVIDENCE REQUIRED
PRIORITY: P0/P1. REQUIRES_BENCH_VALIDATION is not a synonym for supported/accurate.

## A05 pre-assembly evidence

The [A05 audit](../docs/review/a05/README.md), starting from merged PR #12 main
`47bd1f264899ea10e948494e5a3e546e5b51558c`, decodes the real EasyEDA source and
reconciles 149 BOM references, 434 pads, 115 connected nets and fabrication copper/
drill pad locations. Thirty of the 33 requested MCU pins are carrier-connected;
PA13/14/PB2 remain module-only inspection items. The later export shifts RSCK by
0.127 mm with unchanged connectivity. There are 6 RED / 11 YELLOW / 7 GREEN /
3 UNKNOWN findings, a complete population matrix and an unexecuted bench procedure.
Confirmed topology recommendations are K2 DNP/R0_BANK populated and TVS/link DNP;
no live guard footprint exists. U4 bypass links must be populated. The TFT is 1×9,
its unbuffered LED connection is HOLD, UART documentation was reversed (corrected),
and the BOM buzzer's voltage/type is incompatible with the intended drive.

M12-01/M12-03 now have source/export evidence, not physical measurements or complete
native DRC. M12-02 purchased parts/power modules and all bench/qualification items
remain open. The owner identified only MCU/controller families and an unspecified
1S charger/boost/18650: module wiring/ratings and manufactured-board identity are
still required. Do not start power-on or consider A05 signed off until applicable
RED resolutions and the physical inspection gates are recorded.

## A. Electrical contract (from current repo docs and Rev.1 BOM; verify against EasyEDA and assembled PCB)
- +3V3/2 VMID buffered by U4A MCP6002; PA8 TIM1 PWM ~450 kHz -> three RC 5.1k/1nF stages -> U4B MCP6002 VEXC. DC-coupled path, not a precision DAC. Effective reconstruction and waveform distortion require scope data.
- VEXC -> selected reference -> RET -> two-wire DUT -> VMID with K1 NO measurement path. K1 de-energized routes terminals to SAFE. K2 baseline DNP; R0_BANK 0 ohm links low-Z bank; verify actual fitted state.
- 6 nominal RREF: 10/100/1k/10k/100k/1M ohm; 10/100/1k selected with back-to-back AO3400A, 10k/100k/1M with 2N7002; 74HC238 -> ULN2003 -> BC807 gate drivers. One-hot range changes; 10-ohm + 500 mVrms forbidden.
- U5A RET_1X, U5B RET_HG nominal closed-loop 1+68k/4.7k≈15.47, with frequency-dependent phase/gain. Two MCP6002-E/SN op-amps total; characterize low-frequency settling, high gain and 10 kHz distortion.
- PA0 VEXC, PA1 VMID, PA2 RET_1X, PA3 RET_HG: protection 1k/1nF/BAT54S; PA4/5 residual HI/LO, PA6 BAT, PA7 NTC. F1 dual-ADC synchronized 3 packed words/instant: VEXC1/RET1, VEXC2/RETHG, VMID/VMID; 256 sample instants.
- 100 Hz and 1 kHz 64 samples/cycle (6.4 and 64 ksps), 10 kHz 16 samples/cycle (160 ksps); sequential-rank/ADC skew must be physically verified. 12-bit ADC conversion/offset and load-dependent 3.3V variation are not metrology-grade merely because 12-bit codes exist.
- +5V SYS/A power, charger/boost interlock and PA15 CHG_VBUS; BAT/NTC measurement; no current-sense IC, external metrology ADC, Kelvin pair, high-voltage insulation stage or range-selectable resistor other than RREF.
- The external W25Q supplies resource/calibration data. Do not assume its contents can execute Cortex-M3 application code.

## B. Netlist/physical audit must precede qualification
M12-01. Compare PCB/source/ProPrj *.epro2, matching schematic PDF, fabrication BOM, PCB renders and as-built photographs. Build a pin-to-net-to-component-to-testpoint CSV for PA0..PA15, PB0..PB15, PC13, K1/K2, gate pair, range enable, VMID, VEXC, RET, REF, all ADC clamps and ground returns.
M12-02. Record fitted/DNP and actual alternative component datasheets, measured RREF values, NTC beta, probe/cable parasitics, power module behavior and 2-layer physical layout hotspots. BOM ordering and schematic values can differ; treat BOM as assembly intent, physical board as truth.
M12-03. Review MCU GPIO/power-off behavior, 74HC238 truth table/enable polarity, ULN2003 inversion, BC807 drive and MOSFET polarity; inspect for stray simultaneous range paths and unpowered leakage. Probe before enabling relay.
M12-04. Scope K1 operate/release bounce and range switching with controlled current limit. Verify safety interlock and charged DUT prohibition. No invasive measurement on live external voltages.

## C. Observable versus inferable quantities and initial PRODUCT claims
| Quantity | Rev.1 mathematical observability | Initial release recommendation | Major limitations |
| --- | --- | --- | --- |
| complex Z=R+jX, |Z|, phase at 100/1k/10k | core AC output once verified | weak difference denominator near OPEN, phase/rank skew, HF/parasitics |
| resistance at AC test frequency / R series | yes, conditional | qualify central Z/RREF zone first | two-wire contact/RON, leakage, large reactive part |
| capacitance series-equivalent Cs | if negative X is resolved and model passes | qualify selected bands | DC bias, dielectric loss, C frequency dependence |
| inductance series-equivalent Ls | if positive X is resolved | qualify selected bands | DCR dominance, saturation/DC bias, coil parasitic capacitance |
| capacitor series AC ESR at specified f | Re(Z) only in valid capacitive series region | expose with frequency and uncertainty; not standalone DC ESR | low ESR below two-wire/ADC error floor |
| inductor AC loss resistance / Q | Re(Z), |X|/R under admissible model | conditional; use n/a for near-zero/negative R | loss changes with frequency, inaccurate small R |
| capacitor dissipation D / Q | ESR/|X| and reciprocal | conditional; guard near-zero denominator | only equivalent AC at measured conditions |
| component type | inferred from sign and trends | qualitative with MIXED/UNKNOWN allowed | ambiguous close to SRF or strong losses |
| DC winding resistance DCR | DC path plausible, not qualified | BRINGUP research only initially | contact/offset, PWM 20.625 mV step, ADC resolution and current permission |
| low-voltage DC leakage resistance | possibly DC long settling at high RREF | exploratory only | MOSFET/op-amp/PCB leakage, humidity, dielectric absorption, required bias and dwell |
| transient RC charge/discharge / dielectric absorption | feasibility study only | not PRODUCT until repeatable waveform/reference proof | relay, source step resolution, analog filter, ADC sampling |
| dielectric insulation voltage/IR | unavailable | UNSUPPORTED | no HV generator, rated front-end or isolation |
| high-voltage DC/AC readings | unavailable | UNSUPPORTED | Rev.1 designed for de-energized passive DUTs |
| ESL/self-resonance frequency (numeric) | not identifiable generally from only 3 sparse AC points | do not publish | need broader validated sweep beyond hardware/software evidence |

M12-05. Create measurement_capability.json machine-readable identity with quantity, model, condition domain, qualification state, error-bound ID, fixture dependence and fallback state. Never activate entire 33-condition matrix just because a physical SHORT/LOAD completed.
M12-06. Establish first scope/measured SNR limits and qualify Z/RREF bands separately: central 0.2..5 first, extension 0.1..10 only when proven; beyond that show near-open/short/out of characterized range.
M12-07. Establish same-board repeatability with OPEN and SHORT recabling, channel HG/1x crossover, 10 ohm range current and distortion, high-Z input leakage. At 10 kHz even tens of pF materially perturb high-Z measurements; do not extrapolate a 1 MOhm 10 kHz domain without proof.
M12-08. Retest temperature ±as available and supply/charger status, changing one variable at a time; document NTC sensor board temperature is NOT DUT temperature.
M12-09. Verify the existing DSP vector convention, phase sign for R/C/L, sample skew/channel gain, nonlinear source amplitude, aliasing, and denominator/saturation guards; compare with independent complex reference implementation and scope traces. Retain OSL model v4 unless actual evidence justifies replacement.
M12-10. Publish measured data, not nominal accuracy: use stable 1%, optionally 0.1%, resistor controls and distinct R/C/L holdouts. Do not use unspecified capacitor ESR or inductor Q as tight fitting constraints.

## D. Guarded DC study (separate state machine, NOT normal AC DSP)
D12-01. Preserve existing BRINGUP 1 MOhm static PWM pilot: 450kHz carrier, CCR1 neutral 80, pilot 81, estimated source step 20.625 mV, 256 instants/64ksps, 2ms settling, observed source ceiling 50 mV. These are provisional code facts, not measured electrical guarantees. PRODUCT remains disabled.
D12-02. With current-limited isolated board supply and a known passive DUT, scope filtered VEXC - VMID after both positive and (if safely validated) negative duty excursions, residual ripple, settling and transient amplitude. Quantify step quantization, power-up offset and clipping. Do not energize K1 until permit rules and discharge are demonstrated.
D12-03. Independently read ADC code means/noise for VEXC/RET/VMID with source neutral, OPEN, SHORT, known 1 MOhm, and low-to-mid resistance references. Quantify common-mode, channel-specific zero, drift, effective number of usable codes, noise and guard compliance. Scope voltage accuracy alone is not sufficient for nA leakage.
D12-04. Produce hardware permission table for each contemplated RREF and ±bias: worst-case short current/power including RREF tolerance/MOSFET on resistance, amplifier supply/headroom, relay contact and DUT dissipation. AC triage cannot rule out DC inductor short. No lower RREF until separately bench authorized.
D12-05. Low-ohm DCR study requires two-wire SHORT/contact-offset measurement, remove/reseat repeatability, and optional polarity reversal for thermal EMF cancellation only after validation. Reject values at/below offset uncertainty rather than publish spurious 0.01Ω; identify approximate minimum resolvable R as experimentally supported.
D12-06. High-ohm leakage study requires fixture OPEN baseline, known high-ohm references, accurate applied bias/current, settling versus time, humidity/board cleanliness, and explicit low-voltage bias label. No 'capacitor datasheet insulation resistance' claim.
D12-07. DC reference algorithm: record measured VEXC, RET and VMID, calibrated VADC offsets, series RREF and known parasitics; solve Vsource-Vreturn across RREF and DUT voltage, with denominator/ADC saturation gates. Use bounded source step and signed current; reconstruct from actual observed voltage, never nominal CCR alone.
D12-08. Report a permission, observability and uncertainty budget to the owner. Only after sign-off may an opt-in PRODUCT_DC_V1 and a separate DC calibration record be designed. Integrate results safely with AC without pretending DC DCR==AC series ESR.

## E. Bench experiment design and acceptance
- Test all six ranges at 100 Hz / 1 kHz / 10 kHz / allowed amplitude classes for electrical support; qualification may be narrower than the 33 stored OSL keys.
- Per experiment: raw ADC codes, synchronized phasors, supply/NTC, VEXC/RET waveforms, reference standard identity/tolerance, scope probe loading, repeated captures (minimum 10 repeated in-place and 5 reattachments suggested), blank/open/short, declared measurement confidence.
- For each condition compute measured systematic bias, repeatability, max observed residual, drift, quantified reference uncertainty and conservative uncharacterized risk. No numerical 'max error' is guaranteed by ten repetitions.
- If invalid/unsupported: publish n/a, LOW_CONFIDENCE or UNQUALIFIED; never substitute nominal hardware formula with a visually authoritative precision figure.
- Hardware safety failure STOP: K1 stuck, range glitch, charger inhibit bypass, unexpected clamp conduction, externally sourced voltage on DUT, excessive op-amp dissipation or ADC rail clipping.

## Dependency and deliverables
After F11-01 baseline or in parallel: as-built netlist audit, bench worksheet and oscillograms, capability JSON, safe frequency/range matrix, documented true error-domain boundaries, signed DC go/no-go recommendation. Any change of excitation, ADC rate or calibration model invalidates affected OSL coefficients and requires migration/recalibration.

## 2026-10-09 owner constraints and pre-assembly review
- Board is **unassembled**. Make a pin/net/component and DNP/no-population decision **before soldering**; this is an inexpensive opportunity for a justified DNP component change, NOT for claiming board behavior. Baseline recommendation pending actual schematic: K2 DNP + R0_BANK populated, D_TVS + R_TVS_LINK DNP, experimental guard DNP; do not populate both incompatible K2 and hard link blindly. A switch in configuration requires circuit-level feasibility, fixture/safety and metrology review. No physical test has occurred.
- Confirm correct board revision and BOM before ordering components. The documented TFT pin-count discrepancy (Rev1 BOM J_TFT 1x9 versus docs J_TFT 2x5) must be resolved against the actual PCB/epro2, rather than assuming firmware pinout documents physically match.
- Available Hantek DSO2C10 has two ground-referenced channels, nominal 8-bit vertical ADC, vertical gain ±3% for regular voltage scales, scope timebase ±25ppm; supports CSV/SCPI, no included AWG in C10. It can qualify phase/timebase/PWM filter/ripple/gain shapes but is not a sub-1% voltage or nA leakage standard by default. Isolating oscilloscope mains supply is NOT a safe replacement for differential probe isolation or a reason to float protective earth. Treat all probe ground clips as common GND. Use independent DMM/reference accuracy for scale claims.
- New owner-valued features, scoped by plan 16: DC winding resistance, capacitor natural self-discharge/leakage, capacitor charge/discharge time under a **known** load, inductor RL current/field decay. Carefully distinguish intrinsic capacitor leakage from the 94kΩ SAFE bleed and 1MΩ reference/input loading. The 94kΩ bleeder is a deterministic external load in SAFE, not the capacitor's intrinsic leakage; a time constant measured in SAFE is about the instrument circuit.
- First measurements are AC. No new DC path enabled before physical source/ADC/offset/currents tests and pre-assembly electrical topology inspection; optional transient features must not compel a hardware revision or delay AC release without explicit owner go/no-go.
