# Production completion and quick lead OPEN/SHORT compensation

STATUS: AUDIT_COMPLETE_IMPLEMENTATION_NOT_STARTED

Owner scope: audit all firmware, especially PRODUCT; identify unimplemented or
misleading behavior; complete usable settings/audible feedback; design a native
quick OPEN/SHORT option after changing leads; account for verified memory.
The immediate deliverable is the [audit and evidence](../docs/review/production-audit-2026-10-27/README.md)
and this execution plan. Firmware corrections and the new menu remain pending.
This plan is not a release qualification or a claim that earlier bench checks
were invalid: each check retains its narrow scope.

## Prerequisites and boundaries

- Read the root AGENTS.md mandatory documents, plus DSP/calibration/UI/bring-up
  documents as appropriate. Plans 24–29 establish current first-unit behavior.
- Baseline source: `842276976c7f44a5fcb3b7ed287325428f01668d`; installed image
  source `9ecaa696ec785e6c15f240df23f75bd07d40d5ff`, 61,596 B.
- Preserve UID `54ff70068370525760380567`, OSL sequence 1/all 33, owner splash,
  resource pack and UID-bound experimental NTC profile
  `Firmware/config/units/54ff70068370525760380567-ntc-engineering.json`
  (SHA-256 `9b3826a20b3c621a9e92be0035431b140c45d7a23acaba8ec237a63e4e8cad44`).
- No repeated manual resistor campaign for application fixes. Quick lead
  compensation requires OPEN/SHORT at the ends of the final leads, not six LOADs.
- Retain actual hardware safety permits, residual/power freshness, charger
  interlocks, neutral guarded range transitions, quiet acquisition, bounded
  connected time and deterministic SAFE teardown. UI never owns relay GPIO.
- C17/CMake, no RTOS/heap/framebuffer. Generic C8 remains 64 KiB; the verified
  first-unit 128-KiB engineering variant is explicit and UID bound.
- No feature should bypass unresolved accuracy, polarized-capacitor or voltage
  qualification. Do not infer an operational envelope from one successful DUT.

## Execution order and change boundaries

Use bounded PR commits/sub-phases, each with tests, map deltas and updated status.
The existing authorized PR workflow can carry the audit; do not bundle every
pending feature into one unreviewable implementation.

| Work package | Priority | Dependencies | Required result |
| --- | --- | --- | --- |
| A: capacity/budget baseline | P0 enabling | Plan 29 | Explicit unit 128-KiB build + honest memory accounting |
| B: emergency SAFE/watchdog | P0 | Existing BSP and safety | Deterministic hard-fault path and checked watchdog |
| C: settings persistence | P0/P1 | Existing settings/W25Q | Correct asynchronous ownership and newest edit survives reboot |
| D: idle/backlight and buzzer | P1 | B/C | Working settings effects, quiet-safe audible policy |
| E: bounded UART and telemetry | P1 | A RAM proof, B | Reliable genuine diagnostics and guarded serial OK |
| F: production feature contract | P0/P1 | A, existing profile matrix | One supported PRODUCT measurement/UI behavior |
| G: lead correction math | P1 | Existing factory OSL | Host-tested finite/conditioned complex correction |
| H: lead storage layout | P1 | A/C/G | Power-loss-safe records, old partitions preserved |
| I: endpoint service + runtime | P1 | B/F/G/H | Real qualified endpoint collection and application |
| J: native lead wizard | P1 | D/I | Two user actions; no PC/SWD/LOAD required |
| K: truthful status/UI | P2 | C/E/F/J | Actual state, provenance, no truncated coverage |
| L: qualification/release | P0 release gate | A–K | Software checks plus explicitly scoped physical acceptance |

A and B/C can be prepared independently. G can use host-only math while C is
fixed. UART buffering and lead runtime integration must wait for proven RAM
headroom. Every package remains NOT_STARTED until implemented and verified.

## A — memory and deployment contract (F11)

Expected files: CMakeLists/presets, linker scripts, size-check tooling, unit
manifests/programmer target checks, build documentation. Identify exact existing
tool entry points before edits; never silently change all C8 geometry.

1. Add an explicit engineering `128 KiB / UID verified` memory profile. Record
   both the real 64-KiB factory register and 128-KiB configured geometry. Default
   C8 configuration stays 64 KiB. Reject an unknown UID for the unit-only profile.
2. Match linker LENGTH, deploy bounds, page geometry and binary/map size checks.
   Proposed hard Flash budget is capacity minus 1,024 B: 130,048 B at 128 KiB;
   this is a deliberate engineering budget decision, not ST qualification.
