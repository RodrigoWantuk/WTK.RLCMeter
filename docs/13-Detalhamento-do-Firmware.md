# Detalhamento do firmware

Este documento define a decomposição planejada do firmware do WTK.RLCMeter, os contratos entre módulos e a ordem de implementação.

## Objetivo arquitetural

O firmware deve se comportar como firmware de instrumento:

- segurança e estado elétrico têm prioridade sobre UI;
- aquisição precisa ser determinística;
- DSP deve ser testável fora do STM32;
- hardware específico deve ficar confinado ao BSP/drivers;
- UI e storage nunca podem bloquear o caminho metrológico;
- falha de software deve tender ao estado SAFE;
- calibração e configuração precisam ser versionadas.

## Camadas

```text
┌─────────────────────────────────────────┐
│ app                                     │
│ state machine / orchestration / policy  │
├──────────────┬──────────────┬───────────┤
│ measurement  │ storage      │ ui        │
├──────────────┴──────┬───────┴───────────┤
│ hardware services  │ drivers            │
├─────────────────────┴───────────────────┤
│ bsp / CMSIS / HAL / LL                  │
└─────────────────────────────────────────┘
```

## `src/app`

Responsável por política global, não por detalhes de GPIO.

Componentes planejados:

```text
app_state_machine.c/.h
app_events.c/.h
app_context.c/.h
app_scheduler.c/.h
app_faults.c/.h
app_version.c/.h
```

### Estado principal

```text
BOOT
SELF_TEST
SAFE_CHECK
WAIT_SAFE
READY
PREPARE_RANGE
PRE_EXCITATION
K1_MEASURE
SETTLING
ACQUIRE
K1_SAFE
PROCESS
RESULT
FAULT
```

O estado global é explícito e serializável para diagnóstico.

### Eventos

Exemplos:

```text
APP_EVENT_BUTTON_UP
APP_EVENT_BUTTON_OK
APP_EVENT_BUTTON_DOWN
APP_EVENT_MEASURE_REQUEST
APP_EVENT_DMA_BLOCK_READY
APP_EVENT_MEASUREMENT_DONE
APP_EVENT_RESIDUAL_VOLTAGE
APP_EVENT_CHARGER_CONNECTED
APP_EVENT_LOW_BATTERY
APP_EVENT_FAULT
```

O loop principal despacha eventos; ISR apenas produz flags/eventos mínimos.

## `src/bsp`

Única camada autorizada a depender fortemente do STM32F1.

Arquivos planejados:

```text
bsp_clock.c/.h
bsp_gpio.c/.h
bsp_adc.c/.h
bsp_dma.c/.h
bsp_timer.c/.h
bsp_spi.c/.h
bsp_uart.c/.h
bsp_watchdog.c/.h
bsp_time.c/.h
bsp_reset.c/.h
```

### Responsabilidades

- clock tree;
- configuração de JTAG/SWD;
- GPIO safe defaults;
- ADC1/ADC2;
- DMA;
- TIM1/TIM2/TIM3/TIM4;
- SPI2;
- USART1;
- watchdog;
- monotonic clock;
- reset reason.

## `src/drivers`

Drivers de dispositivo reutilizáveis, sem política do instrumento.

Arquivos planejados:

```text
ili9341.c/.h
w25q.c/.h
buttons.c/.h
```

Possíveis auxiliares:

```text
spi_bus.c/.h
crc32.c/.h
```

### ILI9341

API mínima:

```c
bool ili9341_init(void);
void ili9341_set_rotation(uint8_t rotation);
void ili9341_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void ili9341_fill(uint16_t rgb565);
void ili9341_write_pixels(const uint16_t *pixels, size_t count);
bool ili9341_read_id(uint32_t *id);
```

O driver não conhece telas ou unidades de medição.

### W25Q

API mínima:

```c
bool w25q_init(void);
bool w25q_read_jedec_id(uint32_t *id);
bool w25q_read(uint32_t address, void *dst, size_t size);
bool w25q_fast_read(uint32_t address, void *dst, size_t size);
bool w25q_page_program(uint32_t address, const void *src, size_t size);
bool w25q_sector_erase(uint32_t address);
bool w25q_wait_ready(uint32_t timeout_ms);
```

