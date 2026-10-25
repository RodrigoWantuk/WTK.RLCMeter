# 14C — High-impedance integration investigation

STATUS: IN_PROGRESS — OPEN_PILOT_SAVED_LOAD_FIXTURE_PENDING

## Scope and prerequisites

Continue the owner-directed engineering calibration without the unavailable
Hantek; no scope qualification is inferred. The original source-7017e5e0 cohort
has all 99 artifacts, 30 valid fits and three high-Z/10-kHz missing-overlap fits.
The unit now runs verified source f86b7f4e, including the experimental UID-bound
NTC LUT and serial battery telemetry. New diagnostic artifacts must stay separate
from the original cohort; do not relabel its identity or temperatures.

Start with bounded observations using the unchanged capture API, before choosing
an integration implementation. No firmware, acquisition timing, DMA topology,
gain assumptions, validity flags, stability limits, calibration model or
installation coverage rule changes are authorized as incidental edits here.
Any longer-integration implementation needs a documented design amendment,
meaningful synthetic tests, strict embedded builds and a physical pilot.

## Diagnostic sequence

1. Read fresh battery and SAFE state over COM4; require charger absent and battery
   above the existing LOW threshold. Never substitute an old voltage.
2. With the last owner-confirmed OPEN fixture unchanged, acquire six separate
   OPEN artifacts at 1 Mohm/10 kHz/100 mVrms, without automatic retries.
3. Preserve all CCO1 bytes and raw UART evidence. Each artifact still represents
   the existing six accepted hardware transactions. Compare normalized 1X
   transfer and effective HG transfer across the six exported phasor means as a
   diagnostic only. Do not promote their flags or invent an integrated artifact.
4. Read battery again before the next resistor. Ask the owner for a fresh Minipa
   measurement and direct attachment of the approximately 1-Mohm resistor.
5. Acquire a bounded matching LOAD diagnostic using that actual fixture and
   compare repeatability/observability before selecting a corrective method.
6. Keep K1 SAFE between transactions, all real permits, residual/charger/battery
   checks, disabled-range address transitions, bounded cancellation and deadlines.
   Preserve W25Q, the original 99 artifacts and blank active calibration.

## OPEN pilot result

All six diagnostic artifacts were accepted on f86b7f4e, each six accepted/zero
rejected blocks with HG-only validity and no observed HG overlap. No flags were
changed. Across the six exported means, relative complex standard deviation is
6.91360% for normalized 1X transfer and 6.83089% for effective HG transfer. These
are repeatability diagnostics, not measured accuracy, qualified SNR or a
replacement for the embedded 2% criterion. No claim that a particular integration
length will fix the issue is justified by this small sample alone.

Battery was 3.949 V initially and 3.942 V at the final serial status; SAFE/idle,
zero safety faults, zero protocol errors, active calibration sequence zero.
The owner was asked to remeasure and attach the 1-Mohm resistor for step 5.
The remaining two blocked conditions and independent validation remain pending.

See [preserved pilot evidence](../Firmware/releases/2026-10-25/osl-integration-pilot/README.md).