3. Carry current unit NTC metadata into every unit image. Prepare rollback of
   MCU image and separately preserve external mutable data/assets. No mass erase,
   option-byte/RDP changes or repeated capacity stress experiment.
4. Measure exact static RAM and stack after each package. Current accounted RAM
   is 17,116 B / 17,408 B (292 B margin); 128-KiB Flash adds no RAM.
5. Prototype a factory load-only calibration service, eliminating duplicate
   2,576-B scan-set lifetime where possible and the unused full-workflow state.
   Preserve active coefficients, validation/slot fallback, full-wizard builds and
   host contracts. Reuse the existing 3,076-B I/O workspace by explicit ownership;
   never alias it while acquisition or storage still owns data.
6. Do not claim savings until Arm map/type/stack evidence confirms them. Retain
   the 2,048-B stack reserve and RAM hard gate; add stack high-water measurement.

Acceptance: generic 64-KiB and opted-in unit profiles build and reject oversized
images/incorrect deployment identities. All supported profiles pass warning/size
policy. Before new state/rings are integrated, RAM headroom covers their actual
sizes without relaxing the existing reserve. Physical extended-Flash endurance
and temperature status remains REQUIRES_BENCH_VALIDATION.

## B — deterministic emergency SAFE (F07)

Expected files: BSP startup/fault/watchdog/GPIO/timers and app startup/safety tests.

- Add a minimal BSP shutdown used by unexpected fault handlers: excitation off,
  range enable off and K1 de-energized with known board polarity. It must work
  before full initialization and without SPI, DMA callbacks, heap, logging or
  an application lock. Keep the watchdog reset fallback.
- Propagate watchdog initialization failure to a latched startup fault and block
  measurement. Report the failure later only through safe normal-loop diagnostics.
- Document boot/reset/fault output ordering and which failures reset versus latch.
- Test register-level fault/stop sequencing where practical; physically inject a
  controlled fault with an empty passive fixture and measure source/K1 shutdown.

Acceptance: no measurement permission if watchdog setup fails; faults drive SAFE
without waiting for UI/Flash. Actual shutdown timing remains unqualified until
measured. Do not replace a missing test with a claimed two-second guarantee.

## C — real settings persistence (F01–F03)

Expected files: settings service/header, settings W25Q adapter, common async I/O
contracts if necessary, app save scheduling, storage tests and migration docs.

1. Define `not started / accepted pending / completed / failed` semantics or a
   similarly explicit owned-operation contract. Distinguish transport contention
   from an accepted W25Q erase/program. Check all settings/calibration/resource
   callers; preserve quiet/exclusive ownership and actual device polling.
2. Add bounded start/transaction timeouts, abort/error reporting and recovery.
   Storage errors may disable saving/resources but must not leave K1 connected.
3. Keep a captured write revision separate from mutable current settings. On
   successful readback, persist that snapshot; if the user edited again, schedule
   the newest dirty revision once. Latest-value coalescing bounds RAM and wear.
4. Version settings with header-plus-payload CRC and last commit marker. Validate
   bounds/reserved fields/sequence wrap; migrate supported schema 2/3 via inactive
   slot. A corrupted/newer frame must not hide an older usable slot.
5. Expose saving/saved/failed state. Long-cancel restores the documented edit
   snapshot; reboot restores only the last successfully committed version.

Tests: turn all three audit probes into passing-behavior regressions; actual
W25Q adapter/driver start/poll integration, busy-before-start, page boundaries,
poll failure/timeout, edits at each write phase, both slot corruptions, power
loss at every erase/program/commit point and sequence wrap. Do not use only an
always-OK fake. Physical acceptance: save each setting, power-cycle, read back;
preserve OSL/assets and confirm measurements remain possible after edits.

## D — applied settings and audible feedback (F04–F06)

Expected files: app_product, app_shell output adapter, hw_buzzer policy, BSP TIM4,
UI/controller/hardware tests and user controls documentation.

- Repair timeout without weakening hardware permits: expected disabled idle
  ranges are not an emergency; charger/residual/fault/measurement states retain
  their defined wake policy. Wake click is consumed before any measurement/menu
  activation. Cover rollover, OFF/15/30/60-s choices and actual READY blockers.
- Verify brightness preview, cancel, committed restore, language and mute on real
  output paths. Keep exact changed-row updates and prevent duplicate full paints.
- Define a small static pattern table for navigation/confirm/back/rejected/save
  success/save failure/measurement complete/fault. Limit queue depth and define
  priority, duplicate coalescing and overflow behavior. No long blocking tunes.
- Maintain an atomic stopped/armed buzzer state, clear pending TIM4 sources on
  stop, and guard late ISR entry. Quiet acquisition cancels/defer-drops according
  to event policy; a completion event may play only after SAFE teardown.
