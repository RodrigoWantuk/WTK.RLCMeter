# `measurement`

Núcleo metrológico do WTK.RLCMeter.

## Objetivo

Manter aquisição, DSP, cálculo de impedância, autorange e confidence independentes de UI e do hardware gráfico.

## Arquivos planejados

```text
measurement_types.h
acquisition.c/.h
phasor.c/.h
complex_math.c/.h
impedance.c/.h
autorange.c/.h
confidence.c/.h
calibration_apply.c/.h
measurement_engine.c/.h
```

## Fluxo

```text
raw ADC samples
   -> channel scaling
   -> synchronous I/Q / single-bin DFT
   -> calibrated complex phasors
   -> impedance equation
   -> derived R/C/L/ESR/Q/D
   -> confidence gates
   -> accept / retry / rerange / reject
```

## Equação central

```text
Vs = VEXC - VMID
Vx = RET  - VMID
Zx = ZREF * Vx / (Vs - Vx)
```

## High gain

O hardware atual usa ganho nominal aproximado de 15,47× em `RET_HG`, mas o código deve trabalhar com resposta complexa calibrada por frequência/range/amplitude.

## Testabilidade

`phasor`, `complex_math`, `impedance`, `autorange`, `confidence` e aplicação de calibração devem compilar em host tests sem HAL/CMSIS.

## Saída

Além do valor medido, o engine retorna metadados de qualidade: clipping, SNR, estabilidade, range/frequência/amplitude usados, canal 1X/HG, calibração aplicada e reason de retry/rerange quando houver.
