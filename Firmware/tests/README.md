# `tests`

Testes host-side e vetores de validação do firmware.

## Prioridades

- matemática complexa;
- DFT/detecção síncrona;
- equação de impedância;
- derivação R/C/L;
- aplicação de calibração;
- autorange;
- confidence gates;
- parsing/CRC de records;
- state machine pura;
- manifest de assets.

## Vetores

Manter fixtures para R, C e L ideais, além de casos com ruído, clipping, offset, skew e respostas próximas de OPEN/SHORT.

Testes host-side não substituem qualificação em bancada, mas impedem regressões matemáticas e de state machine antes de gravar o MCU.
