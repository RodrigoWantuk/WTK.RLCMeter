# Documentação técnica

Esta pasta contém a especificação técnica viva do WTK.RLCMeter.

A documentação separa deliberadamente **hardware da primeira revisão**, **arquitetura planejada**, **metas ainda não qualificadas** e **extensões futuras**. Resultado medido no protótipo tem precedência sobre expectativa teórica.

## Índice

1. [`01-Arquitetura-de-Hardware.md`](01-Arquitetura-de-Hardware.md) — AFE, RREF, alimentação, relés, TFT e Flash.
2. [`02-Modelo-de-Medicao-e-DSP.md`](02-Modelo-de-Medicao-e-DSP.md) — equações complexas, aquisição síncrona e derivação R/L/C.
3. [`03-Seguranca-e-Protecao.md`](03-Seguranca-e-Protecao.md) — SAFE/MEASURE, tensão residual, intertravamentos e limites.
4. [`04-Arquitetura-de-Firmware.md`](04-Arquitetura-de-Firmware.md) — organização de código, state machine, timers, DMA e persistência.
5. [`05-Pinout-e-Interfaces.md`](05-Pinout-e-Interfaces.md) — GPIOs do STM32 e conectores da Rev.1.
6. [`06-UI-UX-e-Diagnostico.md`](06-UI-UX-e-Diagnostico.md) — ILI9341, assets, telas, visualizações, botões, backlight, buzzer e console.
7. [`07-Calibracao-e-Validacao.md`](07-Calibracao-e-Validacao.md) — OPEN/SHORT/LOAD, confidence e qualificação metrológica.
8. [`08-Roadmap.md`](08-Roadmap.md) — sequência de implementação e futuras revisões.
9. [`09-Bringup-Rev1.md`](09-Bringup-Rev1.md) — ordem segura de montagem e energização.
10. [`10-Decisoes-de-Projeto.md`](10-Decisoes-de-Projeto.md) — decisões consolidadas e alternativas descartadas.
11. [`11-BOM-e-Montagem-Rev1.md`](11-BOM-e-Montagem-Rev1.md) — componentes estruturais, DNP, montagem manual e regras para substituições.
12. [`12-Especificacao-Funcional.md`](12-Especificacao-Funcional.md) — catálogo de features, estados, metas de faixa e critérios funcionais.
13. [`13-Detalhamento-do-Firmware.md`](13-Detalhamento-do-Firmware.md) — módulos, contratos, APIs, persistência, DSP e sequência de implementação.
14. [`14-Extensoes-Futuras.md`](14-Extensoes-Futuras.md) — 4-wire, alta tensão, USB, guard, K2 e outras evoluções fora do baseline.

## Convenções

- `DUT`: Device Under Test.
- `VEXC`: excitação analógica.
- `VMID`: referência virtual próxima de metade de 3,3 V.
- `RET`: nó entre RREF e DUT.
- `RREF`: impedância de referência selecionada.
- `RET_1X`: caminho de retorno sem ganho adicional.
- `RET_HG`: caminho high-gain; hardware atual usa nominalmente ~15,47×.
- `SAFE`: AFE isolado do DUT.
- `MEASURE`: DUT conectado ao caminho de medição.
- `NOMINAL`: combinação qualificada para o alvo principal de precisão.
- `EXTENDED`: combinação funcional fora da região metrológica principal.
- `LOW_CONFIDENCE`: resultado sem garantia suficiente para publicação normal.
- `DNP`: footprint presente, componente não montado.

## Fonte de verdade

Quando houver divergência entre documentos históricos e a placa atual, usar esta precedência:

1. PCB/BOM exportado mais recente que será efetivamente montado;
2. esquemático correspondente;
3. documentação técnica atualizada;
4. documentos históricos de revisão;
5. estimativas/discussões antigas.

Isso é particularmente importante para valores que mudaram ao longo do projeto, como op-amps, ganho high-gain, população de K2 e escolhas de footprints.

## Regra de verdade metrológica

- Precisão é propriedade medida, não inferida da BOM.
- As metas de aproximadamente 1 Ω–10 MΩ, 1 nF–10 mF e 10 µH–10 H ainda precisam de qualificação.
- O range de 1 MΩ depende fortemente de leakage e capacitância parasita reais.
- A resposta do MCP6002 em 10 kHz precisa de calibração complexa.
- O ganho nominal de `RET_HG` não substitui `H_HG(f)` calibrado.
- A proteção de tensão residual não implica classificação CAT.
- Interfaces indisponíveis no pinout atual não devem ser descritas como funcionalidades existentes.
- Visualizações derivadas na UI não devem ser confundidas com modos de aquisição que ainda não foram implementados/qualificados.
- Features futuras devem permanecer explicitamente marcadas como fora da Rev.1.

## Atualização da documentação

Ao mudar hardware que afete resposta analógica ou segurança:

1. atualizar BOM/esquemático;
2. registrar decisão em `10-Decisoes-de-Projeto.md`;
3. atualizar pinout/arquitetura quando aplicável;
4. atualizar chave de `hardware_revision` usada na calibração;
5. revisar bring-up e critérios de qualificação.

Ao mudar somente firmware sem impacto no formato persistente ou metrologia, não é necessário criar nova revisão de PCB.
