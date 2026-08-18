# Arquitetura de hardware

## Escopo

A Rev.1 é uma PCB de duas camadas para caracterização de componentes passivos desenergizados. O projeto evita ADC externo e privilegia encapsulamentos amigáveis à montagem manual.

## Fluxo analógico

```text
PWM_EXC
   │
   ▼
3-stage RC filter
   │
   ▼
U4B buffer ── VEXC ── RREF selecionado ── RET ── DUT ── VMID
                                         │
                                         ├── U5A -> RET_1X
                                         └── U5B -> RET_HG
```

`VMID` é criado a partir de +3V3 por divisor 10 kΩ / 10 kΩ, filtrado e bufferizado por U4A.

## AFE

A Rev.1 usa **2 × MCP6002-E/SN**:

- U4A — buffer de `VMID`;
- U4B — buffer de `VEXC`;
- U5A — buffer `RET_1X`;
- U5B — `RET_HG`.

Ganho nominal high-gain:

```text
G = 1 + 68 kΩ / 4,7 kΩ ≈ 15,468
```

O valor final usado pelo DSP deve ser calibrado como resposta complexa, especialmente a 10 kHz.

## Excitação

`PWM_EXC` é gerado em PA8/TIM1_CH1. O carrier planejado é aproximadamente 450 kHz.

O filtro usa três estágios 5,1 kΩ / 1 nF antes do buffer. A amplitude é dependente do range; o range de 10 Ω deve operar com amplitude limitada para respeitar corrente, THD e headroom do MCP6002.

## Banco RREF

| Range | RREF | Switch |
|---|---:|---|
| 0 | 10 Ω | 2 × AO3400A back-to-back |
| 1 | 100 Ω | 2 × AO3400A back-to-back |
| 2 | 1 kΩ | 2 × AO3400A back-to-back |
| 3 | 10 kΩ | 2 × 2N7002 back-to-back |
| 4 | 100 kΩ | 2 × 2N7002 back-to-back |
| 5 | 1 MΩ | 2 × 2N7002 back-to-back |

A seleção é one-hot:

```text
STM32 -> 74HC238 -> ULN2003 -> BC807 -> gates dos pares MOSFET
```

`RANGE_EN` deve permanecer LOW durante reset e durante qualquer troca de range.

Os ranges baixos convergem em `LOWZ_BUS`. A Rev.1 usa `R0_BANK = 0 Ω` como baseline até `RET`. K2 permanece como contingência.

## ADC

Canais principais:

- `ADC_VEXC`;
- `ADC_VMID`;
- `ADC_RET_1X`;
- `ADC_RET_HG`.

Cada canal usa 1 kΩ série, 1 nF para GND e clamp BAT54S para GND/+3V3.

Canais auxiliares:

- `ADC_OV_HI`;
- `ADC_OV_LO`;
- `ADC_BAT`;
- `ADC_NTC`.

## K1 SAFE/MEASURE

K1 é HFD27/005-S e é fail-safe por bobina desenergizada.

Conectividade física Rev.1:

```text
TEST_LO (COM) -> SAFE_LO (NC) / VMID (NO)
TEST_HI (COM) -> SAFE_HI (NC) / RET  (NO)
```

Assim, sem energia na bobina, o DUT fica conectado apenas à seção SAFE. Quando K1 é energizado, TEST_LO é ligado a VMID e TEST_HI a RET para medição.

## Alimentação

Entrada J_PWR:

```text
1  VBAT_PROT
2  +5V_SYS
3  GND
4  CHG_VBUS
```

`+5V_A` é derivado por:

```text
+5V_SYS -- 4,7 Ω -- +5V_A
```

com desacoplamento local. `VBAT_PROT` é monitorado por divisor 100 kΩ / 100 kΩ.

`CHG_VBUS` alimenta o intertravamento analógico e um divisor 10 kΩ / 12 kΩ para a entrada digital `CHG_DETIO`.

## TFT e Flash

Display: ILI9341 SPI.

Flash atual: W25Q64JVSSIQ.

SPI compartilhado:

```text
PB13 -> RSCK 33 Ω  -> TFT_SCK  -> ILI9341 SCK + W25Q CLK
PB15 -> RMOSI 33 Ω -> TFT_MOSI -> ILI9341 MOSI + W25Q DI
PB14 --------------> TFT_MISO <- ILI9341 MISO + W25Q DO
```

CS independentes: PB12 para TFT e PA12 para Flash.

## Componentes DNP

Primeira montagem:

- `D_TVS`: DNP;
- `R_TVS_LINK`: DNP;
- K2 pode permanecer DNP quando `R0_BANK` for usado como baseline.

O ramo TVS é reservado para experimentos futuros de robustez depois de caracterizar seu impacto em capacitância e leakage.
