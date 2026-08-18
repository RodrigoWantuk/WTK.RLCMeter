# Hardware Architecture

## Scope

Rev.1 is a two-layer PCB for characterization of **de-energized passive components**. It deliberately avoids an external ADC and favors packages that remain practical for manual assembly.

## Analog path

```text
PWM_EXC
   │
   ▼
3-stage RC filter
   │
   ▼
U4B buffer ── VEXC ── selected RREF ── RET ── DUT ── VMID
                                         │
                                         ├── U5A -> RET_1X
                                         └── U5B -> RET_HG
```

`VMID` is generated from +3V3 through a 10 kΩ / 10 kΩ divider, filtered, and buffered by U4A.

## Analog front-end

Rev.1 uses **2 × MCP6002-E/SN**:

- U4A — `VMID` buffer;
- U4B — `VEXC` buffer;
- U5A — `RET_1X` buffer;
- U5B — `RET_HG` amplifier.

Nominal high-gain path:

```text
G = 1 + 68 kΩ / 4.7 kΩ ≈ 15.468
```

The DSP must use a calibrated complex response rather than this nominal DC gain alone, especially at 10 kHz.

## Excitation

`PWM_EXC` is generated on PA8 / TIM1_CH1. The planned PWM carrier is approximately 450 kHz.

A three-stage 5.1 kΩ / 1 nF RC filter reconstructs the analog excitation before buffering. Excitation amplitude depends on range; the 10 Ω reference range must use reduced amplitude to protect current, distortion, and MCP6002 headroom.

Baseline measurement amplitudes are planned at 100 mVrms and 500 mVrms, with 500 mVrms forbidden on the 10 Ω RREF range.

## RREF bank

| Range | RREF | Switch technology |
|---|---:|---|
| 0 | 10 Ω | 2 × AO3400A back-to-back |
| 1 | 100 Ω | 2 × AO3400A back-to-back |
| 2 | 1 kΩ | 2 × AO3400A back-to-back |
| 3 | 10 kΩ | 2 × 2N7002 back-to-back |
| 4 | 100 kΩ | 2 × 2N7002 back-to-back |
| 5 | 1 MΩ | 2 × 2N7002 back-to-back |

Selection is one-hot:

```text
STM32 -> 74HC238 -> ULN2003 -> BC807 -> MOSFET gate pairs
```

`RANGE_EN` must stay LOW during reset and throughout every range transition.

Low ranges converge on `LOWZ_BUS`. The current baseline uses `R0_BANK = 0 Ω` to connect that bus to `RET`; K2 remains a contingency option and may remain DNP in the first assembly.

## ADC channels

Primary measurement channels:

- `ADC_VEXC`;
- `ADC_VMID`;
- `ADC_RET_1X`;
- `ADC_RET_HG`.

Each channel uses a 1 kΩ series resistor, 1 nF to ground, and BAT54S clamping to GND/+3V3.

Auxiliary channels:

- `ADC_OV_HI`;
- `ADC_OV_LO`;
- `ADC_BAT`;
- `ADC_NTC`.

## K1 SAFE/MEASURE relay

K1 is an HFD27/005-S and is fail-safe by coil de-energization.

```text
TEST_LO (COM) -> SAFE_LO (NC) / VMID (NO)
TEST_HI (COM) -> SAFE_HI (NC) / RET  (NO)
```

With the coil off, the DUT is connected only to the SAFE network. With K1 energized, TEST_LO connects to VMID and TEST_HI connects to RET for measurement.

## Power

J_PWR receives:

```text
1  VBAT_PROT
2  +5V_SYS
3  GND
4  CHG_VBUS
```

`+5V_A` is derived locally:

```text
+5V_SYS -- 4.7 Ω -- +5V_A
```

with local decoupling. `VBAT_PROT` is monitored through a 100 kΩ / 100 kΩ divider.

`CHG_VBUS` drives both the hardware interlock and a divided digital input on PA15.

## TFT and external Flash

Display: ILI9341 SPI.

Current baseline Flash: W25Q64JVSSIQ.

Shared SPI bus:

```text
PB13 -> 33 Ω -> TFT_SCK  -> ILI9341 SCK + W25Q CLK
PB15 -> 33 Ω -> TFT_MOSI -> ILI9341 MOSI + W25Q DI
PB14 --------> TFT_MISO <- ILI9341 MISO + W25Q DO
```

Chip selects are independent: PB12 for TFT and PA12 for Flash in the current pinout.

## DNP / experimental options

First assembly baseline:

- `D_TVS`: DNP;
- `R_TVS_LINK`: DNP;
- K2 may remain DNP while `R0_BANK` is used;
- active guard provisions, where present, remain experimental until leakage/parasitic measurements justify them.

DNP options must not be promoted to the default BOM without measured A/B evidence.
