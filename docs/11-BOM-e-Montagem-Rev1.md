# BOM e montagem — Rev.1

Este documento registra as escolhas de componentes que caracterizam a Rev.1 e as convenções de montagem do primeiro protótipo. A BOM exportada do EDA permanece a fonte de verdade para quantidades e designators.

## Filosofia de montagem

A Rev.1 foi deliberadamente ajustada para montagem manual:

- resistores/capacitores comuns: 0805;
- componentes que exigem mais tensão/potência/robustez: 1206;
- transistores e MOSFETs discretos: SOT-23;
- diodos 1N4148W: SOD-123;
- BAT54S: SOT-23;
- CIs: SOIC/SOP com pitch 1,27 mm;
- relés, NTC, Blue Pill e conectores principais: THT.

O antigo 2N7002DW/SOT-363 foi substituído por pares de 2N7002 individuais SOT-23 nos ranges de alta impedância para facilitar soldagem e retrabalho.

## Componentes estruturais

| Função | Componente Rev.1 |
|---|---|
| MCU | STM32F103C8T6 Blue Pill module |
| AFE | 2 × MCP6002-E/SN |
| Decoder de range | 74HC238D |
| Driver sink | ULN2003ADR |
| Gate high-side | BC807-25 |
| MOSFET low-Z | AO3400A |
| MOSFET high-Z | 2N7002 individual |
| Driver relé/buzzer/interlock | BC817-25 |
| Relé SAFE/MEASURE | HFD27/005-S |
| Relé contingência low-Z | HFD27/005-S |
| Flash | W25Q64JVSSIQ baseline |
| TFT | módulo ILI9341 SPI externo |
| NTC | MF58-104J3950GB, 100 kΩ / B≈3950 |

## RREF

```text
RREF1  10 Ω
RREF2  100 Ω
RREF3  1 kΩ
RREF4  10 kΩ
RREF5  100 kΩ
RREF6  1 MΩ
```

Os valores nominais não substituem calibração. O firmware deve usar correções por range/frequência e, quando aplicável, amplitude.

## Componentes DNP no primeiro protótipo

### TVS

```text
D_TVS       DNP
R_TVS_LINK  DNP
```

O footprint é preservado para experimentos futuros de robustez. O TVS não deve ser montado antes de caracterizar o impacto em capacitância e leakage.

### K2

K2 pode permanecer DNP enquanto `R0_BANK = 0 Ω` for a conexão baseline entre `LOWZ_BUS` e `RET`.

## Alimentação

A carrier board não implementa o carregador/boost 1S completo. Ela recebe de um módulo externo:

```text
VBAT_PROT
+5V_SYS
GND
CHG_VBUS
```

`+5V_A` é derivado localmente de `+5V_SYS` através de `RA = 4,7 Ω` e desacoplamento.

## SPI e TFT

Rev.1 possui:

- `RSCK = 33 Ω` em série na origem de SCK;
- `RMOSI = 33 Ω` em série na origem de MOSI;
- MISO compartilhado sem terminação série dedicada na Rev.1;
- `RTFT_CS = 10 kΩ` pull-up;
- `RFLASH_CS = 10 kΩ` pull-up;
- WP/HOLD da W25Q mantidos inativos por pull-up.

O cabo do TFT deve ser curto. O conector Rev.1 fornece dois GNDs para melhorar retorno de sinal/backlight.

## Buzzer

O piezo passivo fica fora da PCB, na carcaça.

```text
PB1 -> RBBUZZ 4,7 kΩ -> BC817
+5V_SYS -> piezo -> BUZZ_LOW -> BC817 -> GND
```

`RPBUZZ = 4,7 kΩ` fica em paralelo com o piezo para fornecer caminho de descarga à carga capacitiva.

## Ordem sugerida de montagem

1. passivos de alimentação e inspeção de shorts;
2. AFE/VMID;
3. lógica de range e drivers;
4. Flash/TFT interface;
5. seção SAFE;
6. Blue Pill;
7. conectores/relés/NTC/eletrolítico e demais THT.

A estratégia detalhada de energização está em [`09-Bringup-Rev1.md`](09-Bringup-Rev1.md).

## Substituições

Substituições por disponibilidade brasileira são aceitáveis quando preservarem parâmetros elétricos relevantes. Componentes do caminho metrológico não devem ser trocados apenas por footprint/preço sem avaliar:

- RDS(on);
- capacitância OFF;
- leakage;
- offset/GBW/slew rate do AFE;
- tensão de trabalho e potência dos resistores SAFE;
- tensão máxima de comutação e resistência de contato dos relés.

A primeira PCB deve ser qualificada com a BOM realmente montada; mudanças posteriores precisam gerar nova identificação de hardware/calibração quando afetarem a resposta analógica.