O driver deve reconhecer ao menos densidades W25Q16/32/64/128 compatíveis em 3,3 V.

## `src/hardware`

Encapsula hardware específico do instrumento e aplica sequências seguras.

Arquivos planejados:

```text
hw_safety.c/.h
hw_relays.c/.h
hw_range.c/.h
hw_excitation.c/.h
hw_power.c/.h
hw_battery.c/.h
hw_temperature.c/.h
hw_backlight.c/.h
hw_buzzer.c/.h
```

### Segurança

Contrato sugerido:

```c
typedef struct {
    bool charger_connected;
    bool residual_present;
    bool supply_ok;
    bool adc_ok;
    bool range_ok;
} safety_status_t;

bool safety_measure_allowed(const safety_status_t *status);
void safety_force_safe(void);
```

Nenhum chamador externo deve energizar K1 diretamente.

### Range

```c
typedef enum {
    RREF_10R,
    RREF_100R,
    RREF_1K,
    RREF_10K,
    RREF_100K,
    RREF_1M,
} rref_range_t;

bool range_select(rref_range_t range);
void range_disable(void);
```

`range_select()` implementa:

```text
RANGE_EN=0
A0/A1/A2=new range
wait dead-time
RANGE_EN=1
```

### Excitação

```c
typedef struct {
    uint32_t frequency_hz;
    uint32_t carrier_hz;
    uint16_t amplitude_mv_rms;
} excitation_config_t;
```

A API rejeita combinações proibidas, como 500 mVrms com RREF de 10 Ω.

## `src/measurement`

É o núcleo metrológico e deve ser majoritariamente puro/testável no host.

Arquivos planejados:

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

### Tipos centrais

```c
typedef struct {
    float re;
    float im;
} complexf_t;

typedef struct {
    complexf_t vexc;
    complexf_t vmid;
    complexf_t ret;
} phasor_set_t;

typedef struct {
    complexf_t z;
    float magnitude;
    float phase_rad;
    float resistance;
    float reactance;
    float capacitance_f;
    float inductance_h;
    float esr;
    float q;
    float d;
} measurement_result_t;
```

Os tipos exatos podem mudar, mas a ideia é impedir que formatos de UI contaminem o cálculo.

### Aquisição

`acquisition` controla a sessão de ADC/DMA através de uma abstraction do BSP.

Metadados necessários:

```text
frequency
sample_rate
cycles
samples_per_cycle
RREF
excitation amplitude
RET channel 1X/HG
ADC timing/skew metadata
calibration key
```

### Fasores

Baseline: detecção síncrona / DFT de um único bin.

Para cada canal:

```text
I = Σ x[n] cos(ωn)
Q = Σ x[n] sin(ωn)
V = scale * (I + jQ)
```

Janelas devem conter número inteiro de ciclos sempre que possível.

### Impedância

```text
Vs = VEXC - VMID
Vx = RET  - VMID
Zx = ZREF * Vx / (Vs - Vx)
```

`ZREF` é complexo/calibrado quando necessário.

### Canal high-gain

Hardware atual:

```text
G_HG_nominal = 1 + 68k / 4.7k ≈ 15.47
```

O DSP usa `H_HG(f, range, amplitude)` calibrado, não apenas o ganho DC nominal.

### Autorange

Entrada:

- estimativa atual de Z;
- clipping;
- SNR;
- current/headroom;
- frequency/amplitude;
- qualification map.

Saída:

- aceitar;
- trocar RREF;
- trocar 1X/HG;
- trocar frequência;
- trocar amplitude;
- repetir;
- rejeitar.

## `src/storage`

Arquivos planejados:

```text
storage_layout.c/.h
asset_store.c/.h
settings_store.c/.h
calibration_store.c/.h
record_store.c/.h
```

### Layout lógico

Exemplo inicial:

