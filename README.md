# WTK.RLCMeter

WTK.RLCMeter é um medidor RLC portátil de dois fios, baseado em STM32, criado para caracterização de componentes passivos com excitação AC controlada, aquisição síncrona, seleção automática de faixa e calibração complexa.

O projeto combina uma placa analógica/digital própria com um módulo **STM32F103C8T6 Blue Pill**, display TFT **ILI9341**, memória SPI externa da família **W25Q**, alimentação por bateria Li-ion 1S e firmware dedicado para medição, segurança, interface gráfica e diagnóstico.

> **Estado atual:** hardware Rev.1 em fase de protótipo/qualificação. A arquitetura elétrica está fechada o suficiente para fabricação da primeira placa, mas precisão, parasitas, ganho/fase, leakage e limites reais de cada faixa ainda dependem de caracterização e calibração em bancada.

## Objetivos

- Medir **resistência, capacitância, indutância e impedância complexa**.
- Trabalhar inicialmente em frequências de teste de **100 Hz, 1 kHz e 10 kHz**.
- Usar aquisição síncrona para obter magnitude e fase, em vez de depender apenas de valores RMS.
- Selecionar automaticamente uma entre seis impedâncias de referência: **10 Ω, 100 Ω, 1 kΩ, 10 kΩ, 100 kΩ e 1 MΩ**.
- Manter um caminho analógico simples, calibrável e compatível com PCB de duas camadas.
- Proteger o AFE contra componentes carregados e detectar tensão residual de aproximadamente **±100 V** antes de permitir a medição.
- Oferecer uma UI agradável em TFT, com gráficos, telas de diagnóstico, backlight controlado por PWM e feedback sonoro por piezo.
- Manter os componentes e encapsulamentos tão acessíveis quanto possível para compra e montagem manual no Brasil.

## Arquitetura de alto nível

```text
                   ┌─────────────────────────────┐
                   │ STM32F103C8T6 / Blue Pill  │
                   │                             │
                   │ PWM excitation             │
                   │ dual ADC + DMA             │
                   │ range control               │
                   │ DSP / calibration           │
                   │ UI / storage / diagnostics  │
                   └──────────────┬──────────────┘
                                  │
                                  ▼
PWM_EXC ── 3-stage RC ── buffer ── VEXC
                                  │
                                  ▼
                           selected RREF
                                  │
                                  ▼
                                RET ───── DUT ───── VMID
                                  │
                    ┌─────────────┴─────────────┐
                    ▼                           ▼
                  RET_1X                    RET_HG
                    │                           │
                    └────────── ADC ────────────┘
```

O modelo de medição usa as grandezas complexas:

```text
Vs = VEXC - VMID
Vx = RET  - VMID
I  = (Vs - Vx) / ZREF
Zx = ZREF * Vx / (Vs - Vx)
```

A implementação final usa valores **calibrados em magnitude e fase**, não apenas os valores nominais de resistores, ganhos e filtros.

## Hardware atual

Principais blocos da Rev.1:

- **MCU:** STM32F103C8T6 em módulo Blue Pill.
- **AFE:** 2 × MCP6002-E/SN, quatro canais no total.
- **Ganho de retorno:** 1× e aproximadamente 15,47×.
- **Seleção de faixa:** 74HC238 + ULN2003 + BC807 + MOSFETs back-to-back.
- **Ranges:** 10 Ω / 100 Ω / 1 kΩ / 10 kΩ / 100 kΩ / 1 MΩ.
- **MOSFETs low-Z:** AO3400A.
- **MOSFETs high-Z:** 2N7002 individuais em SOT-23.
- **Relé SAFE/MEASURE:** HFD27/005-S.
- **Display:** TFT SPI com controlador ILI9341.
- **Flash:** W25Q64JVSSIQ na BOM atual; firmware será compatível com densidades W25Q menores/maiores adequadas.
- **SPI TFT/Flash compartilhado**, com chip-selects independentes e resistores série de 33 Ω em SCK e MOSI.
- **Entrada:** borne TEST_HI / TEST_LO.
- **Alimentação:** +5V_SYS externo proveniente de módulo 1S Li-ion de carga/boost; a placa monitora bateria e presença do carregador.
- **Backlight:** PB0 / PWM.
- **Piezo externo:** PB1 / PWM, com driver BC817 e conector dedicado.
- **Entrada do usuário:** três botões.
- **Temperatura:** NTC próximo ao banco de referência para compensação/diagnóstico.

## Segurança e tensão residual

A placa é projetada para **componentes passivos desenergizados**. Ela não é um multímetro CAT-rated e não deve ser conectada diretamente à rede elétrica ou a circuitos energizados.

Antes de conectar o DUT ao AFE, o firmware mantém o relé K1 no estado fail-safe e lê duas redes de detecção de tensão residual. Cada lado usa três resistores de 560 kΩ em série e referência de 27 kΩ para VMID, permitindo observar aproximadamente ±100 V dentro da faixa do ADC.

Há ainda:

- bleeder de 94 kΩ entre SAFE_HI e SAFE_LO;
- clamps Schottky nas entradas ADC;
- intertravamento por hardware quando `CHG_VBUS` está presente;
- detecção por firmware de carregador conectado;
- relé K1 desenergizado no estado SAFE;
- `RANGE_EN` com estado seguro durante reset.

