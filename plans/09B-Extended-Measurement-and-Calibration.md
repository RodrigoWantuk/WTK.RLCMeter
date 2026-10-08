# Rev.1 Extended Measurement and Calibration

STATUS: IN_PROGRESS

This work follows the existing AC, OSL, safety, product UI, and resource contracts.
It must not reclassify synthetic tests as physical qualification.

## Current implementation boundary

- AC acquisition, OSL calibration, automatic range policy, and product results remain
  the only executable PRODUCT measurement path.
- `measurement_dc.c` contains host-tested DC divider mathematics and a pure pilot
  prerequisite check. It is compiled for Arm but not called by an application transaction, cannot
  energize K1, and cannot authorize a DC measurement.
- `tools/calibration_campaign.py` is a host-only interval-fit prototype. It requires
  the 33 Rev.1 OSL condition identities and exact supported frequency matching. Its
  CLI now binds each standard to one BRINGUP RAW file by SHA-256, checks condition,
  permit/relay metadata, active-calibration sequence, and the dump's persisted-OSL
  DSP result. A supplied active-set frame is checked for commit, CRC, Rev.1 model,
  all 33 unique condition keys, finite/nondegenerate OSL coefficients, and the same
  sequence as every RAW. The fit remains **diagnostic JSON only**, not a W25Q
  calibration record or a qualified correction. The supplied frame is not
  authenticated, does not prove physical OSL fixtures, and the PC does not
  independently reproduce the embedded OSL calculation. No COM export of the
  active frame exists yet.
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

Before any board-executable DC transaction, the electronics owner must provide and
bench-verify continuous/peak source-current limits, output/series-path dissipation,
allowed bias and dwell time, ADC/offset resolution, settling criteria, capacitor
polarity policy, and an abort/discharge sequence. The op-amp absolute or typical
short-circuit ratings are not operating-current permissions. Until then, both
PRODUCT and BRINGUP DC execution remain disabled.

## Remaining implementation gates

1. **Space and ownership:** PRODUCT Release is now 61344 B Flash against its
   61440 B project gate (96 B headroom); BRINGUP is 62356 B against physical
   65536 B. Audit
   the map, remove duplication without weakening safety or blank-W25Q recovery,
   and remeasure before adding target code. Do not raise the physical limits.
2. **DC transaction:** after the electrical contract, add a distinct Phase 05
   cooperative static-bias path. Every attempt obtains a fresh Phase 04/05 permit,
   begins at 1 MOhm, measures VEXC/RET/VMID rather than trusting PWM duty, and
   returns K1 SAFE before analysis. Charger, residual, ADC, saturation, timeout,
   and cancellation faults use the existing emergency cleanup. Requalify residual
   evidence after release. Do not infer safety from AC impedance.
3. **DC claims:** DCR is conditional on validated two-wire offset/contact resolution.
   Low-voltage leakage is exploratory until an open fixture, dwell, bias, and
   uncertainty are characterized. It is not datasheet insulation resistance.
4. **Supplementary calibration:** the PC can check SHA-bound completed RAW captures
   and a separately supplied active OSL frame structurally. It must still obtain
   that frame from the device, prove its active identity, and solve all standards
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
