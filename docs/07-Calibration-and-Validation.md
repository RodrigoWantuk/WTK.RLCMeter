# Calibration and Validation

## First-unit experimental NTC profile

The optional [factory profile](../Firmware/tools/NTC_FACTORY_PROFILE.md) fits
owner-supplied temperature/raw-ADC pairs to an effective divider ratio and Beta,
then emits a fixed-size MCU lookup table. It is UID-bound, versioned and explicitly
unqualified. The first unit used 23.5 degC/ADC1952 and 30.1 degC/ADC1856 at the
owner's explicit request despite contact uncertainty. These fitted parameters
are not verified intrinsic component values; wider-range extrapolation and an
independent third temperature remain unvalidated. Generic builds keep nominal
100-kohm/B3950 conversion. Rebuilds must explicitly select the unit profile.
See [physical delivery and limits](review/bringup-2026-10-25/NTC-Factory-Profile.md).
This NTC adjustment does not complete the 33-condition OSL qualification or
authorize installing a candidate with missing gain-overlap evidence.

## First-board repeatability corrections — October 25, 2026

Independent coherent ADC blocks can have different common timer-start phases.
Source repeatability compares peak amplitudes; return impedance/admittance and
effective HG-transfer repeatability remain complex and retain relative phase.
The PC capture service rotates all six fundamental phasors by the same unit
rotation before averaging blocks, using VEXC1 as phase origin (VEXC2 if VEXC1 is
too small). This preserves within-block voltage ratios and channel phase while
preventing cancellation of otherwise coherent signals. It does not calibrate
ADC skew, physical phase error or source frequency.

Endpoint repeatability needs a nonzero scale: OPEN uses dimensionless
`y = (Vs - Vx) / Vx`, whose ideal value is zero, and SHORT uses impedance near
zero. Their standard-deviation limits use the existing 20000 ppm (2%) fraction
of `max(1, |mean(y)|)` for OPEN and `max(nominal RREF, |mean(Z)|)` for SHORT.
LOAD, source amplitude and effective HG transfer retain relative limits about
their nonzero means. Six accepted blocks and the ten-attempt bound remain.
This is an explicit endpoint-quality policy correction, not an accuracy bound:
2% of the range scale does not imply 2% of a small DUT value. Independent holdouts
and a measured uncertainty budget remain required. Nominal RREF sets the scale
of this stability test only; it does not replace the uncertain LOAD anchor.

First-board evidence exposed both issues: low-range OPEN had normalized residual
mean about 0.000516 with standard deviation 0.001299, while the source variation
was about 0.40%. At 1 MOhm / 1 kHz the unaligned source phasor variation was about
11.2%, but OPEN normalized-return variation was about 1.42% (1X) / 0.62% (HG).
These observations motivate software regression cases; they are not accuracy or
electrical qualification. Scope-dependent checks remain NOT_TESTED under the
owner's explicitly limited engineering campaign.

Calibration capture also refreshes real auxiliary samples between repeats,
recognizes fully drained aborts, and holds UART replies after the first hardware
attempt until the entire repeat workflow drains. Initial START acknowledgement
still precedes hardware activity. Capture deadline, permits, excitation levels,
OSL equations and persistent schema/model remain unchanged.
Successful acquisition also waits in BUSY for fresh post-capture auxiliary
readings before publication. This wait shares the original capture deadline;
timeout/cancellation/interlock never publishes a pending result. The C-backed
fixture exercises a real post-capture refresh delay, cancellation and timeout.

The first full SHORT sweep also exposed a separate endpoint rule: a known SHORT
must observe zero return without demanding the ordinary 50 uV nonzero DUT signal.
Only the calibration SHORT uses a zero minimum return magnitude. Excitation,
denominator, finite-data, clipping, six-repeat stability and electrical permission
checks remain. OPEN, LOAD and ordinary measurements retain their signal thresholds.
HG gain overlap still requires the original nonzero threshold on both channels;
noise at zero cannot establish a measured HG gain. The regression uses quantized
raw ADC blocks and checks zero SHORT, zero OPEN/LOAD, absent source and clipping.
Firmware identity changes require fresh same-image OPEN/SHORT acquisition; the
earlier 66 observations remain preserved and are not relabelled or mixed.

