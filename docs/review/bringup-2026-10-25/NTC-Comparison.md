# NTC comparison — October 25, 2026

Status: COLD_COMPARISON_SAVED; WARM_REFERENCE_PENDING; NO_CORRECTION_APPLIED.

| Contact | External reference | Firmware | ADC | Nominal inferred resistance |
| --- | ---: | ---: | ---: | ---: |
| Owner-described PCB thermal point | 23.8 degC | 27.13364 degC | 1958 | 91623.76 ohm |
| Direct isolated NTC bead after stabilization | 23.5 degC | 27.27030 degC | 1952 | 91087.27 ohm |

The external reference is an owner-provided contact thermometer. Its model is
not requested or recorded. Reference uncertainty is unspecified, not zero.
These two readings use different contact conditions and are not a two-temperature
Beta calibration. The direct-bead point gives a local comparison difference of
3.77030 degC. No permanent offset or curve is installed from that difference.

The current BOM specifies RNTC 100 kohm and MF58 100 kohm/B3950; the firmware
uses a fixed 100-kohm/B3950 lookup table. The owner could not read a resistor
marking and the purchased thermistor listing says only "NTC 3mm 100k". Actual
Beta is unknown. The owner is preparing a warmer stable bead-contact point;
its raw ADC ratio and reference temperature can determine an effective
R25/Rtop ratio and a Beta estimate without assuming a measured Rtop or R25.
This still requires reference/thermal-contact evidence and is not full-curve
accuracy qualification. The runtime has no persistent NTC trim interface yet;
any added storage/firmware contract must be explicitly designed and tested.

Both RAM snapshots were read only in SAFE/idle, without reset; fresh NTC validity
and sample ages were checked. Battery remained normal, fault mask zero, protocol
counter unchanged at 1. No MCU firmware, W25Q setting or calibration was written.

See [structured readings](NTC-Comparison.json) and
[physical evidence](../../../Firmware/releases/2026-10-25/ntc-comparison-checkpoint/README.md).
