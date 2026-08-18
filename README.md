# WTK.RLCMeter

WTK.RLCMeter é um medidor RLC portátil de dois fios baseado em **STM32F103C8T6**, desenvolvido para caracterização de componentes passivos por excitação AC controlada, aquisição síncrona, seleção automática de faixa e calibração complexa.

O projeto combina uma PCB analógica/digital própria, módulo **Blue Pill**, display TFT **ILI9341**, Flash SPI externa **W25Q**, alimentação por Li-ion 1S e firmware dedicado para medição, segurança, interface gráfica, armazenamento e diagnóstico.

> **Estado atual — agosto de 2026:** hardware de primeiro protótipo em fase de fabricação/bring-up e qualificação. O circuito e o PCB estão maduros o suficiente para a primeira montagem, mas precisão, limites reais de faixa, parasitas, leakage, resposta de ganho/fase e desempenho em 10 kHz ainda precisam ser medidos em bancada. Valores metrológicos publicados pelo firmware só serão considerados válidos após qualificação.

## Objetivos do instrumento

- Medir **resistência, capacitância, indutância e impedância complexa**.
- Extrair `R`, `X`, `|Z|`, fase e, quando a leitura permitir, ESR, Q e D.
- Frequências baseline de **100 Hz, 1 kHz e 10 kHz**.
- Excitação nominal planejada em **100 mVrms** e **500 mVrms**, escolhida por faixa; 500 mVrms não deve ser usada com RREF de 10 Ω.
- Banco de referência automático: **10 Ω, 100 Ω, 1 kΩ, 10 kΩ, 100 kΩ e 1 MΩ**.
- Faixas-alvo de projeto, ainda não qualificadas: aproximadamente **1 Ω–10 MΩ**, **1 nF–10 mF** e **10 µH–10 H**.
- Aquisição síncrona com ADCs internos do STM32, sem ADC externo na primeira revisão.
- Proteção contra componentes carregados e detecção de tensão residual em torno de **±100 V** antes de permitir conexão ao AFE.
- UI em TFT com leitura principal, gráficos/visualizações, diagnóstico, backlight PWM e feedback sonoro.
- PCB de duas camadas e componentes escolhidos com forte preferência por disponibilidade e montagem manual.

> As faixas acima são **metas de engenharia**, não uma especificação de precisão garantida. A Rev.1 será usada para definir regiões `NOMINAL`, `EXTENDED` e `LOW CONFIDENCE` por combinação de range, frequência e amplitude.

## Arquitetura de alto nível

```text
                     ┌──────────────────────────────┐
                     │ STM32F103C8T6 / Blue Pill   │
                     │                              │
                     │ PWM excitation              │
                     │ ADC1 + ADC2 / DMA           │
                     │ range + relay control       │
                     │ DSP + calibration           │
                     │ UI + storage + diagnostics  │
                     └──────────────┬───────────────┘
                                    │
                                    ▼
PWM_EXC ── filtro RC 3 estágios ── buffer ── VEXC
                                    │
                                    ▼
                              RREF selecionado
                                    │
                                    ▼
                                  RET ───── DUT ───── VMID
                                    │
                         ┌──────────┴──────────┐
                         ▼                     ▼
                      RET_1X               RET_HG
                         │                     │
                         └──────── ADC/DMA ────┘
```

Durante a medição:

```text
Vs = VEXC - VMID
Vx = RET  - VMID
I  = (Vs - Vx) / ZREF
Zx = ZREF * Vx / (Vs - Vx)
```

A implementação usa fasores complexos e valores calibrados em magnitude e fase. O firmware não assume que resistor, op-amp, filtro, switch ou ADC sejam ideais.

## Hardware atual

| Bloco | Implementação atual |
|---|---|
| MCU | STM32F103C8T6 em módulo Blue Pill |
| ADC | ADC1 + ADC2 internos, 12 bit |
| AFE | 2 × MCP6002-E/SN |
| Retorno | `RET_1X` + `RET_HG` |
| Ganho HG nominal | `1 + 68k/4,7k ≈ 15,47×` |
| RREF | 10 Ω / 100 Ω / 1 kΩ / 10 kΩ / 100 kΩ / 1 MΩ |
| Seleção de range | 74HC238 + ULN2003 + BC807 + MOSFETs back-to-back |
| MOSFETs low-Z | AO3400A |
| MOSFETs high-Z | 2N7002 individuais em SOT-23 |
| Relé SAFE/MEASURE | Hongfa HFD27/005-S |
| K2 low-Z | footprint de contingência, DNP no baseline |
| Display | ILI9341 SPI |
| Flash | W25Q64JVSSIQ baseline; driver preparado para família W25Q |
| Entrada | TEST_HI / TEST_LO, dois fios |
| Alimentação | +5V_SYS de módulo Li-ion 1S carga/boost externo |
| Temperatura | NTC MF58-104J3950GB próximo ao banco de referência |
| Controles | três botões: UP / OK / DOWN |
| Backlight | PB0 / PWM |
| Buzzer | PB1, piezo passivo externo via BC817 |
| Charger detect | PA15 / `CHG_VBUS` |

