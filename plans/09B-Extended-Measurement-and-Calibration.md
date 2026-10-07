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
  the 33 Rev.1 OSL condition identities, safe capture provenance, and exact supported
  frequency matching. It produces **diagnostic JSON only**, not a W25Q calibration
  record or a qualified correction. Current OSL evidence itself is not imported or
  verified by this tool.
- The existing PRODUCT calibration schema/model and A/B transactional slots are
  unchanged. No DC or supplementary-curve coefficient is installed at runtime.

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

1. **Space and ownership:** PRODUCT Release starts at 61396 B Flash against its
   61440 B project gate; BRINGUP starts at 62296 B against physical 65536 B. Audit
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
4. **Supplementary calibration:** the PC must ingest authenticated/traceable
   completed captures and the active OSL set, then solve all current standards
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
