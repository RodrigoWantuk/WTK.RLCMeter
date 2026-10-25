# First-board consolidation — 2026-10-24

Status: INTEGRATION_VERIFIED; SOURCES_PUBLISHED. Owner-authorized current
firmware/documentation/PCB consolidation. Historical A05 observations retain
their original scope and are not retroactive bench certificates.

## CHG_VBUS up to5.5V

October24 source/BOM/Gerber agree: J_PWR.4 feeds RCHG_H10k, PA15/CHG_DETIO and
RCHG_L12k toGND. A separate RUSB_B10k drives QUSB_INH BC817-25 base; its emitter
isGND and collector isK1_BASE. With positive0..5.5V DC, commonGND and normal3.3V
board power, this detection input is dimensioned to accept5.5V. PA15 is nominal
2.727V at5V and3.000V at5.5V. Allowing both divider resistors±5% conservatively
gives3.136V maximum before small input-leakage error. PA15 is FT; firmware frees
JTAG and uses floating digital input without internal pulls. See ST's operating
specifications in [DS5319 Rev20](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf).
This is a circuit/specification check, not an executed5.5V physical test.

RUSB_B limits forward base current to roughly0.5mA and resistor power below3mW.
The base is forward-biased through10k, not subjected to5.5V reverse emitter-base
voltage. See [Diodes BC817-25](https://www.diodes.com/_files/datasheets/ds11107.pdf).
Independent relay inhibition still requires physical verification. This rating
is for pin4 detection; pin2 system power remains regulated nominal5V. Real charger
input VBUS is required; boost output is not charger-presence evidence. Negative
input, overshoot, ESD and unpowered/backfeed behavior are not qualified here.
Fitted Blue Pill/charge-boost power-off behavior must be checked separately.

## Hardware recovery

- Owner repaired a COUT solder bridge;5V/3.3V subsequently reported stable.
- All six10k RGS are gate-toGND, eliminating the measured gate-driver DC path into
  RET. Current CAD has pad1GND/pad2GATE. First-board lifted former-source resistor
  ends share aGND bus; SRC nodes must not be grounded. Static recovery/basic R/C/L
  captures are observed; negative-source VGS and switching/power transients remain
  REQUIRES_BENCH_VALIDATION.
- Owner fitted a passive5V buzzer and confirms tones. BOM still names a9–15V
  indicator; the exact production substitute is unresolved.
- Display-flat solder repair resolved white screen. Owner confirms portrait180,
  rendering and buttons/tones. LCD returns MADCTL88/nonzero status, with zero04/D3
  IDs; exact controller identity remains unknown.
- Fitted Flash returns EF4016/4MiB; BOM baseline is W25Q64. Existing storage layout
  is capacity-relative and supports this density.8MiB examples are not actual
  board addresses. Read-only identification is not write/erase qualification.

[hardware-audit.json](hardware-audit.json) records current artifact hashes and
reconciliation:149 components,434 pad connections/top flashes,84 component drills,
115 connected nets. No old RSCK position exception is needed. Native CAD DRC,
creepage and physical continuity remain distinct checks.

## Firmware integration

Every metrology profile shares corrected ADC2 EXTTRIG/software-trigger selection,
TIM2CC2 with PA1 analog/unremapped, disabled-ADC sequencer/DUALMOD setup, guarded
CR2 stop avoiding unchanged ADON writes, K1 operate30ms and bounded atan2 phase.
All256 samples remain intact; none is silently discarded.

Every TFT profile shares50ms reset/250ms release wait, common DCS initialization,
MADCTL88/portrait180 rotation2 and TFT PCLK1/64 (562.5kHz at36MHz). Flash stays/8.
BRINGUP_CAL remains headless. Rendering is chunked without a framebuffer.
No acquisition schedule or analog voltage setting changes.

Calibration hardware identity is0x00010002/65538 for RGS-toGND. Schema2/model5
and OSL equations stay unchanged. C and PC tools agree; old0x00010001 records are
rejected, not erased or re-labelled. Acquire calibration on the reworked board.
Regression rejects an old-hardware frame with validCRC.

Bench-supply, one-shotK1 and peripheral tests remain explicit BRINGUP variants.
PRODUCT keeps residual/charger/battery/clock/fault gates, quiet mode and valid
calibration requirements. This task updates/builds/pushes sources; it does not
flash production over the confirmed peripheral image on the board.

## Verification and remaining release work

44/44 CTests passed;486 Python tests ran with99 environment skips;18 PCB tests
passed. Embedded PRODUCT Release/Debug, factory Release/Debug, BRINGUP,
BRINGUP_CAL, bench-supply, K1 and peripheral builds pass hard memory/composition
checks. Existing soft-size/LTO advisories remain. Build hashes/sizes are recorded
in [the source-bound manifest](../../../Firmware/releases/2026-10-24/firmware-builds.json).
The [nine-image bundle](../../../Firmware/releases/2026-10-24/README.md) is built
from clean source0dd1f3f and included in Git with the owner PCB replacements.

Buildable integrated sources are ready for subsequent calibrated product work.
Physical release still needs battery/charger wiring/sequencing, CHG software and
independent inhibition/power-off checks, post-rework OSL and metrology/range/
temperature qualification, low-range/current/transient checks, Flash writes/
resources/settings/power-loss, exact buzzer/backlight records and a validated
polarized-capacitor method/domain. No invalid-calibration or battery bypass is
promoted into PRODUCT. Compilation does not certify measurement accuracy.

See [basic checks](../a05/Basic-Functions-Before-Final-Assembly.md),
[display evidence](../a05/Display-White-Screen-2026-10-24.md) and
[plan17](../../../plans/17-Bringup-Consolidation-and-Publish.md).

## Subsequent battery-powered session

After publication, the owner connected the assembled battery supply and the
standard BRINGUP image was programmed/readback-verified and booted through COM4.
Battery ADC readings3878..3883mV/OK, chargerABSENT, K1SAFE and zero faults were
observed. The owner subsequently reported VBAT_PROT3.9V, +5V_SYS5.1V,3V3 3.3V,
CHG_VBUS0.0V and empty DUT terminals. See
[the battery-start evidence and multimeter comparison](Battery-Start.md);
broader power-system qualification remains pending. After an initially missed
connection and owner setup correction, passive UART endpoints confirmed
ABSENT/PRESENT/ABSENT, BLOCKED_CHARGER only while present, K1SAFE and zero faults.
The initial unsuccessful test is preserved in the same report. Independent
hardware relay inhibition and on-state CHG_VBUS voltage remain unqualified.
