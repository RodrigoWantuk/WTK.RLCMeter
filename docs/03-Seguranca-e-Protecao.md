# Segurança e proteção

## Escopo

WTK.RLCMeter Rev.1 é destinado a **componentes passivos desenergizados**. A rede SAFE permite sobreviver/detectar tensão residual dentro do envelope projetado, mas não transforma o equipamento em multímetro de alta tensão ou instrumento CAT-rated.

Nunca conectar diretamente à rede elétrica ou a um circuito energizado.

## Estado fail-safe

K1 desenergizado = SAFE.

```text
TEST_HI -> SAFE_HI
TEST_LO -> SAFE_LO
```

K1 energizado = MEASURE.

```text
TEST_HI -> RET
TEST_LO -> VMID
```

O firmware deve assumir SAFE em boot, reset, fault e brownout.

## Detector de tensão residual

Cada lado possui:

```text
SAFE_x -> 560k -> 560k -> 560k -> SENSE_x
                                  |
                                 27k
                                  |
                                 VMID
```

Com `RTOP = 1,68 MΩ` e `RLOW = 27 kΩ`:

```text
k = 27k / (1,68M + 27k) ≈ 0,015817
VSENSE = VMID + k * (VIN - VMID)
```

Para VMID ~1,65 V:

- +100 V -> ~3,21 V;
- 0 V -> ~1,62 V;
- -100 V -> ~0,04 V.

Isso define o envelope de observação aproximado. **O limiar para permitir fechar K1 deve ser muito menor que 100 V e será definido/validado em firmware e bancada.**

## Bleeder

```text
SAFE_HI -- 47k -- 47k -- SAFE_LO
```

Total: 94 kΩ.

O tempo de descarga depende da capacitância do DUT:

```text
tau = 94k * C_DUT
```

O firmware nunca deve assumir que um atraso fixo descarregou o componente; deve medir novamente a tensão residual.

## Proteção ADC

Os canais OV usam resistor série e BAT54S para limitar excursões no ADC. A rede de 1,68 MΩ limita fortemente a corrente de clamp em eventos de tensão residual.

## Intertravamento durante carga

`CHG_VBUS` atua em duas camadas:

1. **hardware:** QUSB_INH força a base de K1 para o estado que impede MEASURE;
2. **firmware:** divisor 10 kΩ / 12 kΩ gera `CHG_DETIO` em PA15.

A camada de firmware é informativa/política; o bloqueio de K1 não depende somente dela.

## TVS DNP

`D_TVS` e `R_TVS_LINK` formam uma contingência de proteção em paralelo com os terminais de teste.

Primeira montagem: ambos DNP.

Motivo: capacitância, leakage, não linearidade e dependência térmica do TVS podem degradar especialmente os ranges de alta impedância e capacitâncias pequenas.

O footprint existe para permitir comparação futura A/B depois de a metrologia baseline estar caracterizada.

## Quiet/safe fault policy

Qualquer uma destas condições deve abortar MEASURE e retornar K1 a SAFE:

- residual acima do limite;
- CHG_DETIO ativo;
- falha de ADC relevante;
- range inválido;
- watchdog/reset;
- erro interno de state machine;
- perda de condição de alimentação confiável.
