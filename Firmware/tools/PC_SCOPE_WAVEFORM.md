# A04.3a — offline oscilloscope evidence

The workbench's **Oscilloscope** tab processes imported recordings only. It does
not connect to the scope, send serial commands, excite the board or approve bench
operations. The PCB is unassembled: **REQUIRES_BENCH_VALIDATION**. Use the existing
[Windows setup](PC_OSL_WORKBENCH.md). Standard-library analysis and QtGui/QPainter
plots require no new dependency, QtCharts, QML or licensing module.

## Formats and scaling

**Generic UTF-8 CSV** supports one/two voltage channels, numeric time, scientific
notation, optional header and explicitly skipped opaque preamble. Column indices
are zero based. Separators: comma, semicolon or tab; decimal: dot or comma.
Separator and decimal must differ. No locale guessing or thousands separators.
Empty/incomplete rows, nonfinite numbers and changing column counts are errors.
Quoted single-line fields work; multiline fields and binary LWF are unsupported.

Declare time units (`s/ms/us/ns`), channel units (`V/mV/uV`), net, scale, tip-voltage
offset, probe attenuation, coupling and whether CSV values are **probe-tip volts**
or **scope-connector volts**. Tip values are not multiplied by attenuation again;
connector values are multiplied once. Unknown ADC-code conversion is unsupported.
Scale is never inferred from headers; unit annotations must agree with mapping.
ADC/count labels are refused. Do not relabel codes as volts to bypass this check.

