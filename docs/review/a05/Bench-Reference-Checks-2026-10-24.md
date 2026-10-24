# Post-rework resistor checks, 2026-10-24

Status: IN_PROGRESS; REQUIRES_BENCH_VALIDATION. These diagnostic captures do not
qualify accuracy, all ranges or battery operation. No calibration record is made.
Scope: physical bring-up09A and plan12 M12-03/M12-06/M12-09. The owner requested
maximum practical checks with available passive resistors before battery fitting.

## Fixed setup and evidence

Actual board UID54ff70068370525760380567, firmware480aa310f557, BRINGUP external
bench variant, calibration MISSING/IDEAL fallback, all six RGS reworked to GND as
described in [the recovery report](RGS-Ground-Pulldown-Rework-Candidate.md).
Owner reports external5V_A/COUT1=5V, 3V3=3.3V. Battery input floating; no ADC
substitution. Display absent; COM3 at115200, ST-Link supply/TTL VCC disconnected.
Numeric current-limit, DMM model/uncertainty, pictures and scope timing remain
unprovided. Do not manufacture historical B01 acceptance evidence.

Every DUT transaction uses real fresh residual/VMID/charger/fault/clock admission,
single-use permit issued/validated in the same attempt, nominal100mVrms,
K1operate30ms/release8ms and normal cleanup. UART statuses are commanded states,
not relay-contact/one-hot decoder feedback. All samples remain unchanged.
Local raw/report/transcript evidence is under
`Firmware/build/physical-20261024-adc-noop-stop/`.

The owner listed available references329.5k,99.7k,33.0k,996k,1.5R,2.2M.
Do not treat the significant digits as an uncertainty specification. Nominal
versus DMM indication was not separately itemized; exact resistance is not known.
Fixture changes occur only after SAFE/range-off cleanup and an explicit current
owner confirmation of which isolated passive resistor is attached.

## Completed 10K and 100K branch checks

| Owner reference | Selected RREF | Frequency | Attempts | Firmware mean R, uncalibrated | Firmware mean X, uncalibrated |
| --- | --- | --- | --- | --- | --- |
| 10000 Ohm indication | 10K | 1kHz | 3 | 10316.827 Ohm | See recovery report/RAW |
| 99700 Ohm reported | 100K | 1kHz | 3 | 91865.549 Ohm | -31490.926 Ohm |
| Same99700 Ohm | 100K | 10Hz | 3 | 103605.677 Ohm | -155.422 Ohm |
| 996000 Ohm reported | 1M | 10Hz | 3 | 1030542.016 Ohm | -45073.260 Ohm |
| Same996000 Ohm | 1M | 1kHz | 3 | 97252.072 Ohm | -270486.816 Ohm |
| 329500 Ohm reported | 1M | 10Hz | 3 | 332881.120 Ohm | -3736.308 Ohm |
| Same329500 Ohm | 100K | 10Hz | 3 | 317737.312 Ohm | -732.598 Ohm |
| 33000 Ohm reported | 100K | 10Hz | 3 | 33164.980 Ohm | -11.907 Ohm |
| Same33000 Ohm | 10K | 10Hz | 3 | 31752.811 Ohm | -74.897 Ohm |

The 100K-branch captures all completed256rows/768words, normal first ADC2 sequence,
DMA error0/remaining0, no raw samples at/beyond clip margins16/4079, DSP OK with
HG selected. The1kHz indications were classified CAPACITIVE; the10Hz indications
were RESISTIVE. This is an observed frequency-dependent response of the full
uncalibrated signal path/fixture, not proof of a defective resistor or identified
parasitic component. Do not equate magnitude with resistance or claim that OSL
will necessarily remove every effect. Characterize channel gain/phase, loading,
leakage, fixture and reference response before product claims.

The100K in-place R spreads were147.864Ohm peak-to-peak at1kHz and699.704Ohm at10Hz.
The10Hz mean is about3.92% above the owner99700Ohm value. No reattachments or
temperature sweep were tested; these are not uncertainty bounds or repeatability
qualification. See `99k7-frequency-comparison.json` for every unmodified record.

After the last capture, K1SAFE/rangeDISABLED/excitationOFF/faults0 and source
BATTERY_REQUIRED were verified; COM3 closed. No new image or calibration installed.

The owner then confirmed the996K fixture swap. All six1M-branch captures also
completed256rows, normal first ADC2 sequence, DMAerror0/remaining0, DSP OK, HG
selected and no raw channel at/beyond clip margins. The10Hz runs were RESISTIVE;
the1kHz runs were CAPACITIVE. The10Hz R spread is11568.192Ohm peak-to-peak and
meanR is about3.47% above the owner996000Ohm indication. At1kHz the real component
must not be presented as a DC resistance: the large negative X is material.
See `996k-frequency-comparison.json` and unchanged captures for full evidence.
The large frequency dependence is a characterization issue; no single scale
correction or general high-Z/1kHz accuracy claim is justified by these tests.

