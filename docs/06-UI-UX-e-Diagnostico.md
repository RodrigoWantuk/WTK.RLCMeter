# UI, UX e diagnóstico

## Display

Controlador: ILI9341, 240×320, SPI.

Formato principal: RGB565.

Uma tela inteira em RGB565 ocupa:

```text
240 * 320 * 2 = 153600 bytes
```

Isso é maior que a RAM disponível do STM32F103C8. A UI é portanto incremental/streaming.

## Assets

Imagens, ícones e fontes podem residir na W25Q.

Fluxo:

```text
W25Q -> buffer pequeno RAM -> ILI9341
```

Buffer típico inicial: 512–2048 bytes.

### Asset pack

Formato simples planejado:

```text
header
asset table
asset data...
```

Cada entrada contém ao menos:

```text
id
offset
length
width
height
format
crc32
```

Sem FAT/LittleFS na primeira versão.

## Telas planejadas

### Startup

- logo WTK.RLCMeter;
- versão de firmware;
- progresso do self-test.

### Home / Measure

- grandeza principal R/L/C/Z;
- valor;
- frequência;
- range;
- estado AUTO/MANUAL;
- bateria;
- indicador de alimentação externa;
- confidence.

### Detail

- |Z|;
- fase;
- R e X;
- ESR/Q/D quando aplicável;
- comparação entre frequências;
- gráficos simples de resposta/defasagem.

### Calibration

Wizard para OPEN/SHORT/LOAD com instruções passo a passo.

### Diagnostics

Exibe valores reais úteis para bring-up:

```text
VMID
VEXC
RET_1X
RET_HG
ADC raw
ADC_OV_HI/LO
VBAT
NTC
range selecionado
K1/K2 state
CHG_DETIO
W25Q JEDEC ID
TFT status
```

### Console

Ring buffer de eventos:

```text
[0001.203] BOOT
[0001.215] FLASH EF4017
[0001.244] TFT OK
[0001.310] SAFE residual=0.08V
[0001.315] READY
```

## Controles

Três botões:

- UP;
- DOWN;
- OK.

Long press e repeat são tratados em software.

Exemplo de semântica:

- UP/DOWN: navegar/alterar;
- OK: confirmar/medir;
- OK longo: abrir menu/voltar para Home;
- UP+DOWN: atalho opcional de diagnóstico/calibração.

## Backlight

PB0 controla brilho por PWM.

Funções planejadas:

- brightness configurável;
- auto-dimming após inatividade;
- desligamento do backlight em sleep;
- possibilidade de duty fixo durante aquisição se necessário para reduzir ruído.

## Buzzer

Piezo passivo externo na carcaça.

PB1 controla BC817; o buzzer recebe +5V_SYS e low-side chaveado. Um resistor de 4,7 kΩ fica em paralelo com o piezo para descarga de sua capacitância.

Padrões sugeridos:

| Evento | Feedback |
|---|---|
| botão | click/tom curtíssimo opcional |
| resultado | dois tons ascendentes |
| erro | três tons curtos |
| residual perigoso | padrão de alerta |
| bateria baixa | tons descendentes |
| calibração concluída | sequência positiva |

Buzzer é sempre silenciado durante aquisição metrológica.

## Falha de assets

A UI deve possuir uma fonte bitmap mínima e uma tela de erro embutidas na Flash interna do MCU. Assim o equipamento ainda consegue mostrar diagnóstico básico se a W25Q estiver ausente/corrompida.
