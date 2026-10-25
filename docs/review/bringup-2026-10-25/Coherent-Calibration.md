# Coherent calibration recovery — October 25, 2026

Status: THREE_OPEN_PILOTS_PASS; NEW_SOURCE_OPEN_IN_PROGRESS; OSL_NOT_INSTALLED.

The owner remeasured the approximately1-Mohm resistor as996 kohm and confirmed
direct attachment alone. Six independent LOAD observations at100 mVrms and six
at500 mVrms on f86b7f4e showed effective HG transfer dispersions of4.66066% and
1.90355% across exported six-block means. The matched OPEN dispersions were
6.83089% and2.42160%. All original artifact flags remained HG-only. These are
repeatability diagnostics, not measured accuracy or identification of a faulty
component. The DMM is a consistency reference, not a certified exact impedance.

The owner then confirmed empty terminals. The CAL-only correction groups
complete independently permitted hardware transactions, aligns the six raw
phasors by a shared source phase and derives ratios from their means. It never
averages per-block impedance ratios or clears clipping. Every physical block
retains the original256-sample/3072-byte workspace, hardware timing/guards,
residual/charger/battery checks and SAFE teardown. Six independent groups still
have to pass the original2% criterion. Only integrated keys have a bounded
90-second global timeout; normal keys retain20 seconds. CCO2 explicitly records
group length and physical counts; old clients reject the new artifact format.

Source a52dba35 passes46 host tests and nine strict unit-profile builds. CAL uses
37,424 bytes Flash/13,740 bytes accounted RAM; the service context is1072 bytes.
Ordinary BRINGUP stays65,512 bytes Flash. The UID-bound experimental NTC table is
preserved. Internal Flash was programmed and read back; external W25Q was not
programmed. COM4 identifies the expected UID/hardware/model with capabilities127.

All three formerly blocked OPEN conditions now pass with actual stable1X and
HG paths and observed HG overlap:100 kohm/10 kHz/100 mVrms and1 Mohm/10 kHz at
100/500 mVrms. Their physical block counts are96,576,96; each has six accepted
groups, zero rejects and clipping, SAFE teardown, zero safety/protocol errors.
The final serial battery reading was3.931 V, nominal scaling.

These three exact artifacts seed a new same-image source-a52dba35 OPEN cohort,
whose other30 keys are in progress on the unchanged empty fixture. Historical
source7017 and f86 observations remain separate. New-source SHORT/LOAD,
float32/C solver checks, candidate installation, independent holdouts and scope
qualification remain pending. No complete calibration or accuracy is claimed.

See [unit image and physical evidence](../../../Firmware/releases/2026-10-25/coherent-calibration/README.md),
[pre-correction diagnostics](../../../Firmware/releases/2026-10-25/osl-integration-pilot/README.md),
[protocol](../../../Firmware/tools/PC_CAL_CAPTURE.md) and
[execution plan](../../../plans/14C-High-Z-Integration-Investigation.md).