The owner confirmed the329.5K fixture swap. Three1M/10Hz/100mV captures completed
with HG selected and all channels inside clip margins; R spread1629.920Ohm p-p.
The same fixture was then measured three times on100K/10Hz/100mV without recabling.
HG hit the low clip margin in20/23/26 samples respectively and was rejected by
the firmware, which selected unclipped RET_1X and returned DSP OK/RESISTIVE.
Common VEXC/VMID columns and selected1X had no clip-margin hits. All256 rows,
including clipped HG, are preserved; independent nominal HG calculations in the
original runner reports are diagnostic-only and unusable for those clipped
captures. `329k5-range-comparison.json` explicitly marks that invalid HG path.

The100K/1X R spread is2951.776Ohm p-p. The two selected-channel means differ by
15143.808Ohm (~4.65% of their average); versus owner329500Ohm, they differ by
about+1.03% and-3.57%. This demonstrates acquisition, range response and channel
fallback, not calibrated agreement. Channel/range equalization, nonlinear or
fixture effects and observed variation still need characterization. No records
were corrected to match the reference. Every attempt ended SAFE/off/no faults,
source revoked and COM3 closed.

The owner confirmed the33K swap ("feito o res de 33k"). Three100K/10Hz/100mV
captures selected HG, all columns inside clip margins, meanR33164.980Ohm and
R spread75.160Ohm p-p. Three10K overlap captures on the same fixture selected
RET_1X, with unused HG low-margin hits27/23/25 and no common/selected-channel
clip-margin hits. MeanR31752.811Ohm, R spread221.874Ohm p-p. The selected-channel
means differ1412.169Ohm, about4.35% of their average; their differences from the
owner33000Ohm indication are about+0.50% and-3.78%. This reproduces the direction
of the earlier overlap discrepancy, but does not isolate its cause to either
channel or RREF: both change in this comparison. No scale correction was fitted.
Every capture preserved256 rows, completed768 DMA words with error0/remaining0,
returned DSP OK/RESISTIVE and ended SAFE/off/fault0/source revoked/COM3 closed.
See `33k-range-comparison.json`; clipped HG reference calculations remain unusable.

## Diagnostic software limitations found during reactive comparisons

In this deployed nonfactory BRINGUP image, do not trust the printed phase angle
for |X|>R. The range-reduced atan2 implementation is gated by
WTK_PRODUCT_FACTORY_PROVISIONED in measurement_dsp.c; the active legacy branch
evaluates its approximation outside its intended domain. For example the first
996K/1M/1kHz result printed phase_mrad=-116 while its R=98819.832Ohm and
X=-270995.776Ohm imply about-1.22rad using host atan2. The raw complex calculation
agrees with the independent PC reference; this does not qualify its physical
phase accuracy. Phase-field repair/testing is a separate outstanding software
item, not a soldering diagnosis. No phase correction was hidden in these records.

The existing console milli_from_float() also converts scaled float to int32_t
without a representability guard. A2.2MOhm indication exceeds the positive
signed32-bit milliohm domain (~2147483.647Ohm). Do not report those formatted
resistance fields as a valid2.2M observation. Any later2.2M diagnostic requires
independent calculation from preserved RAW and explicit quality/channel checks,
or a separately reviewed protocol/format fix. No such capture is marked done.

## Next checks and limits

The33.0k overlap comparison is complete. The owner offered assorted capacitors
and inductors. A pending fixture request asks for an isolated, discharged,
nonpolar capacitor near22nF (10..47nF accepted after exact value/type confirmation),
then requests the available inductor nearest10mH without connecting it yet.
No capacitor or inductor capture is recorded. The last confirmed fixture remains
33K until the owner explicitly confirms replacement. Tentative capacitor condition
is1kHz/100mV/RREF10K; an ideal22nF has X about-7234Ohm at1kHz, within the central
impedance-ratio band. This is a test-selection calculation, not measured data.
Check actual value, selected-channel clipping and raw complex response before
interpreting equivalent capacitance. Use independent host atan2 for phase because
of the deployed diagnostic limitation above. No tight ESR/Q acceptance constraint
is justified by an unspecified capacitor or coil. Inductor condition/permission
needs review after the actual part is identified; dynamic transient qualification
remains open. Do not silently reuse resistor fixture metadata for reactive DUTs.

The996K and329.5K comparisons are complete; high-Z frequency response must not be
extrapolated from the10K resistor result. The2.2M reference remains untested and
requires the raw-analysis/formatter precautions above.
The1.5Ohm reference on10R has ratio0.15 outside that initial central band and
requires source-current/headroom and two-wire contact/SHORT characterization;
do not silently treat it as a precision low-ohm acceptance standard.

Dynamic gate discharge, break-before-make, reset/power collapse, inductive
transients, all untested ranges and calibrated accuracy remain open.

## Narrow check of owner-supplied October24 exports

The owner supplied replacement October24 epro2/BOM/Gerber files while tests ran;
their uncommitted changes were preserved. Read-only extraction confirmed all six
RGS pad1=GND and pad2=the respective GATE net in both PCB source and FlyingProbe
export. RREF1..6 values match source/BOM:10/100/1k/10k/100k/1M. Evidence:
`october-export-rgs-rref.json`, with exact new-file hashes.
This is not a full schematic/routed-copper/native-DRC/manufacturing approval and
does not prove that the already-built board is identical to the new exports.
The original A05 scripts still name the historical August paths; updating that
audit baseline is separate from these physical checks. User files were not edited,
restored or staged as part of the tests.
