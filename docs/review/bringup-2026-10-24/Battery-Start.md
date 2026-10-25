# Battery-powered passive bring-up — 2026-10-24

Status: BATTERY_ADC_AND_STANDARD_BRINGUP_BOOT_OBSERVED;
POWER_AND_CHARGER_QUALIFICATION_PENDING.

The owner connected an assembled charge-controller plus step-up supply, battery
powered, with ST-Link and the TTL adapter attached. The component models are
unknown. The owner reports all J_PWR signals connected and charger input off.
The fixture at the DUT terminals has not yet been reconfirmed; no DUT transaction
was requested or executed in this session.

## Executed operations

The preceding peripheral-only image was identified by full internal-Flash
readback before reading live auxiliary-sensor RAM. Its battery sample was
3.881 V nominal, state OK, age126 ms, with no auxiliary fault and K1/range/
excitation commands off. That read used halt/resume without reset.

The published **standard BRINGUP** image from clean source
`0dd1f3fe40ce7e9c68d563bcb80d405042396ead` was then programmed. Its65488-byte
internal-Flash readback matched SHA256
`e4af20eff8f4e06de291fd2c06e5de1437517ad0f0364da253a736f6f244876a`.
Target UID is `54ff70068370525760380567`, STM32 deviceID0x410,64KiB Flash.
The previous full Flash was backed up before erase. This is neither the
peripheral-only variant nor the external-bench-supply or K1-test variant.

With COM4 open at1152008N1, a separate controlled reset/run captured the complete
boot identity: BRINGUP0.1.0, Git0dd1f3fe40ce, SAFE_BOOT, HSE_PLL72MHz, SWD
preserved. Only passive Lab status commands followed; no relay/range/excitation,
capture, calibration-write or external-Flash program/erase command was issued.
The board remains running with the standard battery gates enabled.

## Observations

Five serial sensor snapshots reported battery3879/3883/3883/3878/3879mV,
raw2407/2409/2409/2406/2407, all stateOK, age25..400ms. VMID was1655..1658mV,
valid. Residual classification was SAFE, with sample age0..10ms. These are
**nominal3.3V ADC-scale readings**, not calibrated voltages or multimeter
comparisons. The amplified residual-network terminal estimates are not precise
measurements of the disconnected fixture's voltage.

Both MVP snapshots show K1 SAFE, range DISABLED, charger ABSENT and zero faults.
`BLOCKED_RANGE` is expected with no range selected; no measurement was admitted.
The startup `BLOCKED_SENSOR_INVALID` changed to `BLOCKED_RANGE` as sensor data
became available. ADC status was IDLE/lastOK. W25Q reported EF4016/4194304bytes.
Calibration is NO_VALID_CALIBRATION with schema2/model5/hardware0x00010002;
both slots are invalid/missing. No calibration was created.

NTC ADC snapshots were valid near100kOhm; fitted thermistor identity and actual
temperature were not established. TFT status changed from NOT_READY to READY;
that software status is not a new observation of physical display pixels.

See the [raw UART transcript](battery-start-uart.txt) and
[structured evidence](battery-start-status.json). Full deployment backup,
OpenOCD logs and UART event bytes remain locally under
`Firmware/build/battery-start-20261024/` (ignored build evidence).

## Next physical checks

- Compare VBAT_PROT, regulated+5V_SYS and3V3 against the owner's multimeter with
  charger input off; confirm empty DUT terminals before further fixture work.
- Observe charger ABSENT/PRESENT/ABSENT from the actual charger input signal,
  including software measurement blocking. The disconnected-input observation
  above does not test charger-presence response or independent relay inhibition.
- Separately qualify hardware inhibition, supply sequencing, battery thresholds,
  current/headroom, and unpowered/backfeed behavior. Debugger/TTL attachment means
  this session is not proof of standalone battery-only startup or power-off.

These checks remain REQUIRES_BENCH_VALIDATION until measured. No product release,
accuracy qualification,5.5V physical input test or whole power-system approval
is implied by successful ADC/serial communication.