## October24 integration baseline

RGS-toGND boards now use hardware_revision0x00010002 (65538). Schema2/model5 and equations remain unchanged. Old0x00010001 topology records are incompatible; do not relabel them. Firmware and PC tools agree; stored bytes are not erased. New calibration must be captured on the reworked board.

See [current evidence, input limits and release status](review/bringup-2026-10-24/README.md).

## Purpose

The first PCB is not considered a qualified measurement instrument simply because it powers up. Rev.1 exists to measure and model:

- offset;
- gain;
- phase;
- switch RON;
- trace/relay/contact resistance;
- switch OFF capacitance;
- leakage;
- MCP6002 frequency response;
- PWM-related noise;
- thermal repeatability.

## Complex calibration

Each relevant calibration condition should be identified by a key similar to:

```text
hardware_revision
calibration_model_version
range
frequency
excitation_amplitude
```

`return_channel` is not part of the persistent Rev.1 condition key because Phase 05
captures RET_1X and RET_HG together for the same physical condition. A calibration
record may carry separate RET_1X/RET_HG correction terms and overlap evidence, but the
lookup key remains the shared hardware/model/range/frequency/amplitude condition.

Calibration temperature should also be recorded with an explicit validity flag. If no
valid NTC temperature is available for a capture, firmware must not substitute a
synthetic ambient value.

Stage 2B.2 treats this temperature as calibration provenance for the OSL acquisition.
It is not six independent thermal samples and it is not an input to runtime
temperature compensation.

Phase 08 product calibration freezes temperature per exact workflow rather than once
for the entire wizard. Immediately before each OPEN, SHORT, or LOAD workflow starts,
the wizard copies the latest auxiliary NTC snapshot into the workflow request; that
value then remains fixed for the six accepted samples in that workflow. A condition may
therefore solve from OPEN, SHORT, and LOAD standards captured at different temperatures.
Solver inputs preserve those distinct values and diagnostics expose min/max/span, but
no temperature-span rejection policy is defined before bench evidence.

The NTC datasheet and nominal lookup table establish a provisional temperature
estimate, not a per-board sensor calibration. A user-selected reference temperature
can support a one-point correction only when it comes from an independent thermometer
after thermal stabilization. A menu value selected without that reference must not be
treated as calibration. The board NTC does not measure DUT temperature. Runtime
temperature compensation of complex impedance requires measured drift evidence from
stable standards at multiple controlled temperatures; no coefficient may be inferred
from the NTC nominal curve or from one ambient-temperature point.

Phase 08 Stage 3A.3 keeps the OSL solver and persisted schema/model unchanged while
decoupling campaign aggregation from PRODUCT. The product wizard converts accepted
evidence to solver standards, calls the OSL solver directly for each condition, and
inserts the resulting record into the candidate set. BRINGUP retains a console-owned
campaign helper for engineering diagnostics. Full PRODUCT activation still requires a
validated Rev.1 set covering the fixed 33-condition domain; `10R + 500mV` remains
forbidden.

## OPEN / SHORT / LOAD

### OPEN

Fixture with no DUT. Characterizes leakage and residual admittance/parasitics.
Evidence should be taken from synchronized raw phasors. A true OPEN may make the final
impedance equation singular, so the calibration workflow must preserve normalized
OPEN observables such as `(Vs - Vx) / Vx` instead of rejecting the capture only because
`Zx` cannot be computed.

### SHORT

Repeatable short at the fixture. Characterizes residual series impedance, contacts, traces, switches, and relay path.
SHORT stability is evaluated from residual series impedance observables on each usable
return path.

### LOAD

