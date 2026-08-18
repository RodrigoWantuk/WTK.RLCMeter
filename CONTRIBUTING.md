# Contribuindo com o WTK.RLCMeter

WTK.RLCMeter é um projeto de instrumento de medição. Alterações de hardware, firmware ou documentação devem preservar rastreabilidade entre intenção de projeto, implementação e evidência de bancada.

## Princípios

- Segurança e estado fail-safe têm prioridade sobre conveniência de implementação.
- Precisão é uma propriedade medida e calibrada; não deve ser inferida apenas de datasheet ou simulação.
- Mudanças no AFE, ranges, proteção ou pinout precisam ser refletidas na documentação técnica.
- Código de DSP deve permanecer desacoplado de UI, Flash e GPIO.
- Dependências externas devem ter licença compatível e ficar isoladas em `Firmware/third_party`.

## Alterações de hardware

Uma mudança de PCB relevante deve incluir, quando aplicável:

1. atualização do esquemático;
2. sincronização do PCB;
3. DRC sem shorts, nets abertas ou violações críticas;
4. export de Gerber/BOM correspondente à mesma revisão;
5. atualização de `docs/01-Arquitetura-de-Hardware.md` e/ou `docs/05-Pinout-e-Interfaces.md`;
6. registro de componentes DNP;
7. nota de revisão quando a mudança altera comportamento elétrico ou mecânico.

Mudanças em `SAFE_HI`, `SAFE_LO`, K1, detector residual, bleeder, clamps ou clearances devem ser tratadas como alterações de segurança e revisadas com cuidado adicional.

## Alterações de firmware

Baseline:

- C17;
- sem RTOS na primeira implementação;
- sem alocação dinâmica no caminho crítico de aquisição;
- ISR curta;
- ADC/DMA/timers para temporização determinística;
- GPIO/periféricos acessados por BSP/services, não espalhados pela aplicação;
- K1 deve retornar a SAFE em fault/reset;
- `RANGE_EN` deve ficar desabilitado durante troca de range.

### Testes

Matemática e state machines puras devem, sempre que possível, possuir testes host-side em `Firmware/tests`.

Casos prioritários:

- operações complexas e I/Q;
- cálculo de impedância;
- reconstrução do canal high-gain;
- autorange;
- migração/CRC de calibração;
- policy SAFE/MEASURE;
- parsing do asset pack.

## Documentação

`docs/README.md` é o índice da documentação normativa atual.

Evite afirmar como capacidade qualificada aquilo que ainda é apenas planejado. Use linguagem explícita para distinguir:

- hardware existente;
- feature implementada;
- feature planejada;
- comportamento validado em bancada;
- range NOMINAL;
- range EXTENDED.

## Licença

Contribuições ao repositório são disponibilizadas sob os mesmos termos do projeto, descritos em [`LICENSE.md`](LICENSE.md).

O projeto usa PolyForm Noncommercial License 1.0.0. Uso comercial requer licença separada do autor.
