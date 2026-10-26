# First calibrated PRODUCT pilot — scheduling regression

Source 46df1fbb factory PRODUCT was programmed/readback-verified and booted.
The actual runtime loads active sequence 1 with all 33 records, resources OK,
display fault 0, safety faults 0, K1 SAFE, range/excitation OFF, no acquisition.
The latest pre-program battery was 3.873 V via COM4; no battery voltage was
read over SWD. Physical metadata reads are bounded and resume the idle core.

READY was blocked: presentation state 21, raw blockers 0x0C. The range was
actually disabled, whereas the UI demanded a READY range before scheduling its
preparation. Separately, a residual metadata read showed valid VMID, UNKNOWN,
safe count 0 and age 79 ms versus the 50-ms limit. Cooperative auxiliary ADC
progress was interleaved with slow TFT rendering. The factory outer guard also
needed to distinguish the hardware transaction's expected ADC/range transitions
from actual hard blockers. No resistor or other DUT measurement was commanded.

This is preserved failed pilot evidence, not a READY/accuracy qualification.
Plan 20 fixes application scheduling only, keeping hardware permits, thresholds,
ADC/DSP, stored OSL, assets and settings unchanged. The physical-evidence archive
contains exact firmware, program/boot/read-only diagnostic records and procedures.
