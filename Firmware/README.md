# Firmware

Firmware do WTK.RLCMeter para **STM32F103C8T6 / Blue Pill**.

Este diretório contém a arquitetura e, gradualmente, a implementação do firmware do instrumento. O objetivo é manter aquisição metrológica determinística, segurança fail-safe e UI responsiva sem acoplar DSP a periféricos específicos.

## Stack baseline

- C17;
- GNU Arm Embedded (`arm-none-eabi-gcc`);
- CMake;
- CMSIS + STM32CubeF1 HAL/LL;
- HAL para periféricos não críticos;
- LL/registradores quando necessário em timer, ADC e DMA;
- sem RTOS inicialmente;
- testes host-side para código puro;
- nenhuma alocação dinâmica no caminho crítico de aquisição.

STM32CubeIDE pode ser usado como IDE/debugger, mas o build do repositório deve permanecer reproduzível por linha de comando.

## Estrutura

```text
Firmware/
├── README.md
├── assets/              # fontes de imagens/fontes antes do empacotamento
├── config/              # defaults, feature flags e parâmetros versionados
├── src/
│   ├── app/             # state machine e orchestration
│   ├── bsp/             # binding STM32 e clock/periféricos
│   ├── drivers/         # ILI9341, W25Q, buttons e drivers de dispositivo
│   ├── hardware/        # serviços seguros de relé/range/power/buzzer/etc.
│   ├── measurement/     # acquisition, DSP, Z, autorange e confidence
│   ├── storage/         # assets, settings e calibração persistente
│   └── ui/              # screens, widgets, navigation e console
├── tests/               # testes host-side, fixtures e vetores conhecidos
├── third_party/         # dependências externas isoladas
└── tools/               # asset packer e ferramentas de calibração/análise
```

Cada subdiretório possui seu próprio `README.md` com responsabilidades, dependências permitidas e arquivos planejados.

## Regras de dependência

```text
app
 ├── hardware
 ├── measurement
 ├── storage
 └── ui

hardware -> bsp
measurement -> acquisition abstraction + tipos puros
storage -> drivers/W25Q
ui -> drivers/ILI9341 + storage/assets

drivers -> bsp
bsp -> CMSIS/HAL/LL
```

Regras obrigatórias:

- `measurement` não inclui headers de ILI9341, W25Q ou GPIO;
- `ui` não aciona K1, K2 ou `RANGE_EN` diretamente;
- somente `hardware` decide sequências de relés/ranges;
- somente `bsp` conhece detalhes de pin mux, registers e handles HAL;
- ISR apenas move/sinaliza dados; processamento pesado ocorre no loop cooperativo;
- toda operação que possa colocar o DUT em MEASURE possui caminho explícito de abort para SAFE.

## Scheduler cooperativo

Sem RTOS na primeira implementação:

```text
while (1)
{
    safety_poll();
    input_poll();
    app_step();
    measurement_step();
    ui_step();
    storage_step();
    diagnostics_step();
    watchdog_service();
}
```

As funções `*_step()` devem ser curtas e não bloqueantes. Operações demoradas — erase de Flash, animações, atualização de grandes regiões do TFT — são fragmentadas em estados.

## Interrupções

Uso planejado:

- timer de trigger de ADC;
- DMA half/full complete;
- UART RX, se necessário;
- timebase do buzzer quando usado em software;
- SysTick apenas como relógio de sistema de baixa resolução.

Nenhuma transformação complexa, renderização ou acesso de storage roda dentro de ISR.

## Timers

Baseline:

- **TIM1_CH1 / PA8** — carrier de `PWM_EXC`;
- **TIM2** — candidato a trigger determinístico da aquisição;
- **TIM3_CH3 / PB0** — PWM contínuo do backlight;
- **PB1** — saída do buzzer; embora seja TIM3_CH4, backlight e buzzer não podem usar frequências independentes no mesmo ARR/prescaler;
- **TIM4** — candidato a timebase para toggle de PB1 por software, preservando frequência independente do backlight.

O mapeamento final deve ser congelado após validação do clock tree e do esquema de amostragem.

## Estado seguro no boot

Antes de inicializar TFT, Flash ou carregar configurações:

```text
RANGE_EN = 0
K1_CMD    = 0
K2_CMD    = 0
BUZZER    = off
TFT_CS    = 1
FLASH_CS  = 1
PWM_EXC   = neutral/off
```

Depois disso:

1. configurar clock e watchdog;
2. configurar GPIOs em estado seguro;
3. desabilitar JTAG mantendo SWD, liberando PA15/PB3/PB4;
4. iniciar UART;
5. iniciar SPI, W25Q e ILI9341;
6. iniciar ADC/DMA/timers;
7. validar configuração/calibração persistente;
8. executar self-test;
9. entrar em `SAFE_CHECK`.