```text
0x000000  superblock / manifest
0x001000  settings slot A
0x002000  settings slot B
0x010000  calibration region
0x100000  asset pack
...       reservado
```

Os endereços reais só devem ser congelados depois de escolher a densidade mínima suportada.

### Record format

```text
magic
schema_version
hardware_revision
record_type
sequence
payload_length
crc32
payload
```

Leitura sempre valida magic/version/size/CRC.

## `src/ui`

Arquivos planejados:

```text
ui_core.c/.h
ui_theme.c/.h
ui_format.c/.h
ui_navigation.c/.h
ui_widgets.c/.h
screen_startup.c/.h
screen_measure.c/.h
screen_details.c/.h
screen_graph.c/.h
screen_settings.c/.h
screen_calibration.c/.h
screen_diagnostics.c/.h
```

### Regras

- nenhuma tela bloqueia;
- nenhum `delay()` para animação;
- render dirty-regions quando possível;
- grandes bitmaps em streaming da W25Q;
- unidade e prefixo SI formatados em camada própria;
- lógica de medição não depende da tela ativa.

## Asset pack

Formato simples sugerido:

```text
header
asset_count
asset_table[]
blob data...
```

Cada entrada:

```text
id
offset
size
width
height
format
flags
crc32
```

Formatos candidatos:

- RGB565 raw;
- RGB565 + RLE simples;
- alpha-mask 1/4/8 bit para ícones/fontes.

Ferramenta host converte PNG/fontes para o formato do firmware.

## Configuração

Defaults compilados devem existir mesmo sem Flash válida.

Exemplos:

```text
backlight brightness
auto-dim timeout
buzzer enabled
preferred display mode
measurement auto/manual policy
log level
```

Nenhum setting pode desabilitar intertravamentos de segurança em builds normais.

## Calibração

Chave conceitual:

```text
hardware_revision
frequency
RREF
excitation amplitude
RET channel
calibration type
```

O formato deve permitir evolução sem invalidar toda a Flash por mudança de struct C.

## Testes host-side

Obrigatórios para:

- complex math;
- DFT/fasores;
- equação de impedância;
- OPEN/SHORT/load correction;
- autorange;
- confidence gates;
- CRC e parsing de records;
- asset manifest;
- state machine pura.

Vetores sintéticos devem cobrir R, C e L ideais e casos com ruído/clipping.

## Observabilidade

Cada sessão de medição deve poder produzir diagnóstico compacto:

```text
session id
range
frequency
amplitude
samples
VEXC phasor
RET phasor
channel used
clipping
SNR
calibration id
result
confidence
retry/rerange reason
```

Em Release, os dados podem ser reduzidos; em Lab, devem ser acessíveis por UART/TFT.

## Política de faults

Faults críticos:

- residual voltage;
- charger connected durante tentativa de MEASURE;
- ADC/DMA inconsistente;
- invalid range state;
- brownout/supply invalid;
- watchdog/reset;
- impossible state transition.

Ação baseline:

```text
stop excitation
RANGE_EN=0
K1_SAFE
K2 safe/default
buzzer off
record fault
show fault when UI is available
```

## Ordem recomendada de implementação

### Fase 1 — platform

- CMake/toolchain;
- startup/CMSIS;
- clock/GPIO;
- UART;
- watchdog/time.

### Fase 2 — UI peripherals

- SPI2;
- W25Q;
- ILI9341;
- buttons;
- backlight;
- buzzer.

### Fase 3 — safety/hardware

- battery/NTC/CHG_VBUS;
- residual ADC;
- K1;
- K2/R0_BANK policy;
- RREF switching.

### Fase 4 — metrology path

- PWM_EXC;
- ADC1/ADC2;
- timer trigger;
- DMA;
- deterministic buffers.

### Fase 5 — DSP

- phasor extraction;
- calibrated channels;
- Z calculation;
- R/C/L derivation;
- confidence.

### Fase 6 — intelligence

- autorange;
- retry/rerange;
- calibration workflow;
- qualification map.

### Fase 7 — product experience

- final screens;
- graphs;
- asset pack;
- settings;
- power policy;
- polished diagnostics.