### Componentes opcionais / DNP

A primeira montagem privilegia a menor capacitância e leakage possíveis no caminho metrológico:

- `K2` e seu driver: DNP enquanto `R0_BANK = 0 Ω` for satisfatório;
- `D_TVS` + link associado: DNP inicialmente;
- guard ativo: footprint/caminho previsto, mas desconectado no baseline até caracterização.

Esses recursos existem para experimentos controlados de robustez ou redução de parasitas, não para serem montados por padrão sem medição A/B.

## SAFE / MEASURE

WTK.RLCMeter foi projetado para **componentes passivos desenergizados**. Ele não é CAT-rated e não deve ser conectado diretamente à rede elétrica ou a circuitos energizados.

K1 é fail-safe:

```text
K1 desenergizado: TEST_HI/TEST_LO -> rede SAFE
K1 energizado:    TEST_HI -> RET, TEST_LO -> VMID
```

Antes de permitir MEASURE, o firmware verifica os detectores de tensão residual. Cada lado usa uma cadeia resistiva de alta impedância referenciada a VMID, com envelope aproximado de observação de ±100 V. O limiar real para permitir a medição será muito menor e será definido na qualificação.

`CHG_VBUS` também cria duas camadas de bloqueio durante carga:

- intertravamento por hardware impedindo K1 de entrar em MEASURE;
- leitura digital em PA15 para política, UI e diagnóstico.

Mais detalhes: [`docs/03-Seguranca-e-Protecao.md`](docs/03-Seguranca-e-Protecao.md).

## Display, Flash e experiência de uso

O ILI9341 e a W25Q compartilham o barramento SPI, com CS independentes. Como a Blue Pill não possui RAM para framebuffer RGB565 completo de 240×320, a UI é desenhada de forma incremental.

Assets maiores — por exemplo splash screen, ícones, fontes ou gráficos estáticos — ficam na Flash externa e são transmitidos ao TFT em blocos pequenos. Não é necessário carregar uma imagem inteira na RAM.

A experiência planejada inclui:

- splash/startup;
- leitura principal de R/L/C/Z;
- magnitude, fase e modelo equivalente;
- seleção automática de range/frequência/amplitude;
- gráficos e visualizações derivadas da medição;
- status de bateria e carregador;
- backlight com PWM e auto-dimming;
- padrões sonoros distintos para confirmação, erro, alerta e bateria baixa;
- tela de diagnóstico com ADCs, rails, range, K1/K2, Flash, TFT e estado interno;
- console de eventos no TFT;
- UART de desenvolvimento e bring-up.

Durante janelas críticas de aquisição, o firmware pode entrar em **quiet mode**, suspendendo atualizações SPI volumosas, buzzer e logging excessivo.

## Pinout e restrição de USB

A alocação atual reserva, entre outros:

- `PB0` — TFT backlight PWM;
- `PB1` — buzzer/piezo;
- `PA15` — detecção `CHG_VBUS`;
- `PA13/PA14` — SWD;
- `PA11/PA12` — funções da placa atual, portanto indisponíveis como USB D-/D+.

Consequentemente, **USB device nativo da Blue Pill não é uma interface da Rev.1**. Console e bring-up usam UART/SWD. O conector USB do módulo de carga é independente dessa limitação.

Pinout completo: [`docs/05-Pinout-e-Interfaces.md`](docs/05-Pinout-e-Interfaces.md).

## Firmware

Baseline planejado:

- C17;
- GNU Arm Embedded (`arm-none-eabi-gcc`);
- CMake;
- CMSIS + STM32CubeF1 HAL/LL;
- sem RTOS inicialmente;
- timer-triggered ADC + DMA;
- DSP fora de ISR;
- sem alocação dinâmica no caminho crítico;
- persistência versionada e protegida por CRC;
- testes host-side para DSP, calibração, state machines e formatos persistentes.