Known standard within the useful region of the selected range. Characterizes scale and phase tracking.
LOAD stability is evaluated from measured impedance relative to the known complex
standard. Both VEXC paths and both return paths must remain observable so later
coefficient solving can distinguish 1X, raw HG, reconstructed HG, and overlap behavior.

The implemented Rev.1 Stage 2B.2 calibration model uses the normalized transfer:

```text
t = Vx / Vs
K = ZL * (tL - tO) / (tL - tS)
Zcorr = K * (t - tS) / (t - tO)
```

where `tS`, `tO`, and `tL` are the measured SHORT, OPEN, and LOAD transfers for one
exact range/frequency/amplitude condition, and `ZL` is the known complex LOAD standard.
This maps SHORT to 0 ohm, LOAD to `ZL`, and OPEN to a singularity. Coefficients are
computed from stable OSL evidence and stored as a projective/Möbius condition record.

Stage 2B.2.1 canonicalizes HG fitting from raw HG evidence:

```text
t_1x     = RET_1X / VEXC_1
t_hg_raw = RET_HG_raw / VEXC_2
H_HG_effective = t_hg_raw / t_1x
t_hg_canonical = t_hg_raw / H_HG_effective
```

Persisted `H_HG_effective` is an effective normalized path gain. It must not be
reported as the physical return amplifier gain because it also absorbs source-path
gain/phase mismatch. If no valid HG overlap was observed, the persisted condition may
carry a nominal fallback value for diagnostics, but runtime calibrated processing must
not select HG unless `HG_OBSERVED` is present.

The solver and model are software-implemented and synthetically host-tested. Physical
accuracy, model residuals, leakage behavior, and temperature drift remain
`REQUIRES_BENCH_VALIDATION`.

### Obtainable reference standards

The required PRODUCT OSL campaign must not depend on knowing a capacitor's ESR,
an inductor's winding loss, or an otherwise unpublished complex impedance. OPEN and
SHORT use repeatable fixtures at the DUT terminals. LOAD may use a stable precision
resistor near the selected range, represented by its published nominal resistance
and tolerance. That tolerance is an uncertainty bound, not evidence that the
individual part equals its nominal value exactly. The instrument must not claim
accuracy finer than the reference and the measured system error support.

The current PRODUCT wizard offers three fixed LOAD preset families. The selected
preset must match the actual resistor used for every prompted range; a different
nominal value must not be silently treated as one of these presets. If the purchased
reference set differs, a future menu-driven known-value entry or matching preset
must be implemented before using it for calibration. A user must not need a reference
LCR meter to discover a hidden ESR or phase value merely to complete the campaign.

Precision capacitors and inductors may be used as independent post-calibration
checks. Compare only quantities specified by their datasheets under compatible
frequency, excitation, DC bias, and temperature conditions. Nominal C or L tolerance
alone does not define ESR, Q, phase, or the full complex impedance at every Rev.1
frequency. A capacitor-specific ESR correction or thermal correction must remain
disabled until appropriate physical standards and repeatable bench evidence support
it; a clean software fit alone is insufficient.

The host-only supplementary campaign prototype accepts any number of current-campaign
R/C/L standards after the 33-condition OSL campaign. Each sample records nominal
value, maximum tolerance, exact capture condition and safe capture provenance. A
capacitor may supply neither ESR nor D, either one, or both. ESR/D may be
one-sided maxima or nominal values with explicit nonzero tolerances; an
unbounded typical value is never treated as exact. Same-condition ESR and D
intervals must be jointly satisfiable. Loss data constrain only the supported
matching frequency. A datasheet ESR at 100 kHz must not
be extrapolated into a 10 Hz/1 kHz/10 kHz fit. Loss constraints additionally require
a calibrated board NTC within 10 C of the stated datasheet temperature; this gate is
provisional and `REQUIRES_BENCH_VALIDATION`. The board NTC is not DUT temperature.
The PC CLI now binds each standard to a completed BRINGUP RAW file by exact SHA-256,
checks its condition and printed persisted-OSL DSP result, and rejects mixed active
OSL sequence numbers. A BRINGUP-only read-only COM command can export a canonical
serialization of the active in-RAM OSL set. The PC validates its sequence, CRC,
and all 33 unique condition records before saving it for campaign binding. This
is not byte-exact physical-slot readback or device authentication, proof of the
OPEN/SHORT/LOAD fixtures, or a qualified correction upload.

