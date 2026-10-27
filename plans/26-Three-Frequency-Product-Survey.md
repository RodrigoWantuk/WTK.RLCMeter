# Three-frequency product survey

STATUS: IMPLEMENTED_DEPLOYED_BASIC_CAPACITOR_AND_INDUCTOR_OBSERVED; REQUIRES_BENCH_VALIDATION (accuracy/polarized envelope/timing/UI)

The owner requests all existing frequencies before R/C/L classification, including
low frequency last for large capacitors and inductors. The 10-mF upper target is
documented but was not covered by the delivered automatic 1/10-kHz search. Do not
claim that implementing a survey qualifies 10-mF electrolytic measurement.

## Scope and prerequisites

Extend the open PR 25 on codex/trimmed-measurements. Plans 24/25 supply guarded
session ownership, evidence admission and seven-point coordinate trimming. Read
the safety, DSP/calibration, hardware and product-mode contracts. No new drive
frequencies, persistent format, calibration coefficient, ideal fallback, amplitude
exception or residual-evidence override is authorized by this plan.

Factory PRODUCT explicitly opts into a 1-kHz -> 10-kHz -> 10-Hz survey, independent
of initial interpretation. Range selection remains adaptive within each stage;
this does not mean an exhaustive six-RREF Cartesian sweep. Bound each stage to
four distinct conditions and the total to twelve raw search captures, nine range
changes and two frequency changes. Reserve the later frequency stages: do not
finish R/OPEN/SHORT from the first frequency. Cancellation/hardware failure still
ends immediately. Preserve every raw attempt and expose attempted/usable frequency
masks. Incomplete coverage cannot claim a fully surveyed resistor result.

Twelve 156-byte attempt records add 936 B over the six-record factory history.
This deliberately spends SRAM for traceability. Inspect actual linker output:
keep the existing 17,408-B hard accounted-RAM gate and 2-KiB stack reservation;
report any preferred 16,384-B budget overrun. Legacy/fixed profiles keep six records
and previous policy. No heap or extra ADC buffer is allowed.

## Evidence and classification

Combine only usable unclipped finite calibrated observations under existing
amplitude/phase thresholds. Low-frequency resistance dominance is compatible
with high-frequency inductive evidence; high-frequency resistance dominance is
compatible with low-frequency capacitive evidence. Opposing usable reactive
signs or inconsistent reactance trends remain UNKNOWN; do not force a type or
lower thresholds to manufacture sub-uH sensitivity. Prefer one supported primary
condition for numeric publication and perform the existing seven-sample trimmed
estimate there. Never average across frequencies/RREF/path/calibration. Keep
unqualified engineering calibration marked as such.

## Hardware ownership and timing

Do not lengthen the 500-ms continuous fast-segment authorization or original
battery-evidence expiry. Drain K1 to SAFE, restore ADC/auxiliary sampling and
release quiet/workspace before entering 10 Hz or starting another fast segment
after its six-search-capture budget. Every new segment/capture must obtain an
ordinary real fresh permit. Ten-Hz capture retains the existing fixed-capture
800-ms sine settling / 400-ms ADC window and SAFE teardown. Repeated 10-Hz
statistical windows also use this safe path, even though it requires additional
relay cycles; no blind continuous multi-second authorization is introduced.

Fresh statistical segments begin through normal authorization, then fast
continuations use the existing explicit statistical role/lock. App DRAINING
cannot publish or restart until cleanup succeeds; cancel/fault during draining
revokes the result. A bounded 60-s overall application survey timeout includes
the worst-case twenty captures and up-to-1-s fresh-evidence waits. It is not a
replacement for any hardware capture/segment timeout. No new ADC_BAT SWD read.

## Validation and deliverables

Test true R/C/L impedances over all three frequencies, 4,700/10,000-uF synthetic
recovery from high-frequency SHORT, inductor low-frequency R/high-frequency L,
opposing reactance/resonance, weak/clipped coverage, OPEN/SHORT and bounded range
failure. Test segmented cleanup, fresh-permit handoffs, seven-sample 10-Hz and
fast-primary completion, cancellation/deadline/late-cleanup failures. Keep legacy
host regressions, all nine strict unit-profile builds and memory gates.

Update product/safety/DSP/calibration docs and PR around final behavior. Prepare
bench cases and preserve existing assets/OSL/NTC. Before deployment determine the
current connection and DUT; never automatically excite a connected electrolytic
under the existing unqualified bipolar AC mode. If the owner cannot supply a
suitable nonpolarized/reference fixture, finish software and report physical
validation pending. A synthetic large-C test is not actual 10-mF qualification.

## Explicit classification and DSP recovery refinements

The survey requires usable unambiguous all-frequency evidence for a resistor
label, and leaves sub-ohm resistance-dominated observations unknown/impedance
under the documented ~1-ohm lower engineering target. This is a conservative
publication restriction, not a lowered reactance threshold or a new accuracy
bound. Persisted engineering coefficients remain applied and unqualified.

