# 14C — High-impedance integration investigation

STATUS: IN_PROGRESS — NEW_SOURCE_33_OPEN_COMPLETE_SHORT_FIXTURE_PENDING

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

## LOAD amplitude comparison

The owner freshly reported 996 kohm and confirmed the resistor connected alone.
The original nominal 1 Mohm +/-1% uncertain reference is retained; the DMM reading
is recorded as a consistency observation, not a certified exact impedance.
Fresh serial battery was 3.947 V before the new LOAD series. Six independent
artifacts at 100 mVrms and six at 500 mVrms were collected with the same resistor,
source f86b7f4e and unchanged hardware/acquisition/stability logic.

| Fixture | Amplitude | Relative complex SD, normalized 1X | Relative complex SD, effective HG |
| --- | --- | --- | --- |
| OPEN, previous six exported means | 100 mVrms | 6.91360% | 6.83089% |
| LOAD, six exported means | 100 mVrms | 4.53700% | 4.66066% |
| LOAD, six exported means | 500 mVrms | 1.94897% | 1.90355% |

These are dispersions across exported six-block means, not the within-artifact
statistics used by the firmware. Every LOAD artifact remains HG-only, without
observed HG overlap; the diagnostic 1.9% number does not promote any path flag.
Improvement with amplitude supports investigating low-signal observability but
does not isolate ADC, VMID, analog noise, interference or another cause. No
longer-integration length or production accuracy is qualified by these results.

All 12 LOAD artifacts are preserved separately, zero safety faults/protocol
errors, SAFE teardown. Final battery is 3.934 V by serial, charger absent and
active OSL sequence zero. No calibration candidate, W25Q write or firmware change
occurred during these pilots. The original source-7017e5e0 campaign is untouched.

Before selecting a corrective implementation, complete a matching six-artifact
OPEN series at 500 mVrms on f86b7f4e. The owner was asked to remove the resistor
and confirm completely empty terminals, without RESET. This is an explicit
fixture dependency; do not start the OPEN capture until that reply arrives.

The owner replied "open". Six matching OPEN artifacts at 500 mVrms completed:
normalized 1X dispersion 2.45374%, effective HG dispersion 2.42160%, all HG-only
with no observed overlap. Final serial battery 3.941 V; SAFE/idle, no faults or
protocol errors. All four diagnostic series remain separate from calibration.

## Explicit corrective design amendment

Implement a CAL-only grouped coherent integration pilot; host builds include it
for tests, PRODUCT/ordinary BRINGUP compile it out. Each physical block remains
the original 256 samples/3072-byte DMA buffer and a complete existing hardware
transaction, including its real permit, 30-ms operate guard, SAFE teardown,
auxiliary restoration and fresh safety checks before the next transaction.
No ADC clock, sample rate, DMA topology, relay guard or analog threshold changes.

Align the six observed GND-referenced complex phasors by the common source phase
and average them before recomputing transfer, impedance and HG observability.
Do not average impedance ratios, fabricate overlap or recover a clipped stream;
OR clipping over the entire group. Six independent groups still have to satisfy
the existing 2% workflow criterion, with at most ten group attempts. Fixed
integration lengths: 96 physical blocks/group for 1 Mohm/10 kHz/100 mVrms;
16 for 1 Mohm/10 kHz/500 mVrms and 100 kohm/10 kHz/100 mVrms; one otherwise.
These are conservative pilot choices, not a validated noise model.

The original 20-second global timeout is insufficient for hundreds of complete
SAFE transactions. Explicitly extend only integrated calibration captures to a
90-second bounded deadline; ordinary captures retain 20 seconds and all hardware
transaction deadlines are unchanged. IDENTIFY advertises the maximum; the updated
client uses the selected condition's quiet window and rejects explicitly short
timeouts before START. Charger/fault/cancellation still drain the hardware FSM.

Advertise capability bit6 and emit CCO2/API2/196-byte artifacts in this service
mode, so old clients reject the new format. Preserve offsets 0..183; add u16
blocks/group at184, physical attempts at186, accepted physical blocks at188,
reserved zero at190, CRC32 at192. Counts40..42 now explicitly count groups.
Legacy fixtures/clients keep CCO1/188-byte behavior. Capture API framing, OSL model5,
ADC nominal scaling and installed-record schema remain unchanged. A new source
cohort is mandatory; never rewrite old artifacts or silently mix firmwares.

