# Safety and Protection

## Scope

WTK.RLCMeter Rev.1 is intended for **de-energized passive components**. The SAFE network is designed to detect and tolerate residual voltage within its intended envelope before connecting the DUT to the measurement AFE, but this does not make the instrument CAT-rated or suitable for direct connection to mains.

Never connect the Rev.1 RLC input directly to an energized high-voltage circuit.

## Fail-safe state

K1 de-energized = SAFE.

```text
TEST_HI -> SAFE_HI
TEST_LO -> SAFE_LO
```

K1 energized = MEASURE.

```text
TEST_HI -> RET
TEST_LO -> VMID
```

Firmware must assume SAFE during boot, reset, fault, watchdog recovery, and brownout handling.

## Residual-voltage detector

Each side uses a high-value divider referenced to VMID:

```text
SAFE_x -> 560k -> 560k -> 560k -> SENSE_x
                                  |
                                 27k
                                  |
                                 VMID
```

With `RTOP = 1.68 MΩ` and `RLOW = 27 kΩ`:

```text
k = 27k / (1.68M + 27k) ≈ 0.015817
VSENSE = VMID + k * (VIN - VMID)
```

For VMID near 1.65 V, the network provides an approximate observation envelope around ±100 V without exceeding the ADC range under the intended conditions.

**The threshold that permits K1 to enter MEASURE must be far below 100 V and will be established through firmware policy and bench validation.**

## Bleeder

```text
SAFE_HI -- 47k -- 47k -- SAFE_LO
```

Total resistance: 94 kΩ.

Discharge time depends on DUT capacitance:

```text
tau = 94k * C_DUT
```

Firmware must never assume that a fixed delay means the DUT is discharged; residual voltage must be measured again.

## ADC protection

Residual-voltage ADC channels use series resistance and BAT54S clamps. The 1.68 MΩ source resistance strongly limits clamp current during residual-voltage events.

## Charger interlock

`CHG_VBUS` is handled in two layers:

1. **Hardware:** the charger interlock prevents K1 from entering MEASURE.
2. **Firmware:** a divided digital signal is read on PA15 for policy, UI, and diagnostics.

Safety must not depend only on the firmware-readable charger signal.

## TVS option

`D_TVS` and `R_TVS_LINK` form an optional protection branch across the test terminals.

First assembly: **DNP**.

Reason: TVS capacitance, leakage, nonlinearity, and temperature dependence can degrade high-impedance and small-capacitance measurements. The footprint exists for controlled A/B evaluation after the baseline metrology is characterized.

## Fault policy

Any of the following conditions must abort or prevent MEASURE and return K1 to SAFE:

- residual voltage above the permitted threshold;
- `CHG_VBUS` / charger detect active;
- relevant ADC failure;
- invalid or indeterminate range state;
- supply/brownout condition that makes measurement unreliable;
- watchdog/reset recovery;
- impossible application-state transition;
- explicit safety fault.

Baseline fault action:

```text
stop excitation
RANGE_EN = 0
K1 -> SAFE
K2 -> safe/default state
buzzer -> off
record fault
report fault through diagnostics when available
```