Acquired weak/near-endpoint DSP failures must not be confused with hardware
capture failures: rerange low when source exists, then visit the other frequencies.
Invalidate DSP output before processing; missing or falsely successful output
cannot reuse older data. Actual DMA/permit/safety failures still abort immediately.

## First-board integration findings

Source 48bc949 passes algorithmic survey tests but has a final-event integration
bug for already-SAFE statistical completion. Internal session 3 (owner-triggered
attempts, later confirmed 4,700-uF electrolytic) has all three frequencies, five
raw search captures, seven valid 10-Hz/RREF10/HG samples and an aggregate near
4,224 uF. PRODUCT retained the preceding 1-kHz partial value because the session
entered DONE while returning NONE instead of FINAL_RESULT. The same pattern
existed for a drained cancel/cleanup failure. Correct both terminal-step returns
and regress the actual PRODUCT view transition, not just final_ready metadata.
This is an observed internal calculation, not calibrated accuracy or polarized-
mode qualification. No remote capacitor measurement is requested.

The initial boot diagnostic also paused the MCU during session 1 acquisition.
That was a tooling error, the core was resumed, and that run is explicitly
invalidated and preserved. Live DAP snapshots subsequently confirmed READY/SAFE,
no faults, restored ownership and unchanged calibration. Replace postboot/result
reads with nonhalting DAP snapshots and stable sequence/output guards; serial
boot can be captured now that the owner reconnected COM4. No debugger battery
voltage read is introduced. A clean corrected boot remains to be confirmed.

## Corrected delivery and owner-triggered basic checks

Final source 0443d3295bd92043493ae805c2763464109ec3fb passes 49 factory and
47 standard host tests and all nine strict unit-bound embedded profiles.
Factory PRODUCT is 60,136 B Flash / 17,080 B accounted RAM, above the preferred
16,384-B RAM target but within the unchanged 17,408-B hard gate with a 2-KiB
stack reservation. No low-level BSP/driver/hardware/profile, DSP equation,
calibration coefficient/format or NTC unit-profile change occurred.

Verified internal programming/readback SHA-256 is
26cb50c86e798d0ca0b040ddd5ab3a6a1f8e9791145a504bd2e8c6f46fa8e20f.
Clean controlled boot captured exactly one SAFE_BOOT banner on COM4, READY/SAFE,
zero sessions/captures, no faults and active OSL1/all33. All postboot/result
inspection used live DAP with identity/image/output/sequence guards, never
pausing/resetting the core or reading battery voltage via SWD. One preflight
attempt deferred safely on an unpolled OpenOCD unknown state; poll corrected the
observation without reset/resume. No programming occurred in that deferred step.

The first corrected owner-triggered attempt was CANCELED: retained button events
show two complete OK cycles 165 ms apart. No hardware/calculation fault was
claimed and that canceled run is preserved separately. The next owner-triggered
4700-uF electrolytic run (session2) completed five search captures, all three
frequencies and six extra windows/seven accepted primary samples at
10Hz/RREF10/HG. It published a final nonpartial 4,196.611 uF to the PRODUCT view;
replayed masks R=25/X=73 and complex mean match. App-start-to-last-K1-SAFE time
from firmware ticks was 9,428 ms. The owner observed repeated relay cycles,
expected from fresh-permit SAFE boundaries; exact physical contact count/timing
was not measured. This is functional observation, not accuracy or polarized-mode
qualification; the actual component capacitance is not independently known.

The owner then connected the marked 470-uH inductor and initiated measurement.
The latest completed result (session5; intermediate sessions3/4 not recovered)
completed three frequency probes plus six extra windows/seven accepted primary
samples at 1kHz/RREF10/HG. Final nonpartial result is 466.910 uH, with replayed
masks R=84/X=22. Its 10-Hz observation is locally RESISTIVE (about 7.433+j0.0524
Ohm), while 1/10 kHz support INDUCTIVE. Combined classification correctly stays
INDUCTIVE and does not average the low-frequency scalar L into publication.
Software start-to-last-SAFE duration was 1,742 ms. Both completed checks ended
hardware IDLE, K1 SAFE, range/excitation OFF, quiet/workspace free, no hardware
fault and unchanged active calibration. No resistor campaign was repeated.

Remaining: independent accuracy/phase/loss, actual 10 mF upper endpoint, polarized
excitation envelope, switching/abort/contact timing, sub-uH sensitivity and
physical display appearance of F3/3/AVG7/3. Current UI data publication is observed;
TFT was disconnected in favor of COM4 during the corrected tests. New serial
measurement/result commands were not invented: UART boot is captured, completed
result/metadata is live SWD, and all acquisitions were owner-triggered buttons.

The functional specification's stale 100-Hz baseline entry is corrected to the
existing 10-Hz/model-5 baseline; no new frequency profile is introduced.
