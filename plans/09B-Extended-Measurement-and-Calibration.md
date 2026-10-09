# Rev.1 Extended Measurement and Calibration

STATUS: IN_PROGRESS

This work follows the existing AC, OSL, safety, product UI, and resource contracts.
It must not reclassify synthetic tests as physical qualification.

## Current implementation boundary

- AC acquisition, OSL calibration, automatic range policy, and product results remain
  the only executable PRODUCT measurement path.
- BRINGUP now has an experimental `lab dc pilot` transaction. It first captures AC
  at 1 MOhm / 1 kHz / 100 mVrms, returns K1 SAFE, waits for fresh residual-safe
  qualification, then starts a separate Phase 05 DC transaction with a new
  single-use permit. TIM1 stays at the 450 kHz carrier with static CCR1=81
  (neutral=80, one nominal 20.625 mV PWM step); the DC DMA capture uses the
  existing 256-instant/64 kHz profile. K1 SAFE, excitation OFF, range disabled,
  ADC restored, and quiet released precede analysis and UART output. Cancellation
  uses the same Phase 05 abort path. PRODUCT has `WTK_ENABLE_DC_PILOT=0`.
- The DC analyzer averages measured VEXC/RET_1X/VMID ADC codes, rejects any
  individual rail-clipped sample, rejects source bias above 50 mV or reversed
  polarity, and requires four nominal ADC codes of source/current/DUT resolution.
  These are provisional software guards, not accuracy or current qualification.
  The pilot cannot resolve low-ohm DCR with 1 MOhm RREF; it reports explicit
  unresolved states. `tools/pc_dc_pilot.py` can request one controlled BRINGUP
  pilot, validate its bounded result frame, and retain SHA-256 evidence.
- `tools/calibration_campaign.py` is a host-only interval-fit prototype. It requires
  the 33 Rev.1 OSL condition identities and exact supported frequency matching. Its
  CLI now binds each standard to one BRINGUP RAW file by SHA-256, checks condition,
  permit/relay metadata, active-calibration sequence, and the dump's persisted-OSL
  DSP result. A supplied active-set frame is checked for commit, CRC, Rev.1 model,
  all 33 unique condition keys, finite/nondegenerate OSL coefficients, and the same
  sequence as every RAW. The fit remains **diagnostic JSON only**, not a W25Q
  calibration record or a qualified correction. The supplied frame is not
  authenticated, does not prove physical OSL fixtures, and the PC does not
  independently reproduce the embedded OSL calculation. BRINGUP can now export
  a canonical serialization of its active in-RAM OSL set over COM through the
  shared workspace; this is not byte-exact physical-slot readback.
  Capacitor ESR and D/tan-delta may now each be absent, bounded by a maximum,
  or supplied as a nominal value with explicit tolerance. Same-condition
  intervals are fitted jointly and incompatible intervals fail as conflicts;
  out-of-band values are reported as ignored, not extrapolated.
- `tools/pc_campaign_add.py` now creates an individual standard entry from a
  completed RAW capture and revalidates the full candidate campaign against the
  supplied OSL frame before atomically saving JSON. It supports FIT and held-out
  VALIDATION entries plus optional loss specifications. This removes manual
  condition/SHA transcription but does not authenticate the board, source
  temperature, or physical reference component, and it does not transfer a
  correction to the MCU.
- `tools/pc_capture.py` passively collects a completed BRINGUP RAW v1 DUT dump over
  COM, validates its framing, 256 rows, timing metadata, and prints a SHA-256 identity
  of the exact bytes written (including on Windows).
  It does not command the relay or substitute for an authenticated PRODUCT capture
  export or calibrated OSL processing.
- The existing PRODUCT OSL calibration schema/model and A/B transactional slots
  are unchanged. No DC coefficient is installed; supplementary curves use separate
  reserved W25Q slots and are inert without explicit qualification and installation.
- The PC campaign report now separates `FIT` standards from optional held-out
  `VALIDATION` standards. Validation never changes the fit; a failed held-out
  interval sets `HOST_VALIDATION_FAILED` and makes the CLI exit nonzero.
  Each standard now has inspectable corrected complex impedance, derived SI
  value, declared tolerance interval, and a pass/fail result across all of its
  active constraints. Conflicting FIT groups identify their condition and sample
  IDs; malformed or non-finite constraints fail explicitly. This is review
  evidence, not an uncertainty estimate or an installable calibration record.
  An in-band inductor `Q_min` constraint now also requires nonnegative series
  resistance, so a negative-R fit cannot satisfy Q spuriously.
  Constraint rank is reported out of 12
  per condition. `FULL_LINEAR_SPAN_UNQUALIFIED` is only a necessary span check,
  not a bounded-uncertainty or physical-accuracy claim. All output remains host
  diagnostic JSON, not an installable correction.
- PRODUCT detail rendering now shows R (or capacitor-model AC ESR when its
  real part is nonnegative), X, |Z|, and phase, with `n/a` for an invalid DSP
  result. Q and D remain DSP outputs but are not on the PRODUCT page yet.
- A shared R/X details-row renderer saved 52 B in PRODUCT Release (61396 to
  61344 B). Sharing decimal append logic offsets most of the new detail page:
  PRODUCT Release is now 61428 B and 17028 B accounted RAM after adding mOhm
  formatting for sub-0.1-Ohm values. This leaves only 12 B against the
  61440 B Flash gate, so substantial embedded features still
  require code-size recovery. No safety gate was removed.