Falha de TFT ou Flash nunca autoriza medição insegura.

## State machine do instrumento

```text
BOOT
  |
  v
SELF_TEST
  |
  v
SAFE_CHECK <------------------------------+
  |                                       |
  +-- residual/charger/fault --> WAIT ----+
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
  +--> RETRY / RERANGE
  |
  v
RESULT
```

O princípio é simples: o DUT permanece conectado ao AFE pelo menor tempo necessário e o processamento/renderização acontece preferencialmente após retorno a SAFE.

## Aquisição e DSP

Fluxo planejado:

1. configurar frequência/amplitude de excitação;
2. selecionar RREF com `RANGE_EN=0` durante a comutação;
3. aguardar dead-time e settling;
4. energizar K1 somente após safety gates;
5. timer dispara ADCs em cadência determinística;
6. DMA recebe blocos;
7. DSP calcula componentes I/Q / DFT de bin único;
8. formar fasores calibrados de VEXC, VMID e RET;
9. calcular impedância complexa;
10. aplicar confidence gates e, se necessário, rerange/retry;
11. retornar K1 a SAFE antes de UI pesada.

O canal `RET_HG` atual possui ganho nominal `1 + 68k/4,7k ≈ 15,47×`. O firmware deve usar resposta complexa calibrada, nunca apenas essa constante nominal.

## Autorange

A decisão de faixa considera simultaneamente:

- magnitude estimada do DUT;
- clipping de `RET_1X` e `RET_HG`;
- SNR;
- corrente no caminho de excitação;
- frequência;
- amplitude permitida;
- combinações qualificadas em calibração;
- proximidade de OPEN/SHORT.

Troca segura:

```text
RANGE_EN=0
set A0/A1/A2
wait dead-time
RANGE_EN=1
wait settling
```

O range de 10 Ω não deve usar excitação de 500 mVrms.

## Quiet mode

Durante aquisição crítica:

- buzzer off;
- nenhuma escrita grande no TFT;
- nenhuma leitura/erase/program de Flash desnecessária;
- logging UART volumoso suspenso;
- backlight permanece com duty estável; se houver acoplamento medido, poderá ser temporariamente congelado em condição qualificada.

## Persistência

Sem filesystem inicialmente. A W25Q é particionada logicamente em:

- asset pack;
- calibration records;
- settings;
- diagnóstico/eventos opcionais.

Records persistentes devem conter, no mínimo:

```text
magic
schema_version
hardware_revision
sequence
payload_length
crc32
payload
```

Settings e calibração usam dois slots ou journal simples para tolerar perda de energia durante atualização.

## Assets

O TFT 240×320 RGB565 exige 153,6 kB para um framebuffer completo, acima da RAM disponível na Blue Pill. Portanto:

- não existe framebuffer full-screen;
- desenho é incremental;
- bitmaps são lidos da W25Q em blocos pequenos e transmitidos diretamente ao ILI9341;
- assets podem ser pré-convertidos para RGB565/RLE ou formatos simples apropriados;
- SPI é compartilhado entre TFT e Flash com CS independentes.

## Diagnóstico

Logs compactos em ring buffer:

```text
ERROR
WARN
INFO
DEBUG
TRACE   # somente builds de laboratório
```

O diagnóstico deve permitir bring-up sem breakpoint, mostrando pelo TFT/UART:

- estado da state machine;
- valores ADC crus e convertidos;
- range e RREF;
- K1/K2;
- `CHG_VBUS`;
- bateria/NTC;
- JEDEC ID da Flash;
- status do TFT;
- clipping/SNR/confidence;
- códigos de fault.

## Build profiles planejados

- `Debug` — asserts, logs e símbolos completos;
- `Lab` — instrumentação extra, TRACE e telas de engenharia;
- `Release` — comportamento final, logs reduzidos;
- `HostTests` — módulos puros compilados no host sem STM32.

## Fases de implementação

1. build system + BSP + UART;
2. TFT + Flash + botões;
3. safety state machine e diagnóstico;
4. ranges + K1/K2;
5. PWM_EXC + ADC/DMA;
6. I/Q + cálculo complexo de Z;
7. autorange + confidence;
8. calibração persistente;
9. UI final, gráficos e assets;
10. qualificação metrológica e fechamento da matriz válida.

Veja também:

- [`../docs/04-Arquitetura-de-Firmware.md`](../docs/04-Arquitetura-de-Firmware.md)
- [`../docs/13-Detalhamento-do-Firmware.md`](../docs/13-Detalhamento-do-Firmware.md)