Current-campaign tolerance intervals are simultaneous hard constraints. The previous
active correction is only a soft minimum-change prior when a new campaign runs; it is
not decayed by elapsed time during normal use. Conflicting current samples require
repeat or explicit discard. Only one logical active coefficient set is retained on
the device, with redundant physical copies for power-loss tolerance. The current
host prototype does **not** write firmware records or change PRODUCT results.
Its diagnostic JSON reports the corrected complex impedance, derived SI value,
specified interval, and all-constraint pass/fail for each FIT and held-out VALIDATION
standard. These checks aid review and conflict triage but do not establish measurement
uncertainty or physical qualification.

Before producing a supplementary candidate frame, the host solver also rounds all 12
coefficients of each condition to the exact little-endian binary32 values stored on
the MCU and rechecks every current-campaign FIT and held-out interval. A fit that
passes only in host double precision is reported as `HOST_QUANTIZATION_FAILED` and
cannot become a candidate. This is a serialization consistency check, not proof that
target floating-point arithmetic, fixture effects, or physical accuracy are qualified.

DC DCR and low-voltage leakage remain separate future qualified measurement/calibration
modes, not new outputs inferred from AC OSL. The BRINGUP-only 1 MOhm static-bias pilot
is exploratory evidence collection, not DCR or leakage calibration. DC short/offset
and known-resistor evidence are required for DCR; open-fixture leakage, actual bias,
dwell, and uncertainty are needed
before a leakage number can be published. The Rev.1 low-voltage path cannot claim
datasheet high-voltage insulation resistance.

The Stage 2B.2 host comparison deliberately injects complex gain/phase error, residual
series impedance, and shunt leakage/admittance, then validates impedances that were not
used as OPEN/SHORT/LOAD fit standards. In that deterministic model the previous
two-complex affine correction fitted at SHORT/LOAD leaves substantially larger
intermediate residuals, especially near high impedance and with complex reactance,
while the OSL/Mobius transfer maps the synthetic DUTs back to the true impedance within
the configured float tolerances. This justifies replacing the mathematical
`model_version`; it does not qualify the real PCB.

## Optional supplementary runtime (A02.1)

`WTK_ENABLE_SUPPLEMENTARY_CURVES` defaults to OFF. This does not disable ordinary
OSL calibration, change schema v2/model v5, or relax qualification/boot prerequisites.
`measurement_cal_supplementary_supported()` reports build capability, not data
qualification. With capability OFF, `measurement_cal_apply_curve()` returns
`MEASUREMENT_CAL_CURVE_NOT_SUPPORTED` without modifying the result. A fresh OSL
measurement clears `supplementary_applied`; only successful enabled application
sets it. The UI must not interpret OSL qualification as supplementary application.

The PRODUCT OFF image does not load/read WCRV records. Both reserved A/B partitions
and their format remain intact, including previously provisioned records. Enabling
the runtime restores the existing qualified-record validation and post-OSL path.
See the [ON/OFF state table and measured cost](review/a02/README.md) before budgeting
future curve installation. This capability is not evidence of physical qualification.

## Range validation

For each range and frequency:

1. measure standards near 0.1× RREF;
2. measure near 1× RREF;
3. measure near 10× RREF;
4. repeat at each allowed excitation amplitude;
5. measure repeatability and drift;
6. record magnitude and phase error;
7. document clipping/SNR/headroom boundaries.

## Qualification components