- A BUSY/play failure must not silently consume a required output acknowledgment.
  Muted events must not build an audible backlog on unmute.

Acceptance: idle repro now sleeps; both language layouts remain correct; save and
power-cycle preserve values. Mocked late IRQ tests pass; physical tones work with
sound enabled, cease immediately when muted/quiet, and do not corrupt acquisition.

## E — UART robustness and genuine telemetry (F09/F14)

Expected files: BSP USART1, bounded parser/serial UI service, PC client, app sensor
snapshot adapter, protocol documentation and tests.

- Size a fixed RX ring against RAM and worst cooperative latency. ISR only reads
  status/data and updates bounded indices/error counters; acquisition keeps
  higher priority. Define critical sections, overflow policy and error recovery.
- Preserve duplicate/stale request and expected-session checks. A buffered command
  arriving while quiet/busy cannot execute later after the DUT may have changed.
  Define reception epochs/discard or explicit busy replies; do not queue OK.
- Add paced host writes while needed. Never retry an ambiguously delivered start
  automatically. Exercise bursts, split frames, corruption and timeouts.
- Version capability/status/result messages: UID/build/effective profile, genuine
  battery/NTC values with validity and age, charger, fault, calibration and lead
  sequences, search/trim coverage, primary frequency and raw-versus-corrected
  provenance. Do not invent numeric fields unavailable from fresh samples.
- Reject nonfinite/out-of-range numeric policy inputs explicitly. Respect the
  owner's request that numeric battery readings use UART, not SWD.

Acceptance: normal and paced bursts survive concurrent UI work; overflow is
visible and resynchronizes without unintended activation. Quiet-time injection
does not start a later measurement. Actual sensor telemetry is compared with
the existing auxiliary snapshot, without a new resistor campaign.

## F — one production feature contract (F08)

Expected files: generated build config, CMake presets/options, product controller,
UI contracts/tests, manifests and supported-profile documentation.

Decouple provisioning choice from continuous K1 ownership, three-frequency
classification, seven-point estimates, rich result provenance and row caching.
Make feature differences intentional and advertised. Keep diagnostic/fixed
capture paths and full factory OSL/recovery separately supported. Do not adopt
unsafe old behavior to save a few bytes in a preset named release.

Acceptance: all final PRODUCT variants share tested classification, cleanup and
display behavior; manifests enumerate effective features. Tests cover serial
ON/OFF, provisioning variants and generic/unit memory profiles. Expected flash
growth fits the selected profile; do not automatically link every diagnostic
feature into the product.

## G — pure complex lead-correction math (F10)

The new feature is **fixture compensation relative to the existing factory OSL
reference plane**, not a new absolute RREF/HG calibration. Two endpoint standards
cannot independently recover arbitrary factory gain drift. Factory LOAD and
HG transfer remain unchanged.

Proposed fast model at each of the three existing frequencies:

```text
Zs(f) = residual series resistance + j*omega*series inductance
Yo(f) = residual parallel conductance + j*omega*parallel capacitance
Zcorr = (ZF - Zs) / (1 - Yo*(ZF - Zs))
```

