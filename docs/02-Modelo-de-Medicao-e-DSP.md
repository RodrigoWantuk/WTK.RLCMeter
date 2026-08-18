# Modelo de medição e DSP

## Modelo elétrico

Durante a medição:

```text
VEXC -- ZREF -- RET -- ZDUT -- VMID
```

Grandezas complexas relativas a VMID:

```text
Vs = VEXC - VMID
Vx = RET  - VMID
I  = (Vs - Vx) / ZREF
Zx = ZREF * Vx / (Vs - Vx)
```

Esta é a equação central do instrumento.

## Processamento complexo

Amplitude isolada não é suficiente para separar resistência e reatância. Cada canal é convertido em um fasor complexo por detecção síncrona / DFT de bin único:

```text
V = I_component + j * Q_component
```

Uma FFT completa não é necessária no caminho normal.

## Frequências baseline

Primeira qualificação:

- 100 Hz;
- 1 kHz;
- 10 kHz.

A matriz final range × frequência × amplitude será definida empiricamente. Nem toda combinação será automaticamente válida.

## Aquisição

Fluxo planejado:

1. timer cria trigger de amostragem;
2. ADCs capturam canais de forma determinística;
3. DMA transfere blocos para RAM;
4. ISR apenas sinaliza buffer pronto;
5. DSP executa fora da ISR;
6. janelas usam número inteiro de ciclos sempre que possível.

Qualquer atraso determinístico entre ADC1/ADC2 ou ranks deve ser medido e compensado.

## RET 1× e high-gain

`RET_1X` preserva headroom.

`RET_HG` aumenta SNR para sinais pequenos. Reconstrução conceitual:

```text
RET = VMID + (RET_HG - VMID) / H_HG(f)
```

onde `H_HG(f)` é ganho complexo calibrado, não apenas 15,468 real.

O algoritmo escolhe o canal com melhor SNR sem clipping.

## Derivação de grandezas

Com:

```text
Z = R + jX
```

podem ser calculados:

```text
|Z|   = sqrt(R² + X²)
phase = atan2(X, R)
```

Modelo série:

```text
R = Re(Z)
L = X / (2*pi*f)            quando X > 0
C = -1 / (2*pi*f*X)         quando X < 0
```

Também podem ser derivados ESR, Q e D quando a leitura possuir confidence suficiente.

## Autorange

O objetivo é colocar `|ZREF|` na mesma ordem de magnitude do DUT, mantendo simultaneamente:

- sinal suficiente no ADC;
- corrente segura no AFE;
- headroom;
- baixa distorção;
- combinação range/frequência qualificada.

Sequência de troca:

```text
RANGE_EN=0 -> A0/A1/A2 -> dead time -> RANGE_EN=1 -> settling -> acquire
```

## Confidence gates

Uma leitura pode ser rejeitada ou marcada EXTENDED por:

- clipping;
- SNR insuficiente;
- amplitude de excitação fora da janela;
- instabilidade entre blocos;
- fase incoerente;
- DUT próximo demais de OPEN/SHORT para aquele range;
- combinação não qualificada;
- tensão residual detectada;
- saturação/corrente excessiva do AFE.

O firmware deve preferir retornar `LOW CONFIDENCE` a publicar precisão que não foi comprovada.
