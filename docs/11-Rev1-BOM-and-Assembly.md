# Rev.1 BOM and Assembly Notes

This document records the component choices that define Rev.1 and the assembly conventions for the first prototype. The BOM exported from the EDA project remains the source of truth for quantities and designators.

Before soldering, use the [A05 population matrix and blockers](review/a05/README.md).
Actual module identities, power subsystem and manufactured-board inspection are
still pending. POPULATE preserves reviewed topology; HOLD requires resolution.

## Manual-assembly philosophy

Rev.1 was deliberately adjusted for manual assembly and rework:

- ordinary resistors/capacitors: 0805;
- passives requiring more voltage, power, or robustness: 1206;
- discrete transistors and MOSFETs: SOT-23 where practical;
- 1N4148W-class diodes: SOD-123;
- BAT54S: SOT-23;
- ICs: SOIC/SOP with 1.27 mm pitch where possible;
- relays, NTC, Blue Pill, and major connectors: THT.

The earlier 2N7002DW/SOT-363 concept was replaced by individual 2N7002 SOT-23 devices in the high-impedance ranges to simplify soldering and rework.

## Structural components

| Function | Rev.1 component |
|---|---|
| MCU | STM32F103C8T6 Blue Pill module |
| AFE | 2 × MCP6002-E/SN |
| Range decoder | 74HC238D |
| Sink driver | ULN2003ADR |
| High-side gate driver | BC807-25 |
| Low-Z MOSFET | AO3400A |
| High-Z MOSFET | Individual 2N7002 |
| Relay/buzzer/interlock driver | BC817-25 |
| SAFE/MEASURE relay | HFD27/005-S |
| Low-Z contingency relay | HFD27/005-S |
| External Flash | W25Q64JVSSIQ baseline |
| TFT | External ILI9341 SPI module |
| NTC | MF58-104J3950GB, 100 kΩ / B≈3950 |

## RREF values

```text
RREF1  10 Ω
RREF2  100 Ω
RREF3  1 kΩ
RREF4  10 kΩ
RREF5  100 kΩ
RREF6  1 MΩ
```

Nominal values do not replace calibration. Firmware should use complex corrections by range/frequency and, where relevant, amplitude/channel.

## High-gain path

Current baseline:

```text
RF_HG = 68 kΩ
RG_HG = 4.7 kΩ
Gnom ≈ 15.47×
```

The effective response is frequency dependent and must be qualified/calibrated.

## DNP components in the first prototype

### TVS option

```text
D_TVS       DNP
R_TVS_LINK  DNP
```

The footprint is retained for later robustness experiments. Do not populate the TVS before characterizing its capacitance/leakage impact.

### K2

The audited baseline is K2 DNP with `R0_BANK = 0 Ω` populated between `LOWZ_BUS`
and `RET`. Its unused QK2/RK2B/RK2PD/DK2 chain may also be omitted. A relay-only
alternative requires removing the hard link and reviewing firmware, which currently
declares K2 absent. Do not approve both population paths simultaneously.

`R_BYP_VM` and `R_BYP_VEXC` must be populated: they close the U4 output/feedback
paths. They are not optional active-guard parts; no guard footprint was identified.

## Power

The carrier board does not implement the complete 1S charger/boost subsystem. It receives:

```text
VBAT_PROT
+5V_SYS
GND
CHG_VBUS
```

`+5V_A` is locally derived from `+5V_SYS` through `RA = 4.7 Ω` plus local decoupling.

## SPI and TFT

Rev.1 includes:

- `RSCK = 33 Ω` series resistor near the SCK source;
- `RMOSI = 33 Ω` series resistor near the MOSI source;
- shared MISO without a dedicated series terminator in Rev.1;
- TFT CS pull-up;
- W25Q CS pull-up;
- W25Q WP/HOLD kept inactive with pull-ups.

Keep the TFT cable short. The actual connector is 1×9 with one GND pin; use the
[verified order](05-Pinout-and-Interfaces.md). HOLD its connection and R_TFT_LED
until the exact module and backlight input are verified. The 0 Ω link to PB0 has
no carrier power driver/current limit.

## Buzzer

Firmware intends a passive tone transducer. The actual PCB has BUZZER1, a two-pin
on-board footprint, rather than the historical external J_BUZZ connector.
**HOLD BUZZER1:** the BOM's PB-12N23MPW-12Q is an internally driven magnetic
indicator rated 9–15 V, incompatible with the 5 V/passive-tone circuit. A selected
replacement must match type, drive/current, dimensions and pin pitch; none is
approved by A05.

```text
PB1 -> base resistor -> BC817
+5V_SYS -> BUZZER1 pad 1 -> load -> pad 2 -> BC817 -> GND
```

A 4.7 kΩ resistor is across the load. It provides discharge for an intended
capacitive transducer; it does not provide magnetic-load flyback protection.

## Suggested assembly order

1. power passives and short inspection;
2. AFE / VMID section;
3. range logic and drivers;
4. Flash/TFT interface;
5. SAFE section;
6. Blue Pill;
7. connectors, relays, NTC, electrolytics, and remaining THT parts.

See [`09-Rev1-Bringup.md`](09-Rev1-Bringup.md) for the staged power-up strategy.

## Substitutions

Substitutions motivated by local availability are acceptable only when the relevant electrical parameters remain appropriate.

For metrology-path parts, evaluate at least:

- RDS(on);
- OFF capacitance;
- leakage;
- op-amp offset/GBW/slew rate/input behavior;
- SAFE resistor voltage and power ratings;
- relay contact resistance, switching rating, and coil behavior;
- dielectric and temperature behavior where relevant.

The first PCB must be qualified using the BOM that was actually assembled. Later component substitutions that materially affect analog response should produce a new hardware/calibration identity.
