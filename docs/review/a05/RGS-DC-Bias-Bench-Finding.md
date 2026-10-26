# Selected-range gate resistor injects DC into the analog path

Status: OPEN; REQUIRES_BENCH_VALIDATION. No circuit correction is defined here.
The earlier polarity/connectivity audit did not analyze this DC loading path.

## Physical evidence

On 2026-10-24 the owner reported these DMM DC voltages to GND with 10K selected,
K1 commanded SAFE and excitation stopped. An isolated nominal 10 kOhm resistor
remained on the terminals. UART reported 10K READY, faults zero and charger ABSENT.
Deployed BRINGUP bench firmware identity: `4127c73adb73`.

| Point | Owner-reported voltage |
| --- | --- |
| U4.7 and both R_BYP_VEXC pads | 0 V |
| RREF4 pads, orientation unspecified | 0 V and 2.5 V |
| RGS4 pads, orientation unspecified | 5 V and 2.5 V |
| U5.3, U5.1, STM32 PA2 | 2.5 V each |

Instrument model, uncertainty, photographs and measurement timestamps were not
provided. These are owner-reported readings, not an accuracy qualification. There
is no K1 contact feedback. Local evidence is retained under
`Firmware/build/physical-20261024-tim2-compare/dc-rgs-bias-001/`.

The August 23 fabrication export `FlyingProbeTesting.json` confirms RGS4.1 at
SRC_10K, RGS4.2 at GATE_10K, both QSEL7A/B sources at SRC_10K and both gates at
GATE_10K. RREF4.1 is VEXC; RREF4.2 connects to QSEL7A drain; QSEL7B drain is RET.
The matching BOM specifies RGS4 = 10 kOhm, RREF4 = 10 kOhm and RGATE4 = 100 Ohm.
Export connectivity is not independent proof of actual solder continuity.

## First-order interpretation

With the selected gate near 5 V and the switches conducting, RGS4 creates:

```text
gate -> RGS4 -> common sources -> QSEL7A -> RREF4 -> VEXC
                    |
                 QSEL7B -> RET (K1 SAFE, high-impedance AFE inputs)
```

For negligible switch resistance/input current and other branches off:

```text
Vsource approximately (Vgate * RREF + VEXC * RGS) / (RREF + RGS)
```

The measured endpoints and nominal equal 10 kOhm resistors predict 2.5 V and
approximately 250 uA. This agrees with the readings without requiring a solder
bridge. It does not prove actual resistor values, switch identity, exclusive
selection or all connections.

RGS loads the analog path while selected; it is not only a gate discharge
resistor. With a sufficiently stiff driver it also adds AC loading. The ideal
unloaded reference model cannot be assumed valid for this topology. Complete
ADC/DMA transfer does not resolve HG clipping or qualify resistance/calibration.

## Completed DC comparison and pending correction review

A UART range request selected 1K using the existing disable/address/settle path.
No excitation or relay command was issued. Nominal RREF3 = 1 kOhm and RGS3 =
10 kOhm predict a source near 0.45 V for a 5 V gate and zero VEXC. The owner then
reported RREF3 pads at 0 V / 0.5 V and RGS3 pads at 5 V / 0.5 V. These rounded
readings agree with the prediction and strengthen the topology-induced DC bias
diagnosis. They are not a numerical resistor/switch accuracy qualification.

After recording the comparison, `lab range off` returned OK. A fresh UART status
confirmed range DISABLED, K1 SAFE and faults zero. Excitation remains stopped;
COM3 is closed. Evidence: `dc-rgs-bias-002/` in the local physical session folder.

A circuit correction requires explicit review of discharge/off-state behavior,
unpowered rails, VGS limits, settling, leakage, all six ranges and safety
invariants. No removal, value substitution or endpoint relocation is prescribed.
Preserve raw readings; do not conceal this effect with calibration offsets or
suppression of clipped samples.

A bounded existing-component recovery candidate is now documented in
[the gate-to-GND pull-down analysis](RGS-Ground-Pulldown-Rework-Candidate.md).
It is not physically implemented or qualified and does not change the canonical
PCB/schematic. Actual RGS4 pad identification precedes any solder instruction.