Expected files: build config/CMake gating; a bounded app integration helper;
calibration workflow phasor reconstruction entrypoint; session grouping and
cancel/error reset; capture service provenance/deadline/format; CAL shell opt-in;
Python decoder and quiet-window negotiation; meaningful C/Python tests and protocol
docs. Verify phase rotation, ratio-of-means, clipping preservation, partial-group
cancellation, fault/timeout teardown, exact physical counts, legacy parsing and
old-cohort compatibility. Run all host checks and strict nine unit builds with
the existing UID-bound NTC profile. Program only a verified image, then pilot the
three OPEN conditions before choosing any reacquisition or installation step.

## Verified software and deployment

Source a52dba35c74e0643a8b15d916eb29c427d917f77 passes all46 host CTest entries,
including the real-C coherent protocol tests, phase/ratio-of-means/clipping math,
partial-group cancellation, charger abort, deadline drain and legacy cohort
compatibility. All nine UID-bound NTC builds pass strict warnings and unchanged
memory gates. BRINGUP stays65,512 bytes Flash; CAL is37,424 bytes Flash and13,740
bytes accounted RAM. The service context is1072 bytes (+72); the single3072-byte
raw workspace and2048-byte reserved stack are unchanged.

The owner-confirmed OPEN fixture remains unchanged. The verified CAL image was
programmed and read back: BIN SHA256
ed9c8dcd6f351e10d248bfe48d3b6b73c5c7e26190e9fb5a8542115e28d12765;
full64-KiB readback SHA256
d90c04efbbd13be1c0b4fe017f6e05a33b105b4e53c128e2a95c24732ce37408.
COM4 identifies a52dba35, capabilities127, maximum90000-ms capture deadline;
SAFE/idle, fault/protocol counters zero, battery3.926 V nominal, active OSL absent.
External W25Q was not programmed. Three explicit CCO2 OPEN pilot keys are being
acquired; physical overlap and corrected OSL feasibility remain to be demonstrated.

## Physical coherent OPEN pilot

All three previously blocked keys now have actual stable 1X and HG evidence and
observed effective HG overlap on source a52dba35. Each has six accepted groups,
zero rejected groups, no clipping, original 2% gates, SAFE teardown and zero
safety/protocol errors. Physical blocks are96,576,96 for the100-kohm/100-mV,
1-Mohm/100-mV and1-Mohm/500-mV keys respectively. This proves OPEN observability
for these observations, not complete OSL fits or physical accuracy.

The final pilot battery was3.931 V over COM4; OSL remains blank. The three exact
CCO2 artifacts seed a new same-image OPEN cohort. With the owner's OPEN fixture
unchanged, the remaining30 keys are being acquired without repeating those three
or mixing any older source. Complete new-source SHORT/LOAD and strict solver
checks are still required before candidate generation or installation.

## Complete same-source OPEN checkpoint

All33 source-a52dba35 OPEN conditions are now accepted. The three exact pilot
artifacts were retained unchanged and the other30 keys were acquired once on the
same image and unchanged owner-confirmed empty fixture. The full cohort contains
198 accepted groups and948 accepted physical blocks/attempts, no rejected groups.
Every condition retains actual usable paths; clipped HG streams remain excluded
where 1X is usable. In particular all three difficult10-kHz OPEN keys have both
paths and observed HG overlap. CRC/SHA, identity, ADC provenance, unique keys and
exact artifact metadata were rechecked before archiving.

Final serial status: battery3.920 V nominal, SAFE/idle, fault0, protocol0,
charger absent, active OSL sequence0. Settings/resources/calibration W25Q partitions
were not programmed. Source7017 and f86 cohorts remain unchanged and separate.
The owner was asked to connect a short, firm copper jumper directly between the
terminals without RESET. SHORT capture must wait for that explicit reply.
Current next stage is33 same-image SHORT keys, then freshly measured LOADs.
No complete OSL candidate or installed calibration exists yet.

See [preserved pilot evidence](../Firmware/releases/2026-10-25/osl-integration-pilot/README.md).