Recommended references include:

- precision resistors;
- C0G/NP0 capacitors for smaller values;
- film capacitors where appropriate;
- known inductors, ideally cross-checked with a reference instrument;
- repeatable OPEN/SHORT fixtures.

## Metrics

Record at least:

- absolute/relative error;
- standard deviation over repeated measurements;
- SNR;
- excitation THD when measurable;
- ADC headroom;
- temperature;
- residual voltage after SAFE/discharge;
- actual excitation frequency;
- actual sampling frequency;
- selected range/channel/amplitude.

## NOMINAL and EXTENDED

Firmware marks a combination `NOMINAL` only after enough measured evidence exists.

Initial engineering objective, not a guaranteed specification:

- central qualified region: roughly 1–2% class where achievable;
- extremes / `EXTENDED`: larger error may be acceptable if explicitly reported.

High-Z ranges, especially 1 MΩ, must not enter unrestricted automatic use before leakage and switch OFF capacitance are characterized.

## Regression policy

Changes to any of the following may invalidate calibration:

- op-amp;
- MOSFET;
- relay;
- RREF part/value;
- analog-path PCB layout;
- acquisition timing/sampling firmware;
- relevant filter values;
- calibration algorithm/model.

Persistent calibration records therefore carry `hardware_revision` and model/schema version information.

## Low-impedance search versus calibration

The bounded search in [plan 23](../plans/23-Bounded-Low-Impedance-Search.md)
adds a supporting 10-kHz attempt after a usable, unclipped,
apparently resistive 1-kHz result on the 10/100-ohm references. It changes
condition selection only. Finite negative estimated R may also be investigated,
while existing negative-resistance/loss publication guards remain unchanged.
The OSL model, coefficients, HG transfer, hardware
revision, ADC timing, calibration serialization and publication gates remain
unchanged. The existing active engineering set can be retained; this change
does not require another resistor calibration campaign. Each selected condition
still needs compatible calibration and real hardware safety permission.

Validation must distinguish extra search from a successful derived value.
Record all attempts, actual frequency/RREF/path and primary index. A second
frequency that disagrees with the first may remain MIXED/UNKNOWN. Do not turn
small negative X into positive L, suppress inconsistent evidence, or infer an
instrument accuracy bound from the nominal DUT marking. The pre-change
0.47-uH observation and its SAFE teardown are preserved in
[bench evidence](../Firmware/releases/2026-10-25/inductor-047-limit-check/README.md).
Sub-microhenry sensitivity, fixture parasitics and phase accuracy remain
REQUIRES_BENCH_VALIDATION.


## Evidence selection and held-session qualification (2026-10-26)

The original four-attempt nominal 0.47-uH capture is replayed in host tests.
Its early 0.1245/1.0915-mV selected returns are retained but cannot vote under
the existing 2-mV weak-return threshold; the later 9.305/5.875-mV attempts remain
resistance-dominated. This fixes evidence admission, not small-inductor accuracy.
Threshold, raw signs and conditional C/L/loss publication remain unchanged.

The PC-provisioned PRODUCT session owner may keep K1 connected across bounded
1/10-kHz captures, while fixed calibration acquisition still returns SAFE per
capture. Stored OSL schema/model/hardware compatibility and coefficients are
unchanged. The engineering set and UID-bound experimental NTC profile must be
preserved through deployment. No repeated resistor campaign is part of this task.

Host tests cover single K1 entry across actual range-service transitions,
neutral/disabled address changes, fresh initial authorization, deadline/battery
expiry, six-capture limit, timer wrap, charger/fault/DMA/cancel failures and
publication only after SAFE cleanup. These are software/fake-I/O observations.
Actual relay counts/timing, analog settling and phase consistency with continuous
connection remain REQUIRES_BENCH_VALIDATION; no owner button/fixture interaction
is available during this work. No debugger state injection or reset-triggered
DUT measurement is an acceptable substitute for a supported measurement command.
