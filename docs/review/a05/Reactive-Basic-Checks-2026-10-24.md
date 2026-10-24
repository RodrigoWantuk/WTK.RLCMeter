# Basic reactive checks, 2026-10-24

Status: IN_PROGRESS; REQUIRES_BENCH_VALIDATION. No final-assembly go-ahead or
calibration has been issued. Scope: physical09A, plan12 M12-09, and a bounded
Phase06 phase-helper regression required by actual reactive captures.

## Actual10nF capacitor observations

The owner connected a capacitor marked103J100, inferred nominal10nF. Manufacturer,
dielectric and independently measured capacitance are unknown. J normally denotes
5% marking tolerance; no certificate or tolerance verification is supplied. The
[Panasonic film catalog](https://industrial.panasonic.com/content/data/CP/PDF/S_Film_cap_catalog_e.pdf)
illustrates the103/0.01uF convention; this does not identify the owner's part.

Three actual DUT captures used deployed480aa310f557, RREF10K,1kHz, nominal100mVrms,
external bench source and IDEAL_UNQUALIFIED fallback. Each retained256rows,
completed768words DMA/error0/remaining0, selected RET_1X and returned DSP OK with
CAPACITIVE classification. Common VEXC/VMID and selected1X had no clip-margin hits.
Unused HG had61/71/63 clipped-margin samples; its calculated Z must not be used.
Every attempt returned commanded SAFE/range disabled/excitation off/fault0,
source revoked and COM3 closed. UART status is not physical contact feedback.

Independent PC complex math on the selected path gave:

| Attempt | X, uncalibrated Ohm | Host series-equivalent C | Host phase |
| --- | --- | --- | --- |
|001| -15073.290 |10.558740nF| -1.566492rad|
|002| -15046.389 |10.577617nF| -1.559135rad|
|003| -15048.989 |10.575790nF| -1.567352rad|

MeanC10.570715nF, in-place spread0.018877nF p-p. These demonstrate a plausible
capacitive response, not capacitor tolerance, ESR, phase accuracy, repeatability
qualification or final instrument accuracy. Do not classify the capacitor as
out of tolerance from this uncalibrated estimate. Source RAW/transcripts/reports
and `cap-10nf-basic-summary.json` are in the local
`Firmware/build/physical-20261024-adc-noop-stop/` evidence directory.

## Phase helper correction, software only so far

The deployed nonfactory helper evaluated the atan polynomial outside |z|<=1.
These10nF captures produced nonsensical printed phase_mrad while R/X agreed with
the independent PC reconstruction. The phase field was not used for C above.
The previously factory-only range-reduced implementation is now shared by every
profile; no factory flag, acquisition setting, R/X equation or safety gate changed.
Expected files: measurement_dsp.c, its host test, and this evidence/status record.

A regression first failed on the original implementation. It now covers all
quadrants/axes, dominant R/X ratios, actual captured R/X as mathematical vectors,
derived phase and nonfinite inputs. It compares against host atan2 with0.002rad
tolerance. All41 MSVC Debug CTests pass. Strict ARM build/size/profile checks pass:
bench64604B Flash/14868B accountedRAM, defaultBRINGUP65368B/14788B,
PRODUCT62160B/15168B. Existing soft-size/LTO advisories remain; hard gates pass.
This is a software numerical check, not physical phase accuracy qualification.
Correction is NOT DEPLOYED yet; actual board firmware remains480aa310f557.

## Polarized blind fixture: hold, no active capture

The owner replaced the10nF piece with a blind capacitor, subsequently identifying
it as electrolytic,25V, negative terminal on TEST_HI. Capacitance, manufacturer,
series and charge history remain unknown; removal has not been confirmed. This
supersedes the10nF fixture: do not reuse the10nF runner or confirmation for it.
No range, excitation, relay, reset or programmer operation was performed after
that replacement. Read-only `polarized-hold-readonly-001` confirms K1SAFE,
range disabled, faults0, charger absent, residualSAFE with measured differential
0mV/age7ms, valid VMID. COM3 closed. This is not a polarized-capacitor measurement.

The owner explicitly requires polarized-capacitor support to be considered before
assembly release. The initial restriction must not be described as proof that
electrolytics can never be measured or that Rev1 necessarily needs replacement.
Choose a test method for the actual capacitor technology and electrical envelope.

[Chemi-Con](https://www.chemi-con.co.jp/en/faq/detail.php?id=alElectricalCh)
describes aluminum electrolytic capacitance measurement using about0.5V AC at120Hz.
This is not permission for arbitrary amplitude, waveform, reverse stress or all
polarized technologies. [NI's measurement guide](https://www.ni.com/en/support/documentation/supplemental/06/capacitance-and-inductance-measurements-with-pxi-dmms.html)
explains DC bias as a way to prevent reverse polarization and uses negative-to-LO
with a positive HI bias. That instrument's bias specification is not Rev1's.

Rev1 MEASURE maps TEST_HI to RET and TEST_LO to VMID. Positive differential bias
would therefore require +HI/-LO, conditional on a validated positive-bias method.
The current AC LUT is centered on fixed neutral; it has no exposed AC-plus-DC-bias
parameter. The separate optional DC pilot is excluded from the deployed bench
variant and is not a ready biased-AC capacitor mode. A25V rating is not a reverse
voltage allowance. Do not energize this reversed fixture by guessing.

Before enabling a polarized method, review permitted DUT AC/DC extrema, actual
technology/datasheet conditions, RREF/current/headroom, charge/settling time,
ADC/unusedHG clamp behavior and normal/abort/reset/power teardown. Normal FSM
returns neutral before relay release; abort requests range isolation and SAFE
before excitation OFF. These code facts do not prove physical switching timing.
Preserve those isolation assumptions. A simple PWM offset is not sufficient
evidence: a differential DC shift also changes the DC level at HG and could
saturate that path. Source/return waveforms and rail/clamp effects need review.

No biased-AC implementation, external DUT bias injection, charged-DUT permission
or hardware modification was performed. Existing plan12/16 feasibility/current
gates remain applicable. Whether the selected polarized domain needs only firmware
or hardware changes is unresolved; do not promise either outcome without evidence.

## Subsequent phase deployment and inductor checks

This section supersedes the earlier NOT DEPLOYED and polarized-fixture hold state.
The owner replaced the electrolytic with an isolated blind inductor and authorized
continuing. A clean managed checkout at12f03bb530f5 built the sealed BRINGUP bench
image64604B, SHA2567fcf9b10388a3f442663fac0c2bd77163e4de46078bd742b62134bba53bd9355.
The complete previous Flash matched its archived readback before programming;
new bytes and full64KiB readback were verified, unaffected pages preserved and
externalW25Q untouched. Controlled boot reported12f03bb530f5, SAFE_BOOT,
HSEPLL72MHz/ADC12MHz, no faults. Evidence moved to
`Firmware/build/physical-20261024-reactive-phase/` (sealed-build/image,
previous-firmware-backup,deployment-001,first-boot-passive-001).

The blind fixture traversed1M/100K/10K/1K at1kHz, then1K at10kHz. Their very small
Z/RREF ratios were unsuitable for a credible value despite DSP OK. A bounded100R
extension at10kHz/100mV was reviewed, excluding10R. Nominal steady peak-current
bound1.414mA; observed source about72mVpeak suggests about0.72mA for a near-short.
These are estimates, not current-sensor or distortion qualification. Unvalidated
physical isolation could still permit a larger OFF transient; existing normal
neutral/release and abort isolation ordering were retained. The Microchip
[MCP6002 datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP6001-1R-1U-2-4-1-MHz-Low-Power-Op-Amp-DS20001733L.pdf)
short-circuit specification is not a guaranteed linear-output current allowance.

Three100R/10kHz blind indications were1.093164,1.101097,0.979683uH; mean1.057982uH,
X0.066475Ohm andR0.746278Ohm. All ratios remained below0.008, outside the initial
central0.2..5 band, and firmware classified these three as RESISTIVE. The owner
then revealed2.2uH nominal, with tolerance/independently measured L unknown. The
indication was51.91% below that nominal. This is LOW_CONFIDENCE, not small-L
qualification or a successful precision identification. Uncalibrated HG phase,
fixture/two-wire parasitics and noise need characterization; no cause is isolated
and no calibration-only cure is promised. See `inductor-blind-summary.json`.

The owner next confirmed470uH, microchoke. Three100R/10kHz/100mV captures selected
HG, returned DSP OK/INDUCTIVE, and gave425.961,425.574,429.423uH using independent
PC complex math. Mean426.986uH, in-place spread3.849uH p-p,9.15% below nominal.
MeanR11.678Ohm/X26.828Ohm are AC series components, not DC winding resistance.
All six raw columns were unclipped; |Z|/RREF0.292 meets the central ratio target.
Each retained256rows/DMA768words complete/error0/remaining0 and ended commanded
SAFE/range-off/excitation-off/fault0/source revoked/COM3 closed. Firmware phase
agreed numerically with host atan2 within0.002rad, demonstrating the source fix's
physical-data numerical regression, not calibrated hardware phase accuracy.
This supports basic inductive response in this condition, not full-range accuracy.
See `inductor-470uh-summary.json` and unchanged raw/transcripts/reports.

The owner canceled a third-inductor test and requested display/buttons next,
disconnecting UART because of physical connector interference. Owner reports
backlight correct but screen white. No model/backlight-current evidence was
provided; retain that observation as owner-reported. Deployed BRINGUP initializes
ILI9341 but draws only the missing-Flash error, so with detectedW25Q there is no
ordinary visible test pattern. White alone does not prove SPI/controller function.
Owner now requests automatic colors, setup/firmware identity, pressed/released
state of all three buttons and distinct buzzer tones. Prepare a separate opt-in
BRINGUP peripheral diagnostic, with active measurement/calibration commands absent;
no calibration or resource provisioning is part of this test. Last confirmed DUT
is470uH; third swap was not confirmed. No final-assembly go-ahead issued.
