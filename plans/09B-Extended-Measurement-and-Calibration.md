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
- `tools/pc_capture.py` passively collects a completed BRINGUP RAW v1 DUT dump over
  COM, validates its framing, 256 rows, timing metadata, and prints a SHA-256 identity
  of the exact bytes written (including on Windows).
  It does not command the relay or substitute for an authenticated PRODUCT capture
  export or calibrated OSL processing.
- The existing PRODUCT calibration schema/model and A/B transactional slots are
  unchanged. No DC or supplementary-curve coefficient is installed at runtime.
- A shared R/X details-row renderer saved 52 B in PRODUCT Release (61396 to
  61344 B, same 17028 B accounted RAM), but this is not sufficient headroom for
  substantial new embedded features. No UI capability or safety gate was removed.

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

1. **Space and ownership:** PRODUCT Release is now 61344 B Flash against its
   61440 B project gate (96 B headroom); BRINGUP is 62800 B against physical
   65536 B. Audit
   the map, remove duplication without weakening safety or blank-W25Q recovery,
   and remeasure before adding target code. Do not raise the physical limits.
2. **DC expansion:** the BRINGUP-only 1 MOhm transaction exists, but its
   electrical behavior and host-orchestrator timing need board validation. Do
   not enable PRODUCT, lower RREF, or label the output DCR until a documented
   current/dwell/contact-offset/error budget and controlled bench tests exist.
3. **DC claims:** DCR is conditional on validated two-wire offset/contact resolution.
   Low-voltage leakage is exploratory until an open fixture, dwell, bias, and
   uncertainty are characterized. It is not datasheet insulation resistance.
4. **Supplementary calibration:** the PC can check SHA-bound completed RAW captures
   and a BRINGUP-exported active OSL set structurally. It must still prove the
   physical slot identity and provenance, and solve all standards
   jointly. The prior active coefficients are a soft initial point only. Define a
   bounded, versioned runtime correction overlay and transactional W25Q format;
   migrate older OSL-only records without losing the last valid set. Do not install
   a fitted curve before cross-validation with unused standards and bench evidence.
5. **PC transport/UI:** extend the framed COM protocol for read-only capture/evidence
   export and candidate transfer/validate/commit with sequence, CRC, busy handling,
   rollback, and no resource/calibration mutation overlap. Keep product measurement
   menu-driven; the PC owns the unbounded campaign input/history, while the device
   stores only one logical active coefficient set in redundant physical slots.
6. **Presentation:** expose AC R/X/|Z|/phase, model-valid C/L, and conditional
   ESR/Rs/Q/D with condition and quality. Do not present ESL, quantified SRF, or
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