**Hantek DSO2C10 native CSV remains unsupported pending a verified actual sample.**
The distinct profile rejects with instructions. The [official Hantek page](https://www.hantek.com/products/detail/17182)
confirms CSV saving/two channels, but establishes no byte-level CSV/scaling contract.
No proprietary fields, metadata scales or SCPI commands have been invented.
Manually verified numeric columns can use generic mapping; that is not a tested
native adapter. Supply an **unchanged USB CSV**, model/firmware identity, screenshots
of timebase/vertical/trigger/acquisition settings, probe attenuation/compensation,
coupling, channel enablement and whether volts include probe scaling. Include known
waveform/independent time-voltage observations and representative two-channel,
preamble/locale/long-record files. Preserve originals and hashes.

## Desktop operation

1. Select a CSV and **generic**; enter mapping/settings or load a saved config.
   Forms are editable, with no manual JSON editing. Loading never starts a capture.
2. Identify instrument/serial, operator/date/timezone, probes/loading, bandwidth,
   trigger/acquisition, supply state/current limit, adapters and fitted population.
   Blanks remain UNKNOWN. Missing physical provenance is INSUFFICIENT_EVIDENCE.
   Declare SYNTHETIC for generated data; confirm simultaneity only for one shared record.
3. Choose a preset, enter parameters, **Import & analyze**. Failure clears stale data.
   Inspect traces/metrics and **Zoom interval** in seconds. The worker produces a new
   original-sample min/max envelope; **Full trace** restores the record.
4. **Compare separate CSV** uses the currently declared mapping/probe settings for
   another recording: amplitude-only comparison, no synchronized phase or subtraction.
   Its source/hash/settings are preserved and copied on export.
5. **Export evidence** chooses a NEW directory. Optional B01 association creates a
   sidecar without changing any check/history. Independent electrical review remains
   required. No automatic PASS, qualification or hardware command exists.

Import/analysis/zoom/export use a separate worker without serial-client access.
Only one operation is outstanding. Closing cancels the parser and drains both
workers; long analysis/copy can delay completion without freezing widgets or
forcibly terminating threads.

## Numerical meaning

All samples contribute min/max/peak-to-peak/mean/RMS/AC RMS (Welford variance).
AC/unknown coupling invalidates DC interpretation. Times must be finite and strictly
increasing. Nonuniform descriptive statistics are **sample weighted**, not time-weighted.
Declared interval must agree with timestamps within the stated 0.1% consistency policy.

Frequency uses stable mean-level rising crossings and a least-squares crossing-time
slope, **not a universal dominant-frequency FFT**. Period spread >5% rejects this
estimate. A requested-frequency fit states when frequency was not independently
measured. Disagreement >5% requires investigation. DC-only traces have no frequency.

Fit `y=dc+a*cos(2*pi*f*(t-t0))+b*sin(2*pi*f*(t-t0))` by the full 3×3 least-squares
system. Peak=`hypot(a,b)`; phase=`atan2(-b,a)`, matching firmware cosine/negative-sine
DFT. The origin is the first timestamp, not an external source clock.
Use at most **32,768 contiguous original samples**, with no decimation/invented filter.
Report count/duration/origin. Require four cycles, >=16 samples/cycle, <=0.1% interval
spread and known effective probe/channel bandwidth reaching the frequency. These are
conservative observability policies, **not hardware thresholds or accuracy bounds**.
Aliasing can still exist without independently established acquisition filtering.
Second/third harmonic projections require ten cycles, >=16 samples at the third
harmonic and adequate bandwidth. THD_2_3 includes finite-window leakage/noise and
is **not total THD**. Residual RMS includes noise AND distortion, not a noise floor.

Clipping status uses only entered display/input limits. Limit contact is not proof
of op-amp clipping. Repeated extrema/plateaus can also be quantization; no limits
means UNKNOWN. No nominal scope accuracy is silently applied.

## Presets

**A — supply:** CH1 +5V_A, CH2 +3V3, both DC/GND. First/last 5% define observed
start/end levels; interpolated 10–90% times give rise/fall/relative start/end,
repeated crossings, overshoot/undershoot and sample resolution. They do not prove
the full transition was recorded; review pre/posttrigger coverage. Fewer than four
intervals across a transition is flagged. Existing A05 engineering windows are
referenced: 5V_A 4.75–5.25 V, 3V3 3.0–3.6 V. Upper excursions prompt investigation;
low ramp values are not automatically settled-rail failures. No permitted delay is
invented. Regulator-inactive observations need an in-capture interval and independent
state evidence. Apparent backfeed still needs regulator/adapters/current-limit/DMM review.

**B — excitation:** CH1 VEXC, CH2 VMID, DC/GND; CH1−CH2 at expected 100/1k/10k Hz.
Report mean/RMS/peak/frequency/phase and available harmonic evidence. 450 kHz targeted
projection needs uniform time, both bandwidths >=450 kHz, >=16 samples/carrier cycle
and four cycles in the fit segment; otherwise INSUFFICIENT_EVIDENCE. A finite
projection alone does not prove carrier presence: leakage/noise/resolution matter.
Two channels cannot simultaneously record a third RET signal; use separate runs.

**C — clamp:** selected MCP6002 output/ADC-side net, DC/GND. Enter the **fitted**
resistor and component/pad mapping evidence. A05 maps VEXC→RADC1/ADC_VEXC,
VMID→RADC2/ADC_VMID, RET_1X→RADC3/ADC_RET_1X, RET_HG→RADC4/ADC_RET_HG;
source/BOM is nominal 1 kΩ. Verify the actual physical pair.
`I_R=(VOUT−VADC)/R` gives estimated signed peak/RMS and left-sample interval occupancy.
Independent 3V3 value/provenance enables nominal GND..measured-3V3 ADC-window counts
and margins. That rail was not simultaneously sampled; transient motion is unresolved.
This is not an absolute-maximum/injection allowance.
With documented **tip-voltage absolute error bounds** e1/e2 and resistor relative
tolerance r<1, peak error estimate=`(e1+e2+max(abs(VOUT−VADC))*r)/(R*(1−r))`.
Otherwise unavailable. This is not expanded uncertainty or a physical certificate.
Current may include capacitive/ADC loading. **Possible BAT54S conduction and actual
+3V3 injection are different, unmeasured quantities**; both remain unavailable.

**D — RET:** simultaneous RET_1X/RET_HG to GND. Report fundamental amplitude,
shared-frequency gain/phase, plateau/limit contact and residual/harmonic evidence.
Declared limit contact rejects ratio/phase. Nominal `1+68k/4.7k≈15.47` is context,
not an acceptance target. Check loading/frequency/coupling/channel skew/scope
accuracy/operating point before interpreting deviations. Unknown limits leave usable
dynamic range UNKNOWN. Independent-acquisition comparison never synchronizes phase.

## Bounds and provenance

Maximum: 512 MiB source, 4,000,000 rows, 64 columns, 4096-byte lines, 1024 skipped
rows, 16 KiB settings. Excess captures are rejected, never silently truncated.
Array('d') uses 24 bytes/sample for time+two channels: 96 MB nominal at 4M, plus
capacity/parser/runtime overhead. One derived array adds 32 MB. Comparing two
distinct records temporarily holds 192 MB nominal. Fit scratch is bounded by 32,768
samples. Plots receive <=4096 points/trace; zoom preserves peaks/endpoints in order.
Use a reviewed shorter record/region if the PC cannot accommodate these bounds.

Bundles: unaltered source.csv, report.json, report.md, waveform.svg, optional
independent-source.csv and a **manifest written last** hashing every file.
Missing/invalid manifest means incomplete evidence; `verify_bundle()` checks it.
Hashes prove byte continuity, not authenticity or OS immutability. Changed sources
are refused on export; output directories are never overwritten.

States: SYNTHETIC; REAL_FILE_UNREVIEWED; MANUALLY_REVIEWED (separate named/date/
rationale/evidence decision); INSUFFICIENT_EVIDENCE. Synthetic cannot become a
physical review. None is physically qualified. Even reviewed electrical acceptance
remains NOT_TESTED. JSON retains settings/net mapping/source SHA/parameters/limitations.

B01 sidecar keys: powerup_waveforms, powerdown_waveforms, signal_backfeed,
analog_adc_voltages, 3v3_stability, bat54s_conduction. Source hash/target key are
retained; no check changes. Later conduction work needs separate authorization.
Existing DMM checks still require DMM evidence. Attach report paths/hash manually,
then record an independent decision using the existing B01 workflow. Synthetic
reports cannot satisfy the desktop physical gate.

## Reproduce today

```powershell
python Firmware/tools/scope_synthetic.py --out Firmware/build/scope-demo --kind powerup
python Firmware/tools/scope_waveform.py Firmware/build/scope-demo/waveform.csv --config Firmware/build/scope-demo/config.json --preset supply --parameters Firmware/build/scope-demo/parameters.json --out Firmware/build/scope-evidence
```

Generator kinds also include powerdown/simultaneous/overshoot/excitation/carrier/
ret/saturated/clamp/lower_clamp/noise; excitation accepts 100/1k/10k Hz. These are
**analytic synthetic signals, not Hantek exports or PCB recordings**. In the GUI load
config.json, choose its preset and import; paired parameters.json loads if present.

```powershell
$env:QT_QPA_PLATFORM='offscreen'
Firmware/build/workbench-env/Scripts/python.exe -m unittest discover -s Firmware/tests/tools -p 'test_scope*.py' -v
```

Existing pinned dependencies/package_workbench.py produce a Windows directory
bundle including these tools/guide. Keep generated CSV/evidence/executables outside Git.

## First bench runs, after A05 gates

R02: R_TFT_LED open until characterized. R05: inspect Bluepill alignment/power/
VBAT/SWD/HSE/PA11/12. Confirm bypass/bank population and K2/TVS DNP. Begin with
reviewed current-limited +5V_A/+3V3 power-up/down, sufficient pre/posttrigger coverage
and independent DMM readings. Then VMID/DC stability, authorized VEXC/VMID passive
capture, separately authorized output/ADC and RET pairs at checked operating points.
Do not intentionally create unsafe power/clamp states to obtain a trace.

**Both ground clips only to verified circuit GND. Never VMID, RET, DUT TEST_LO,
amplifier output or another signal node. Never float protective earth. No mains,
energized DUT, charged calibration capacitor or external high-voltage injection.**
Two channels cannot capture VEXC/VMID/RET simultaneously. Keep separate runs distinct;
high-Z nodes need loading checks. No physical safety/accuracy approval is claimed.
