# A05 — calibration readiness and remaining physical gates

**SCHEMATIC_VERIFIED / FABRICATION_VERIFIED (export), not qualified hardware.**
The Rev.1 board is unassembled. A06.1/A06.2a/A06.2b host tests and linked firmware
establish software behavior; they do not establish relay operation, electrical
permissions, ADC scale, source amplitude, phase accuracy or physical W25Q recovery.
No accuracy/range/33-key usability claim is made by this audit.

## Can the present topology support A06?

| Workflow | Evidence that exists | Physical gate still required |
| --- | --- | --- |
| Factory-blank OSL fixture capture | PR12 shell uses existing calibration session, hardware FSM, residual/charger permits and ADC/DMA; no carrier GPIO mismatch found | R05/R06 and safe boot/sensors; genuine ADC provenance; valid capture/teardown trace with passive OPEN/SHORT/LOAD |
| PC serial link | PA9→J_UART.2, PA10→.3, GND→.1 verified; existing1152008N1/PLC1 bounded service | Correct3.3V adapter/cable, no power injection, real response CRC/request identity |
| External OSL installation | UFLASH CS=PA12, SPI2 bus and3V3 rails present; PR12 capture/install service and W25Q A/B transaction exist | Actual Winbond wideSS package, JEDEC/supply/erase/program/readback, reset recovery without unrelated partition loss |
| Safe fixture changes | K1NC routes terminals to SAFE; firmware teardown precedes extensive serial transfer | Contact continuity/bounce/inhibit and safe discharge; no energized/charged DUT |
| Full33-key attempts | Six RREF × three frequencies × two amplitudes minus three forbidden10Ω/500mV keys | Per-key current/amplitude/ADC-path/settling/leakage validation; invalid keys cannot be repaired by synthetic records |
| Standalone after provisioning | PRODUCT runtime loads full usable OSL from preserved externalW25Q; Resource Pack and boot prerequisites remain | SWD deploy PRODUCT, preserveW25Q, supplyresources if missing, physical boot/provenance/independent holdout test |

The carrier topology plausibly supports the migration once blockers are resolved.
No new PCB source change is justified merely by the existence of the PC workflow.
There is no USB firmware switch: USART1 is the service link; SWD changes the MCU
image. Keep module nativeUSB disconnected because PA11/12 serve K2/Flash. Debug
ground connection is not galvanic isolation or proof of charger-detect state.

## Foreseeable measurement limits

- **RREF and LOAD intervals:** preserve actual part/lot/value, resistor tolerance,
  temperature and independently measured interval. A 1% marking is not an exact
  absolute reference; a DMM or scope is not presumed more accurate without its own
  specifications/evidence. Measure fitted RREF before completing the switch bank
  when parallel circuit loading can be avoided. Do not desolder a qualified bank
  to chase nominal resistor labels without recording the resulting change.
- **Low R:** the two-wire path includes relay contacts, both MOSFETs, R0 link,
  copper and fixture contacts. SHORT recabling/reseating and actual transistor
  lot/drive affect the offset. 100mVrms into an ideal10Ω short implies10mArms/
  14.1mApeak; op-amp drive, distortion, resistor dissipation and transient current
  need measurement.500mVrms on10Ω is forbidden, not an optional qualification case.
- **High R:** 1MΩ with10pF has |ωRC|≈0.628 at10kHz; this illustrative parasitic
  already materially changes the complex response. Clamp/op-amp/MOSFET leakage,
  flux/humidity, cables and off-range capacitance require OPEN/holdout evidence.
  A typical10MΩ scope input is itself a significant parallel load on1MΩ; probe
  capacitance further changes the fixture. Remove probes for OSL data collection
  or explicitly model the unchanged fixture; do not calibrate with probes then
  claim the same unprobed domain automatically.
- **HG/1X:** nominal15.47× gain does not establish effective gain/phase. HG clips
  at substantially smaller RET difference than1X; flags and actual six-channel
  provenance must drive the existing solver/path checks. Reject an observation
  without a usable required path; never clear clipping or create overlap evidence
  to complete a campaign. ADC nominal scale remains explicitly uncalibrated until
  measured; the source's use of idealADC scale is a software fact, not a voltage
  calibration certificate.
- **Source and timing:** loaded RC reconstruction has considerable nominal10kHz
  attenuation/phase; U4 drive/headroom, quantization/ripple, U5 closed-loop phase,
  ADC sequential ranks/dual-ADC skew and aliasing require oscilloscope/raw-data
  evidence. Observe frequencies/amplitudes from the circuit rather than assuming
  PWM settings reproduce commanded DUT voltage exactly.
- **Residual and temperature:** SAFE's94kΩ bleeder and sensing bias are instrument
  loads, not intrinsic DUT leakage. NTC board temperature is not DUT temperature.
  Settled/valid supply and sensor state must precede permits; no DC leakage,
  DCR, transient, insulation or high-voltage feature is qualified here.

## Evidence campaign before releasing a usable domain

First demonstrate passive captures at a small number of benign central-domain
conditions, after the [physical checklist](bringup-checklist.md) passes. Record the
safe output timeline, supply currents, source/return waveforms, raw ADC codes,
six-channel phasors, clipping flags, nominal/actualADC provenance, temperature,
fixture identity and each capture SHA. Verify cancel, charger block and reset
without energizing DUTs or bypassing permits. Tests involving deliberately unsafe
residual input require a separately reviewed low-energy fixture and are **not
authorized by this checklist**.

Then test the six ranges and allowed100/1k/10kHz amplitude combinations individually.
Suggested repetitions from plan12:10 in-place captures and5 fixture reattachments,
including independent holdout resistors/R/C/L with stated intervals. Start with
central |Z|/RREF domains and expand only with evidence. Separate nonlinearity,
fixture variation, supply/noise, temperature and reference uncertainty. A low
fitting residual or successful serialization is not a worst-case accuracy bound.

The persistent full33-key schema must remain unchanged. Its structural completeness
does not qualify the entire matrix. Retain `REQUIRES_BENCH_VALIDATION` in all
synthetic or as-yet-unqualified reports, and keep independent physical evidence
with the campaign and installation readback. If a required electrical capture is
invalid, record the blocker and stop provisioning; do not fabricate coefficients
or self-assert QUALIFIED. PRODUCT resource admission/OSL boot gate remains intact.

## A05 completion boundary

Source/fabrication/BOM reconciliation and pre-assembly recommendations are complete
within the extractor's stated scope. Matching manufactured-board identity,
module/cell specifications, purchased part checks, RED resolutions, physical
continuity, staged power-up and actual assembler sign-off are still open. Full
native DRC/clearance checking also remains open. This is sufficient to prepare
physical inspection, not to sign off assembly or bench calibration.
