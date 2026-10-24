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

## Next checks and limits

The next intended fixture is the owner996k resistor, RREF1M, nominal100mV,10Hz.
Its runner requires the current physical swap confirmation before any command.
1kHz comparison may follow only after reviewing the first capture; high-Z
frequency response must not be extrapolated from the10K resistor result.

329.5k and2.2M are useful potential loads for1M, and33.0k for10K/100K overlap,
within the initial central0.2..5 ratio goals. Each needs a separate identified
fixture, fixed condition and actual result; none is marked tested here.
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
