# Bring-up da Rev.1

O objetivo é descobrir falhas com o menor número possível de componentes energizados.

## 0. Antes de soldar

- fotografar PCB nua;
- conferir orientação dos footprints críticos;
- medir ausência de curto entre +5V_SYS, +5V_A, +3V3 e GND;
- conferir continuidade de J_TEST até K1/SAFE;
- confirmar que D_TVS/R_TVS_LINK não serão montados.

## 1. Alimentação

Montar primeiro a seção de alimentação passiva.

Aplicar +5V_SYS em fonte limitada em corrente.

Verificar:

- +5V_SYS;
- +5V_A após RA;
- ausência de aquecimento;
- consumo anormal.

## 2. VMID e AFE

Montar MCP6002 e rede VMID.

Esperado:

```text
VMID_RAW ~1,65 V
VMID     ~1,65 V
```

Sem PWM, verificar VEXC e saídas sem saturação.

## 3. Blue Pill

Antes de inserir/alimentar o módulo:

- confirmar orientação física;
- conferir 5V/3V3/GND;
- firmware mínimo em estado SAFE.

**Não usar o Micro-USB como interface da Rev.1**, pois PA11/PA12 estão reutilizados.

Programação inicial pode usar ST-Link nos pads/header do próprio módulo ou bootloader ROM por USART1.

## 4. UART e GPIO

Primeiro firmware deve imprimir:

```text
boot
hw_rev
reset_cause
charger
battery
adc raw
```

Validar UP/DOWN/OK, PA15, K1/K2 commands sem bobinas se necessário.

## 5. TFT e Flash

- CS de ambos HIGH antes do SPI;
- ler JEDEC ID da W25Q;
- testar write/read em setor reservado;
- inicializar ILI9341;
- testar cores e texto;
- verificar que TFT MISO libera barramento com CS HIGH.

## 6. Buzzer e backlight

Testar fora de aquisição:

- brightness PWM;
- buzzer em 500 Hz, 1 kHz, 2 kHz;
- confirmar que não há reset ou ripple excessivo em rails.

## 7. Safety detector

Sem K1 em MEASURE:

- 0 V nos terminais;
- pequenas tensões DC progressivas usando fonte limitada;
- validar ADC_OV_HI/LO e polaridade;
- nunca começar o teste diretamente em 100 V.

Validar que CHG_VBUS impede K1 fisicamente.

## 8. Range bank

Com RANGE_EN=0, confirmar todos OFF.

Selecionar cada range individualmente, sem DUT e com excitação muito baixa.

Verificar one-hot e ausência de dois branches simultâneos.

## 9. Excitação

Começar com carga leve/range alto.

No osciloscópio:

- PWM carrier;
- FILT1/2/3;
- VEXC;
- THD/forma de onda;
- amplitude.

Só então avançar para 10 Ω com amplitude reduzida.

## 10. Aquisição

Capturar buffers crus e analisá-los no PC via UART antes de confiar no DSP embarcado.

Comparar:

- VEXC;
- VMID;
- RET_1X;
- RET_HG;
- fase relativa.

## 11. Primeiras cargas

Ordem recomendada:

1. resistor conhecido médio;
2. vários resistores por range;
3. capacitor conhecido;
4. indutor conhecido;
5. OPEN/SHORT.

Só depois iniciar autorange e calibração automática.
