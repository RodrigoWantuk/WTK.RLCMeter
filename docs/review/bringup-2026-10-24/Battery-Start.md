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

## Owner multimeter comparison, charger input off

The owner subsequently confirmed empty DUT terminals and reported these DC
measurements against GND: VBAT_PROT **3.9 V**, +5V_SYS **5.1 V**, 3V3 **3.3 V**,
and CHG_VBUS **0.0 V**. Instrument identity, uncertainty and additional displayed
digits were not supplied. The battery reading is consistent with the nominal
ADC indication at the precision reported; no calibration coefficient or accuracy
claim can be derived from this comparison.

A fresh no-reset passive COM4 session returned battery3878/3876/3870mV, all OK,
VMID valid, residual SAFE, zero faults, charger ABSENT, K1 SAFE and ranges
DISABLED. No active command was issued. See
[the owner comparison and passive snapshot](battery-power-off-dmm.json) and
[its UART transcript](battery-power-off-dmm-uart.txt). The regulated5.1V and3.3V
readings establish the reported DC levels at this operating point; ripple,
current capacity and load/transient behavior remain unmeasured.

## Charger connected, presence not detected

The owner then reported connecting the charger, but could not take multimeter
readings in that setup. The expected charger-present state was **not observed**:
both passive charger/MVP responses remained ABSENT, and safety remained
BLOCKED_RANGE rather than BLOCKED_CHARGER. Zero faults, K1 SAFE and range
DISABLED were maintained. Battery samples were3870/3875/3870mV, stateOK.

A separate ST-Link halt/read/resume snapshot, without reset or register writes,
confirmed PA15 CRH mode nibble4 (floating digital input) and PA15 IDR LOW.
K1_CMD, RANGE_EN and PWM_EXC output latches were all LOW; the core was resumed.
This confirms the observed digital level, not a measured CHG_VBUS voltage or
the cause of that level. The exact signal source, charger-input energization
and physical path still require identification; no solder/design fault is
established by this observation. No charger-presence pass or independent
hardware-inhibit qualification is claimed.

See [the unsuccessful presence test and GPIO snapshot](charger-on-unrecognized.json)
and [its UART transcript](charger-on-unrecognized-uart.txt). At this stage,
active DUT work was deferred until the detection discrepancy was resolved.

## Owner setup correction and completed charger endpoint cycle

The owner acknowledged a setup error, corrected it and requested another read;
the exact physical correction was not specified. A new passive COM4 session
reported **PRESENT**, **BLOCKED_CHARGER**, flags9, allowed0, with K1 SAFE,
range DISABLED and zero faults. The MVP response independently repeated the same
charger/blocker state. Battery samples were4147/4145/4145mV, all OK.

After the owner disconnected only the charger input, leaving battery/debugger/
TTL attached and DUT terminals empty, the next passive session reported
**ABSENT**, **BLOCKED_RANGE**, flags8, allowed0. K1 remained SAFE, ranges
DISABLED, residual SAFE and faults zero. Battery samples were3883/3884/3889mV,
all OK. The remaining range block is expected with no selected range; no
measurement was started. No reset or firmware change was commanded.

Together with the initial charger-off observation, these endpoints establish
ABSENT/PRESENT/ABSENT and the corresponding software charger-block change at
this operating point. The earlier low-level observation is preserved as
history, not treated as a current firmware or board defect. See
[the structured endpoint cycle](charger-cycle.json) and its three linked raw
UART transcripts. On-state CHG_VBUS voltage was not measured. Battery values
retain their nominal ADC scale and do not establish charging current/health.

Independent QUSB_INH relay inhibition, relay-coil/transient waveforms, transition
timing and standalone power sequencing were not exercised by these passive
queries. All observed endpoint relay commands were already SAFE. No forced
relay command, test-terminal voltage or calibration write was issued.

## Next physical checks

- Record the exact corrected charger signal source/path and powered-state
  CHG_VBUS voltage when practical; software endpoint detection/blocking is now
  observed, while the on-state voltage remains unmeasured.
- Separately qualify hardware inhibition, supply sequencing, battery thresholds,
  current/headroom, and unpowered/backfeed behavior. Debugger/TTL attachment means
  this session is not proof of standalone battery-only startup or power-off.

These checks remain REQUIRES_BENCH_VALIDATION until measured. No product release,
accuracy qualification,5.5V physical input test or whole power-system approval
is implied by successful ADC/serial communication.
