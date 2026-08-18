# Calibração e validação

## Objetivo

A primeira PCB não é considerada instrumento metrologicamente qualificado apenas por ligar. A Rev.1 existe para medir e modelar:

- offset;
- ganho;
- fase;
- RON dos switches;
- resistência de trilhas/relés;
- capacitância OFF;
- leakage;
- resposta dos MCP6002;
- ruído do PWM;
- repetibilidade térmica.

## Calibração complexa

Cada combinação relevante deve ser identificada por uma chave semelhante a:

```text
hardware_revision
frequency
range
excitation_amplitude
channel_gain
```

Temperatura de calibração também deve ser registrada.

## OPEN / SHORT / LOAD

### OPEN

Fixture sem DUT. Caracteriza leakage e admitância/parasitismo residual.

### SHORT

Curto no fixture. Caracteriza impedância série residual, contatos e switches.

### LOAD

Padrão conhecido na região útil do range. Caracteriza scale e phase tracking.

A primeira implementação pode usar correções complexas por offset/scale. Se necessário, os três padrões permitem ajustar um modelo bilinear/Möbius:

```text
Zcorr = (a * Zraw + b) / (c * Zraw + 1)
```

Não fixar esse modelo antes de comparar com dados reais.

## Validação por range

Para cada range e frequência:

1. padrões próximos de 0,1× RREF;
2. ~1× RREF;
3. ~10× RREF;
4. repetir em diferentes amplitudes permitidas;
5. medir repetibilidade e drift;
6. registrar erro de magnitude e fase.

## Componentes para qualificação

- resistores de precisão;
- capacitores C0G/NP0 para valores menores;
- capacitores de filme onde adequado;
- indutores conhecidos e, idealmente, comparados com instrumento de referência;
- OPEN/SHORT fixtures repetíveis.

## Métricas

Registrar no mínimo:

- erro absoluto/relativo;
- desvio padrão entre repetições;
- SNR;
- THD da excitação quando mensurável;
- headroom dos ADCs;
- temperatura;
- residual após SAFE;
- frequência real de excitação/amostragem.

## NOMINAL e EXTENDED

A firmware só marca uma combinação como NOMINAL depois de resultados medidos suficientes.

Objetivo inicial de projeto, não garantia prévia:

- região central qualificada: ordem de 1–2%;
- extremos/extended: podem aceitar erro maior, desde que explicitamente indicados.

Ranges high-Z, especialmente 1 MΩ, só entram em automático depois de leakage/Coff serem caracterizados.

## Regressão

Qualquer alteração de:

- op-amp;
- MOSFET;
- relay;
- RREF;
- layout do caminho analógico;
- firmware de sampling/timing;

pode invalidar parte da calibração. O schema deve carregar `hardware_revision` e versão de modelo.