- A versioned post-OSL complex-plane curve substrate now exists for the exact
  Rev.1 range/frequency/amplitude condition. Each sparse condition record has
  three log-|Z|/RREF knots and a 2x2 real matrix acting on complex Z. The target
  applies it only after the Phase 05 SAFE teardown and OSL DSP, then recomputes
  derived values; it never changes measurement permission, GPIO, or the OSL set.
  A two-slot W25Q reader selects a CRC-checked, committed, explicitly qualified
  overlay bound to the active OSL sequence and frame CRC. It reads one bounded
  record per result, without a second acquisition buffer. Missing/out-of-domain
  records retain OSL-only output; a corrupt previously active record rejects
  that result. Unqualified candidates are never applied.
- The PC campaign report now records the supplied OSL frame CRC. The separate
  `calibration_curve_frame.py` builder emits and inspects **unqualified** v1
  candidate frames only after SHA-bound FIT and held-out checks. No ordinary
  uploader, qualification ceremony, or transactional writer exists for these
  slots yet. A host PASS is not physical qualification. The 33-condition OSL
  record and its existing A/B slots remain unchanged.
- A scale-sensitive complex-magnitude bug in the DSP was fixed: large impedances
  no longer use an insufficient fixed-iteration square-root seed. Host regressions
  cover this and the new curve, frame, and A/B reader. Rejected final sessions
  cannot expose an earlier valid primary as a fresh numeric UI result.
- PRODUCT's project Flash gate is temporarily 63 KiB, with maintainer approval;
  the silicon limit remains 64 KiB. Release uses 64092 B and has only
  1444 B of physical margin. This is a blocking size debt for further
  PRODUCT additions, not a new comfortable budget. PRODUCT Debug enables LTO to
  remain linkable under the same physical limit.

## Hardware qualification gate for DC

The Rev.1 RREF bank provides the only firmware-selectable series resistance. No
additional protective resistor can be switched into the existing PCB path. A prior
AC measurement is useful triage but cannot exclude a DC short, notably for an
inductor. The DC pilot must begin with 1 MOhm and a separately authorized static
bias step. It must never lower RREF based on AC impedance alone.

The BRINGUP 1 MOhm pilot is software-bounded but **not bench qualified**. It may
be attempted only with a current-limited bench supply and a controlled DUT;
charged or polarity-sensitive capacitors must not be connected. Nominal PWM
step and RREF alone do not establish actual VEXC, settling, contact resistance,
or leakage. No automatic lower-RREF progression exists. Continuous/peak source
current, dissipation, bias, dwell, ADC/offset resolution, capacitor polarity,
and discharge behavior remain REQUIRES_BENCH_VALIDATION. The op-amp short-
circuit rating is not an operational current permission. PRODUCT DC stays off.

## Remaining implementation gates

1. **Space and ownership:** PRODUCT Release is near the physical 65536 B Flash
   limit under a temporary 64512 B project gate; BRINGUP is also near physical
   capacity. Audit the map and recover margin before more target features. Do not
   weaken safety or blank-W25Q recovery or raise the physical limits.
2. **DC expansion:** the BRINGUP-only 1 MOhm transaction exists, but its
   electrical behavior and host-orchestrator timing need board validation. Do
   not enable PRODUCT, lower RREF, or label the output DCR until a documented
   current/dwell/contact-offset/error budget and controlled bench tests exist.
3. **DC claims:** DCR is conditional on validated two-wire offset/contact resolution.
   Low-voltage leakage is exploratory until an open fixture, dwell, bias, and
   uncertainty are characterized. It is not datasheet insulation resistance.
4. **Supplementary calibration:** the versioned sparse candidate format, pure
   curve application, and qualified A/B runtime reader exist. The PC must still
   prove physical OSL slot identity/provenance, jointly solve campaign constraints
   with bounded uncertainty, establish a bench qualification procedure, and add
   a transactional candidate-transfer/validate/commit writer. The prior active
   coefficients are a soft initial point only. OSL-only records remain valid;
   do not install a fitted curve solely on host held-out success.
5. **PC transport/UI:** extend the framed COM protocol for read-only capture/evidence
   export and candidate transfer/validate/commit with sequence, CRC, busy handling,
   rollback, and no resource/calibration mutation overlap. Keep product measurement
   menu-driven; the PC owns the unbounded campaign input/history, while the device
   stores only one logical active coefficient set in redundant physical slots.
6. **Presentation:** R/X/|Z|/phase and conditional capacitor-model AC ESR are now
   visible; add conditional Q/D and explicit quality/condition metadata after
   recovering Flash headroom. Do not present ESL, quantified SRF, or
   high-voltage insulation resistance from Rev.1. UI text/art stay in W25Q; no
   full framebuffer or hidden UART dependency in normal measurement.

## Acceptance evidence

Host tests must cover compatible/incompatible interval standards, optional ESR/D,
out-of-band datasheet values, current-campaign precedence, DC near-open/short and
invalid samples, and every transaction cleanup path. On hardware, use a current-
limited supply and controlled open/short/known standards before lowering RREF or
making product DC available. Compare DCR and leakage against an independent
instrument; characterize the fixture and temperature. All electrical limits,
loss-model fit, thermal behavior, and accuracy remain
`REQUIRES_BENCH_VALIDATION`.