This is the conventional fixture OPEN/SHORT model; see
[Keysight, Impedance Measurement Handbook, section 4](https://www.keysight.com.cn/cn/zh/assets/7018-06840/application-notes/5950-3000.pdf).
The implementation below composes that model with this project's factory OSL.
It needs project-specific conditioning and bench validation.

For factory transfer `F(t)=k*(t-ts)/(t-to)`, form:

```text
A = k - Zs
B = Zs*to - k*ts
C = 1 - Yo*A
D = -to - Yo*B
Zcorr(t) = (A*t + B)/(C*t + D)
```

Compose before rejecting an intermediate factory OPEN pole. Estimate OPEN
admittance from transfer/endpoint observations directly rather than forming an
infinite `ZF` and inverting it. Apply consistently to the calibrated complex
pipeline before final classification/trim/derived C/L, preserving uncorrected
data where finite. Do not simply replace OPEN/SHORT poles while leaving `k`
unchanged: the audit example produces 9.5238 ohm for a true 10-ohm DUT.

Expected new pure module: `measurement/measurement_lead_compensation.[ch]` and
host tests; exact names can follow existing conventions. No HAL/TFT/W25Q
dependency. The 48-B coefficient core is two complex floats per frequency;
include separate finite/quality/binding metadata. Validate the proposed
frequency-only, path-independent fixture model across ranges/amplitudes. If it
fails, explicitly narrow coverage or use a longer per-condition procedure; do
not claim untested range invariance or silently fill missing keys with nominal
coefficients.

Tests: zero compensation identity, synthetic complex R/C/L across six references
and three frequencies, finite residual OPEN leakage, SHORT inductance, factory
pole crossing, sign/conditioning limits, singular denominator, invalid profile,
noise/dispersion and independent derivation checks. The existing 108-case audit
prototype is an algebra sanity check only; implement tests with production
float precision, actual ratio extraction and realistic noise before acceptance.

## H — separate, atomic lead-profile persistence

Expected files: storage_layout, new versioned lead record codec/store, W25Q
adapter, PC asset/provisioning/preservation tools and layout tests/docs.

- Store lead profiles separately from factory OSL and UI settings. Bind to UID,
  hardware revision, OSL schema/model, factory set sequence **and content CRC**,
  algorithm schema, frequency coverage and qualification state. A new OSL set
  invalidates dependent lead compensation. Sequence wrap and bad-slot fallback
  need defined behavior. Temperature/reference method may be informational;
  no invented RTC date on this hardware.
- Use redundant sector slots, bounded little-endian fields, header/payload CRC,
  commit last and full readback before activation. Failed/cancelled capture/save
  leaves the old profile active. A valid, bound record loads at boot without PC.
- The old 32-KiB mutable tail already contains OSL A/B, settings A/B, WCRV A/B,
  diagnostics and bring-up test sectors. **Do not increase the reserve constant
  and shift all existing offsets.**
- Proposed additive layout: two new 4-KiB sectors immediately before the old
  tail, with all old addresses still anchored to `capacity - 32 KiB`. Reduce the
  resource upper bound by 8 KiB. First check current pack end and actual device
  capacity; if overlapping, use a guarded resource repack preserving the owner's
  image and all mutable records. Update every writer's erase/preservation bounds.
- Snapshot/hash old tail and new sectors before migration. Test actual 4-MiB and
  baseline 8-MiB devices, minimal capacities, every partition boundary and old
  layouts. Do not repurpose WCRV/diagnostic slots silently.

Acceptance: interruptions at every write phase recover the previous valid lead
profile; layout migration preserves exact OSL/settings/assets/NTC provenance.
An unbound/corrupt profile is visibly inactive, never silently misapplied.

## I — real endpoint capture and runtime integration

Expected files: app endpoint service, existing acquisition/session abstractions,
measurement calibration/engine, ownership arbitration and tests. Reuse endpoint
ratio observation code from `app_cal_capture_sample_t` where suitable; do not
route raw endpoint capture through ordinary finite-DUT AUTO acceptance.

1. Capture OPEN and SHORT at 10 Hz/1 kHz/10 kHz using real coherent phasors.
   Proposed SHORT setting: 10-ohm reference, 100 mVrms. Proposed OPEN setting:
   highest useful reference and safe amplitude, subject to actual 1X/HG quality.
   Confirm observability and noise before fixing these as product constants.
2. Collect seven valid observations for each endpoint/frequency, with bounded
   attempts, dispersion/clipping/ratio quality gates and endpoint-specific
   acceptance. An OPEN pole or near-zero SHORT is expected, not a generic error.
3. Use one hardware-owned connected group for seven same-condition windows,
   then SAFE between frequency groups and all manual prompts. Six groups mean
   42 accepted windows. Existing statistical continuation locks its condition;
   do not reuse one 21-window group outside count/deadline contracts. Give endpoint
   grouping an explicit role if current admission cannot express it safely.
4. Retain 10-Hz settling/window durations and interleaved battery/VMID refresh.
   Acquisition estimate is roughly 20–30 seconds plus operator changes, based on
   current 800-ms settle/400-ms low-frequency windows; **not measured UX timing**.
   Show progress rather than promising instant completion or shortening settling.
5. Keep active lead coefficients in RAM before quiet acquisition. Reuse the
   existing product runtime union for endpoint workspace; no extra raw ADC buffer,
   no Flash read inside continuous quiet ownership. Clear cached hints when the
   lead/factory sequence changes; never mix different profiles in seven samples.
6. Apply correction to all published R/X/C/L/ESR/Q/D through the single pure
   pipeline. Preserve raw/factory-corrected evidence and actual coverage flags.
   Do not convert inconclusive results into plausible nominal components.

Acceptance: correct ownership/cleanup on cancel, timeout, charger, stale power,
residual, clipping, unsupported capture and storage failure. Normal measurement
retains one K1 engagement/final release; wizard grouping is separately documented.
Actual lead-end endpoint observations and at least one independent R/C/L holdout
are needed for supported coverage, not a new factory resistor campaign.

## J — native production quick calibration menu

Expected files: product controller/UI view/state, text resources, lead service
adapter, controller/UI tests and operating guide. Available in supported final
PRODUCT, including PC-provisioned builds; no SWD/PC required by the operator.

```text
Calibration -> Compensate leads (OPEN/SHORT)
INTRO -> WAIT_OPEN -> CAPTURE_OPEN -> WAIT_SHORT -> CAPTURE_SHORT
      -> VALIDATE -> SUMMARY -> SAVE -> COMPLETE -> READY
```

- OPEN means disconnected lead tips held in the intended geometry. SHORT means
  join the **far ends of the installed leads** with a firm low-inductance contact.
  Show which stage/frequency and completed/total accepted windows.
- Short OK confirms each manual step/save; UP/DOWN navigates summary/options;
  long OK cancels safely. Buttons during capture cannot restart or queue another
  capture. No direct UI relay control. All waiting states are actual SAFE.
- Summary shows measured residuals, coverage and provisional/qualified status,
  and offers save/retry/cancel. Failed validation cannot commit partial values.
- Add separate view/enable/disable/clear lead compensation actions. Disabling
  preserves factory OSL. Changing tips/leads requires repeat OPEN/SHORT. An
  absent profile uses explicit factory-only status, not invented zero correction.
- Apply incremental row updates for progress/selection/numeric changes. Require
  full redraw only on structural screen changes, language/assets or recovery.

Acceptance: host traces for success/cancel/retry/error/power-loss/reboot, button
debounce/wake behavior, both languages, real display prompts and no unintended
measurement at boot. Physically demonstrate changed leads, one OPEN step and one
SHORT step, saving/rebooting and restoring factory-only behavior independently.

## K — truthful status and remaining product gaps (F12–F14)

- Replace unconditional CRC_OK/PACK_API/PC-ready assertions with real verified
  status/version/capabilities/progress or explicit instruction labels.
- Show current settings save state, factory OSL and lead sequences/coverage,
  experimental NTC provenance, actual battery/charger/temperature/freshness and
  faults. A loaded engineering calibration is not independently qualified.
- Split bounded result metadata rows so range/channel/count/AVG7/3/frequency
  coverage/lead status fit at legal maxima in EN/PT. Preserve n/a and unknown
  results when evidence is insufficient.
- Label unused image mockups as illustrations. Future graphs must derive from
  actual trace/model data with an honest label; do not display prepainted readings
  as current measurements.
- Document supported CLICK mode and three discrete frequencies. LIVE, arbitrary
  frequency sweep, qualified large polarized/DC mode, Kelvin and native USB are
  separate features/hardware decisions. They are not incidental refactoring.

Acceptance: actual source of every displayed status/numeric field is identified;
failure/missing-data UI never claims CRC/accuracy/link success without evidence.
No policy UI module includes low-level range/relay control headers.

## L — integrated acceptance and handoff

Software gates: relevant host suites with realistic asynchronous contracts;
strict embedded profile matrix; serial ON/OFF; generic64/unit128 bounds; linker
map/RAM/stack reports; codec migration/power-loss and parser corruption cases;
dependency boundaries; warning policy; resource manifest and preservation tools.
Rebuild once per relevant change set; do not repeat the full matrix without cause.

Physical checklist, with evidence attached rather than inferred from mocks:

- Boot/reset/fault/charger/stale sensor aborts drive source/range/K1 SAFE.
- Settings apply, cancel, save, reboot and survive one controlled interrupted save.
- Idle sleep/wake, brightness/language/menu row updates and audible mute/patterns
  work on the actual display/buttons/buzzer without acquisition interference.
- Serial bursts and busy-time injection never produce a deferred OK activation;
  telemetry reflects real auxiliary readings and validity.
- Final-lead OPEN/SHORT wizard succeeds and survives reboot; factory OSL hash
  remains unchanged. Limited independent R/C/L holdouts check compensation and
  range/path coverage without recapturing the six factory LOAD groups.
- Normal measurement has one relay engagement/release and seven accepted primary
  samples; multi-frequency evidence remains available before classification.
- Record stack high-water/worst cooperative latency and exact deployed image/hash.

Accuracy/phase/loss/SNR/leakage, extreme endpoints including 10 mF, polarized
operating envelope, thermal qualification and undocumented upper-Flash endurance
remain **REQUIRES_BENCH_VALIDATION** until separately established. These tests
must not be reported as passed merely because a basic function works.

Completion requires updating each package's actual evidence/status, release
manifest, user controls/protocol/storage docs, remaining qualification list and
the authorized Git/PR. This audit commits a reviewable plan; A–L are still pending.
