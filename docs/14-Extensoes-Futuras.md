# Extensões futuras

Este documento registra ideias discutidas para revisões posteriores sem transformá-las em requisito da Rev.1.

## Princípio

Uma feature nesta lista não deve ser descrita como capacidade disponível do hardware atual. Qualquer evolução que altere caminho analógico, segurança, isolamento ou parasitas deve criar nova revisão de hardware e nova calibração.

## Kelvin / 4-wire

A Rev.1 é dois fios.

Uma revisão Kelvin poderia melhorar resistência baixa removendo grande parte de:

- resistência dos cabos;
- contato dos bornes;
- resistência de trilhas e conectores.

Impactos esperados:

- quatro terminais externos;
- roteamento FORCE/SENSE separado;
- nova matriz de relés/switches;
- maior complexidade de proteção;
- nova estratégia OPEN/SHORT/LOAD;
- UI para indicar 2-wire/4-wire;
- revisão de autorange e confidence.

Não deve ser enxertado na Rev.1 apenas por firmware.

## Medição de alta tensão

Foi discutida uma evolução com conectores próprios para:

- AC até aproximadamente 400 Vrms;
- DC na ordem de 600–800 V;
- preservação de capacidade de leitura de sinais pequenos em outra entrada/faixa.

Isso é **fora do escopo da Rev.1**.

Uma implementação séria exige frontend separado e revisão completa de segurança, incluindo:

- divisores HV com distribuição de tensão por vários resistores;
- resistores com voltage rating adequado;
- proteção contra transientes;
- creepage/clearance dimensionados;
- conectores e gabinete apropriados;
- segregação física entre frontend HV e AFE de impedância;
- descarga controlada;
- análise de potência/falha de resistores;
- eventualmente isolamento galvânico;
- novo processo de validação e classificação de segurança.

A rede SAFE atual, apesar de observar tensão residual aproximadamente até ±100 V, **não é um frontend de voltímetro HV**.

## Entrada de tensão de ampla faixa

Também foi discutido manter resolução útil para sinais pequenos — ordem de dezenas de milivolts — junto a uma futura entrada de alta tensão.

Isso provavelmente exige ranges dedicados, não um único divisor fixo gigantesco. Possibilidades futuras incluem:

- múltiplos divisores selecionáveis;
- frontend bufferizado;
- autorange específico de tensão;
- proteção separada da entrada RLC;
- calibração por range.

Essa função deve usar conector diferente do terminal de medição RLC para evitar ambiguidades de segurança.

## Frequências adicionais

Depois de qualificar 100 Hz, 1 kHz e 10 kHz, novas frequências podem ser adicionadas se o hardware mostrar margem suficiente.

Antes disso é necessário medir:

- resposta do filtro de excitação;
- fase do MCP6002;
- skew ADC;
- capacitância dos MOSFETs;
- parasitas do PCB;
- comportamento do DUT em cada faixa.

Adicionar frequências sem nova calibração não é aceitável.

## Excitação adicional

100 mVrms e 500 mVrms são o baseline planejado.

Uma revisão futura pode oferecer níveis extras, desde que:

- corrente máxima seja respeitada;
- headroom de VEXC/RET seja suficiente;
- THD permaneça aceitável;
- cada nível seja incluído na matriz de calibração/qualificação.

## ADC externo

ADC externo foi deliberadamente rejeitado para a primeira revisão por custo, disponibilidade e complexidade de prototipagem.

Uma revisão futura só deve reabrir essa decisão se medições reais mostrarem que os ADCs internos são o limitante dominante depois de:

- sincronismo adequado;
- oversampling/averaging quando aplicável;
- calibração;
- melhoria de layout/ruído;
- seleção correta de range e ganho.

## MCU futuro

A Blue Pill permanece baseline da Rev.1. Uma futura migração de MCU pode ser considerada para obter:

- mais RAM para gráficos/framebuffer parcial;
- ADCs melhores;
- mais timers/DMA;
- USB sem conflito de pinout;
- maior Flash interna;
- mais margem de DSP.

Isso não deve ocorrer antes de a primeira placa produzir dados suficientes para justificar a mudança.

## USB nativo

PA11/PA12 estão ocupados na placa atual, então USB device nativo não existe na Rev.1.

Uma revisão futura pode remapear funções para recuperar USB D-/D+, possibilitando:

- CDC console;
- exportação de calibração/logs;
- atualização de firmware;
- integração com software de PC.

## Storage maior / filesystem

A Rev.1 usa W25Q com layout simples sem filesystem.

Se no futuro houver necessidade de muitos assets, logs ou perfis, podem ser avaliados:

- Flash de maior densidade;
- filesystem leve;
- asset compression mais sofisticada;
- atualização de asset pack por PC.

O caminho inicial deve continuar simples e determinístico.

## Guard ativo

O PCB prevê possibilidade de experimentar guard, mas o baseline mantém o caminho isolado/DNP para não introduzir comportamento não caracterizado.

Se leakage em ranges altos se mostrar dominante, o guard pode ser avaliado por comparação A/B. Qualquer benefício deve ser medido antes de virar população padrão.

## K2 para banco low-Z

K2 é uma contingência física para isolar o banco low-Z caso a capacitância parasita associada prejudique ranges altos.

Baseline:

```text
R0_BANK = 0 Ω populado
K2 = DNP
```

Somente medições de Cpar/leakage justificam inverter essa população em revisão posterior.

## TVS opcional

A proteção TVS em paralelo com os terminais de teste permanece DNP inicialmente porque capacitância e leakage podem degradar a metrologia.

Uma variante mais robusta pode ser criada depois de medir o custo metrológico do TVS selecionado.

## Critério para promover uma extensão

Uma extensão deixa de ser “futura” somente quando houver:

1. requisito explícito;
2. análise elétrica;
3. impacto em segurança;
4. impacto em PCB/BOM;
5. estratégia de firmware;
6. plano de calibração;
7. critérios de aceitação;
8. nova identificação de hardware quando aplicável.