O ramo `D_TVS + R_TVS_LINK` é uma contingência experimental e deve permanecer **DNP na primeira montagem**, para não introduzir capacitância/leakage desnecessários no caminho de medição.

Veja [`docs/03-Seguranca-e-Protecao.md`](docs/03-Seguranca-e-Protecao.md).

## Interface e experiência de uso

O firmware prevê:

- tela de startup;
- leitura principal de R/L/C/Z;
- magnitude e fase;
- gráficos e visualizações da resposta do componente;
- seleção automática de range/frequência;
- indicação de bateria e carregador conectado;
- backlight com PWM e auto-dimming;
- tons distintos para confirmação, resultado, erro, tensão residual e bateria baixa;
- tela de diagnóstico com ADCs, rails, range, relés, Flash e estado interno;
- console de eventos no TFT;
- console UART para desenvolvimento e validação.

Assets gráficos podem ser armazenados na Flash SPI e enviados ao ILI9341 em blocos, sem framebuffer de tela inteira na RAM do STM32.

## Firmware

A arquitetura planejada é bare-metal/cooperativa, sem RTOS inicialmente, com timers, DMA e interrupções apenas onde trazem determinismo real.

```text
Firmware/
├── src/
│   ├── app/          # state machine e orchestration
│   ├── bsp/          # binding STM32/periféricos
│   ├── drivers/      # ILI9341, W25Q, botões
│   ├── hardware/     # relés, ranges, excitation, safety
│   ├── measurement/  # aquisição, DSP, impedance, autorange
│   ├── storage/      # assets, settings, calibration
│   └── ui/           # telas, widgets, fontes, console
├── assets/
├── tests/
└── tools/
```

Princípios:

- nenhum `malloc` no caminho de aquisição;
- `RANGE_EN=0` durante troca de faixa;
- K1 sempre retorna a SAFE em falha ou reset;
- buffers pequenos para streaming Flash → TFT;
- ADC acionado por timer e transferido por DMA;
- cálculo complexo por detecção síncrona/DFT de bin único;
- calibração OPEN/SHORT/LOAD por frequência/range/amplitude;
- quiet mode durante aquisição, reduzindo atividade de TFT/backlight/buzzer quando necessário.

Veja [`docs/04-Arquitetura-de-Firmware.md`](docs/04-Arquitetura-de-Firmware.md).

## Estrutura do repositório

```text
WTK.RLCMeter/
├── README.md
├── LICENSE.md
├── PCB/
│   ├── source/
│   ├── fabrication/
│   ├── renders/
│   └── revisions/
├── Firmware/
│   ├── src/
│   ├── assets/
│   ├── tests/
│   └── tools/
└── docs/
```

- [`PCB/README.md`](PCB/README.md): convenções para arquivos EasyEDA, Gerbers, BOM e revisões.
- [`Firmware/README.md`](Firmware/README.md): bootstrap e organização do firmware.
- [`docs/README.md`](docs/README.md): índice da documentação técnica.

## Limitações conhecidas da Rev.1

- A precisão final ainda não está qualificada; o primeiro protótipo é parte do processo de validação metrológica.
- Os ranges de maior impedância são os mais sensíveis a capacitância parasita, leakage e capacitância OFF dos MOSFETs.
- O MCP6002 tem menor GBW que o TLV9064 originalmente considerado; ganho/fase a 10 kHz devem ser calibrados e validados.
- A entrada SAFE detecta tensão residual; ela **não transforma o instrumento em medidor de alta tensão**.
- O USB nativo da Blue Pill usa PA11/PA12. Na alocação atual, esses pinos são usados por `K2_CMD` e `FLASH_CS`; portanto **USB device nativo não é uma interface disponível na Rev.1 sem remapeamento/revisão de hardware**. UART permanece disponível para console e bring-up.

## Roadmap resumido

1. Fabricar e montar a Rev.1.
2. Validar alimentação, VMID, excitação, ranges e SAFE sem DUT.
3. Validar ILI9341, W25Q, botões, backlight, buzzer e UART.
4. Implementar aquisição síncrona e processamento complexo.
5. Qualificar OPEN/SHORT/LOAD e criar persistência de calibração.
6. Caracterizar cada range em 100 Hz, 1 kHz e 10 kHz.
7. Definir ranges NOMINAL/EXTENDED e critérios de confidence.
8. Refinar UI, gráficos e diagnóstico.
9. Fechar limites metrológicos reais da Rev.1 e decidir alterações da Rev.2.

Veja [`docs/08-Roadmap.md`](docs/08-Roadmap.md).

## Licença

WTK.RLCMeter é disponibilizado sob a **PolyForm Noncommercial License 1.0.0**, seguindo o modelo dos demais projetos WTK.*.

Uso pessoal, estudo, pesquisa, avaliação, hobby e demais usos não comerciais são permitidos nos termos da licença. Uso comercial, industrial, integração em produto ou serviço pago, revenda ou qualquer uso com finalidade comercial exige licença comercial separada e autorização escrita do autor.

Para licenciamento comercial: [rodrigowantuk@gmail.com](mailto:rodrigowantuk@gmail.com).

**Required Notice:** Copyright 2026 Rodrigo Wantuk.

Consulte [`LICENSE.md`](LICENSE.md) para os termos completos.
