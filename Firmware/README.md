# Firmware

Firmware do WTK.RLCMeter para STM32F103C8T6.

## Objetivos de arquitetura

- determinismo na aquisição;
- estado seguro por default;
- módulos pequenos e testáveis;
- DSP desacoplado do HAL/MCU;
- UI sem bloquear aquisição;
- ausência de alocação dinâmica no caminho crítico;
- persistência versionada e com CRC;
- diagnóstico suficiente para bring-up sem depender de breakpoint.

## Stack planejada

Baseline proposto:

- C17;
- GNU Arm Embedded (`arm-none-eabi-gcc`);
- CMake;
- CMSIS + STM32CubeF1 HAL/LL;
- HAL para periféricos não críticos;
- LL/registradores quando necessário para timer/ADC/DMA;
- testes host-side para matemática, calibração, assets e state machine pura.

STM32CubeIDE pode ser usado como IDE/debugger, mas o repositório não deve depender de arquivos proprietários da IDE para compilar.

## Estrutura

```text
Firmware/
├── README.md
├── src/
│   ├── app/          # state machine, scheduler e orchestration
│   ├── bsp/          # clocks, GPIO, ADC, DMA, timers, SPI, UART
│   ├── drivers/      # ILI9341, W25Q, buttons
│   ├── hardware/     # relays, RREF, excitation, safety, battery, buzzer
│   ├── measurement/  # acquisition, I/Q, impedance, autorange, confidence
│   ├── storage/      # assets, settings, calibration, CRC/versioning
│   └── ui/           # screens, widgets, fonts, console, navigation
├── assets/           # imagens/fontes de origem antes de empacotar
├── config/           # parâmetros de build e defaults versionados
├── tests/            # testes host-side e fixtures
├── tools/            # asset packer, calibration tooling, scripts
└── third_party/      # dependências externas claramente isoladas
```

## Regras de dependência

```text
app
 ├── hardware
 ├── measurement
 ├── storage
 └── ui

hardware -> bsp
measurement -> tipos puros + acquisition abstraction
storage -> drivers/W25Q
ui -> drivers/ILI9341 + storage/assets

drivers -> bsp
bsp -> CMSIS/HAL/LL
```

`measurement` não deve depender diretamente de ILI9341, W25Q ou GPIO.

## Scheduler

Sem RTOS inicialmente. O `main()` executa um loop cooperativo com tarefas curtas:

```text
while (1)
  safety_poll
  input_poll
  app_step
  measurement_step
  ui_step
  storage_step
  diagnostics_step
```

Interrupções são reservadas para temporização e movimentação de dados:

- timer trigger de ADC;
- DMA half/full complete;
- UART RX quando necessário;
- buzzer timebase se implementado por toggle temporizado.

Nada pesado roda em ISR.

## Timers planejados

- **TIM1 CH1 / PA8:** carrier `PWM_EXC`.
- **timer dedicado de trigger:** aquisição ADC, candidato TIM2.
- **TIM3 CH3 / PB0:** PWM de backlight.
- **TIM4:** candidato para timebase do buzzer, togglando PB1 por ISR; evita obrigar backlight e buzzer a compartilhar a mesma frequência de TIM3.
- SysTick: tick de sistema de baixa resolução, não usado para amostragem metrológica.

A atribuição final deve ser validada junto ao código gerado/clock tree.

## Estado seguro no boot

Antes de inicializar display ou storage:

```text
RANGE_EN = 0
K1_CMD    = 0
K2_CMD    = 0
BUZZER    = off
TFT_CS    = 1
FLASH_CS  = 1
PWM_EXC   = neutral/off
```

Em seguida o firmware desabilita JTAG mantendo SWD para liberar PA15/PB3/PB4.

## Fases de implementação

1. BSP e UART.
2. TFT + Flash + buttons.
3. diagnóstico e state machine SAFE.
4. ranges/relés.
5. PWM_EXC e ADC/DMA.
6. I/Q e cálculo de Z.
7. autorange/confidence.
8. calibração persistente.
9. UI final e gráficos.
10. qualificação metrológica.

Consulte [`../docs/04-Arquitetura-de-Firmware.md`](../docs/04-Arquitetura-de-Firmware.md).
