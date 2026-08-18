# Documentação técnica

Esta pasta contém a especificação técnica viva do WTK.RLCMeter.

A documentação separa deliberadamente **hardware Rev.1**, **arquitetura planejada** e **capacidades ainda não qualificadas**. Resultado medido no protótipo tem precedência sobre expectativa teórica.

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

## Convenções

- `DUT`: Device Under Test.
- `VEXC`: excitação analógica.
- `VMID`: referência virtual próxima de metade de 3,3 V.
- `RET`: nó entre RREF e DUT.
- `RREF`: impedância de referência selecionada.
- `SAFE`: AFE isolado do DUT.
- `MEASURE`: DUT conectado ao caminho de medição.
- `NOMINAL`: combinação qualificada para o alvo principal de precisão.
- `EXTENDED`: combinação funcional fora da região metrológica principal.
- `DNP`: footprint presente, componente não montado.

## Regra de verdade

- Precisão é propriedade medida, não inferida da BOM.
- O range de 1 MΩ depende de leakage e capacitância parasita reais.
- A resposta do MCP6002 em 10 kHz precisa de calibração complexa.
- A proteção de tensão residual não implica classificação CAT.
- Interfaces indisponíveis no pinout da Rev.1 não devem ser descritas como funcionalidades existentes.
- Visualizações derivadas na UI não devem ser confundidas com modos de aquisição que ainda não foram implementados/qualificados.
