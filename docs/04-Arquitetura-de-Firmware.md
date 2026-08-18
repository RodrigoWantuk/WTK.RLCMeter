# Arquitetura de firmware

## Princípios

O firmware é organizado como firmware de instrumento, não como demo de microcontrolador:

- SAFE é o estado default;
- o caminho de aquisição é determinístico;
- UI e storage não bloqueiam a medição;
- DSP é isolado de periféricos;
- falha de qualquer pré-condição retorna K1 a SAFE;
- configuração/calibração são versionadas e validadas por CRC.

## State machine principal

```text
BOOT
  |
  v
SELF_TEST
  |
  v
SAFE_CHECK <-----------------------------+
  |                                      |
  +-- residual/charger/fault --> WAIT ---+
  |
  v
READY
  |
  v
PREPARE_RANGE
  |
  v
PRE_EXCITATION
  |
  v
K1_MEASURE
  |
  v
SETTLING
  |
  v
ACQUIRE
  |
  v
K1_SAFE
  |
  v
PROCESS
  |
  +--> RETRY/RERANGE
  |
  v
RESULT
```

O DUT deve permanecer conectado ao AFE pelo menor tempo necessário.

## Boot

Ordem:

1. clock e watchdog;
2. GPIOs em estado seguro;
3. desabilitar JTAG mantendo SWD para liberar PA15/PB3/PB4;
4. UART de diagnóstico;
5. SPI;
6. W25Q;
7. ILI9341;
8. ADC/DMA/timers;
9. leitura de calibração/configuração;
10. self-test;
11. SAFE_CHECK.

Uma falha de TFT ou Flash não deve, por si só, permitir uma medição insegura. Segurança é independente da UI.

## BSP

`bsp` encapsula:

- clock tree;
- GPIO;
- SPI2;
- USART1;
- ADC1/ADC2;
- DMA;
- TIM1/TIM2/TIM3/TIM4;
- watchdog;
- monotonic time.

O restante do firmware não deve escrever registradores GPIO diretamente.

## Drivers

### ILI9341

API mínima:

```text
init
reset
read_id/status
set_rotation
set_window
fill
write_pixels_rgb565
draw_bitmap
draw_glyph
```

Não haverá framebuffer 240×320×16-bit na RAM.

### W25Q

API genérica:

```text
init/read_jedec_id
read
fast_read
write_enable
page_program
sector_erase
read_status
wait_ready
```

Reconhecer ao menos W25Q16/32/64/128 de 3,3 V.

### Buttons

Debounce e eventos:

```text
PRESS
RELEASE
LONG_PRESS
REPEAT
```

## Hardware services

APIs sem exposição de GPIO:

```text
relay_set_safe()
relay_set_measure()
range_disable()
range_select(range)
excitation_configure(freq, amplitude)
excitation_stop()
charger_connected()
battery_read()
buzzer_play(pattern)
backlight_set(percent)
```

## Aquisição

O módulo de acquisition entrega blocos sincronizados de amostras crus. Ele não calcula impedância.

O DSP recebe buffers e metadados:

```text
frequency
sample_rate
range
amplitude
channel timing
calibration key
```

e retorna fasores complexos, qualidade e estatísticas.

## Quiet mode

Durante janelas críticas:

- buzzer sempre off;
- UI não escreve grandes blocos SPI;
- Flash não é acessada salvo necessidade explícita;
- backlight pode permanecer em duty fixo ou ser temporariamente fixado se testes mostrarem acoplamento;
- UART logging de alto volume é suspenso.

O resultado é renderizado depois que K1 volta a SAFE.

## TIM3 e buzzer

PB0 e PB1 pertencem a canais do TIM3, portanto canais PWM de hardware compartilhariam o mesmo prescaler/ARR e, consequentemente, a mesma frequência base.

Para manter backlight independente dos tons, o baseline recomendado é:

- TIM3_CH3: PWM contínuo do backlight em PB0;
- TIM4: interrupção/timebase do buzzer;
- PB1: GPIO togglado no ritmo do tom.

Assim tons podem variar sem alterar a frequência do backlight. O piezo não toca durante aquisição.

## Persistência

A Flash externa é dividida logicamente em:

- asset pack;
- calibration records;
- settings;
- optional diagnostic/event data.

Sem filesystem na primeira versão.

Records possuem:

```text
magic
schema_version
hardware_revision
sequence
payload_length
crc32
payload
```

Configuração/calibração deve usar dois slots ou journal simples para sobreviver a interrupção de energia durante escrita.

## Diagnóstico

Logs compactos em ring buffer podem ser exibidos no TFT e enviados pela UART.

Níveis:

```text
ERROR
WARN
INFO
DEBUG
TRACE (build de laboratório)
```

O firmware deve ser utilizável em bring-up mesmo sem debugger com breakpoint.