```text
Firmware/
├── README.md
├── assets/
├── config/
├── src/
│   ├── app/
│   ├── bsp/
│   ├── drivers/
│   ├── hardware/
│   ├── measurement/
│   ├── storage/
│   └── ui/
├── tests/
├── third_party/
└── tools/
```

A divisão é intencional: `measurement` não conhece TFT/GPIO; `ui` não controla relés diretamente; `hardware` expõe serviços seguros; `bsp` concentra bindings do STM32.

Documentação: [`Firmware/README.md`](Firmware/README.md), [`docs/04-Arquitetura-de-Firmware.md`](docs/04-Arquitetura-de-Firmware.md) e [`docs/13-Detalhamento-do-Firmware.md`](docs/13-Detalhamento-do-Firmware.md).

## Estrutura do repositório

```text
WTK.RLCMeter/
├── README.md
├── LICENSE.md
├── CONTRIBUTING.md
├── PCB/
│   ├── source/
│   ├── fabrication/
│   ├── renders/
│   └── revisions/
├── Firmware/
│   ├── assets/
│   ├── config/
│   ├── src/
│   ├── tests/
│   ├── third_party/
│   └── tools/
└── docs/
```

- [`PCB/README.md`](PCB/README.md) — convenções de fonte EDA, fabricação, renders e revisões.
- [`Firmware/README.md`](Firmware/README.md) — stack, dependências e fluxo de desenvolvimento.
- [`docs/README.md`](docs/README.md) — índice da especificação técnica viva.

## Documentação técnica

A documentação detalhada cobre arquitetura de hardware, modelo de medição/DSP, segurança, firmware, pinout, UI/UX, calibração, roadmap, bring-up, decisões consolidadas, BOM e montagem.

Também existem três documentos de visão transversal:

- [`docs/12-Especificacao-Funcional.md`](docs/12-Especificacao-Funcional.md) — features, estados e critérios de aceitação;
- [`docs/13-Detalhamento-do-Firmware.md`](docs/13-Detalhamento-do-Firmware.md) — módulos, APIs, dados e fluxo do firmware;
- [`docs/14-Extensoes-Futuras.md`](docs/14-Extensoes-Futuras.md) — 4-wire/Kelvin, alta tensão e outras evoluções fora da Rev.1.

## Limitações conhecidas

- Precisão final ainda não foi qualificada.
- 1 MΩ e capacitâncias pequenas são particularmente sensíveis a leakage e capacitância parasita.
- O MCP6002 exige caracterização cuidadosa de ganho/fase, especialmente no caminho `RET_HG` a 10 kHz.
- A rede SAFE detecta tensão residual; ela não transforma o aparelho em voltímetro de alta tensão.
- O instrumento é dois fios; Kelvin/4-wire é possibilidade futura.
- USB device nativo não está disponível com o pinout atual.

## Roadmap resumido

1. Fabricar e montar a primeira placa.
2. Validar alimentação, +3V3, +5V_A, VMID e estados seguros.
3. Validar TFT, W25Q, botões, backlight, buzzer e UART.
4. Validar K1, rede SAFE e intertravamento de `CHG_VBUS`.
5. Validar banco RREF, dead-time e K2/R0_BANK baseline.
6. Validar PWM_EXC e ADC/DMA.
7. Implementar I/Q, fasores e cálculo de impedância.
8. Implementar autorange, confidence gates e escolha 1X/HG.
9. Implementar calibração e persistência.
10. Qualificar a matriz range × frequência × amplitude.
11. Refinar UI, gráficos e diagnóstico.
12. Congelar limites metrológicos reais da revisão e decidir alterações seguintes.

Roadmap detalhado: [`docs/08-Roadmap.md`](docs/08-Roadmap.md).

## Licença

WTK.RLCMeter usa a **PolyForm Noncommercial License 1.0.0**, seguindo o padrão adotado nos projetos WTK.*.

Uso pessoal, estudo, pesquisa, avaliação, hobby e outros usos não comerciais são permitidos nos termos da licença. Uso comercial, integração em produto/serviço pago, revenda ou exploração comercial requer licença separada do autor.

**Required Notice:** Copyright 2026 Rodrigo Wantuk.

Consulte [`LICENSE.md`](LICENSE.md) para os termos completos.
