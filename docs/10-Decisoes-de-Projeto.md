# Decisões de projeto consolidadas

Este documento registra decisões que já foram discutidas para evitar reabrir escolhas sem uma razão técnica nova.

## MCU

**STM32F103C8T6 Blue Pill** permanece como MCU da Rev.1.

RP2040 foi avaliado, mas a Rev.1 permanece STM32 pela arquitetura ADC/timers já consolidada e pelo custo de mudança do hardware.

## ADC

Sem ADC externo na Rev.1.

Os dois ADCs internos do STM32 são usados para aquisição. A qualidade final será obtida por sincronismo, DSP e calibração, não por adicionar um conversor caro antes de validar o protótipo.

## AFE

TLV9064 foi a opção original, mas a disponibilidade brasileira motivou a mudança para **2 × MCP6002-E/SN**.

Consequência: menor GBW e maior dependência de calibração de ganho/fase, especialmente no canal high-gain a 10 kHz.

## RREF switches

Ranges baixos: AO3400A.

Ranges altos: 2N7002 individual em SOT-23, dois por range. A escolha substitui 2N7002DW/SOT-363 para facilitar soldagem manual.

## Passivos

0805 é o encapsulamento mínimo preferido para R/C comuns. 1206 é usado onde tensão, potência, caminho de baixa impedância ou robustez justificam.

## Display

ILI9341 SPI, sem framebuffer completo em RAM.

UI usa renderização incremental e assets externos.

## Flash

Família W25Q em SPI convencional. A BOM atual usa W25Q64JVSSIQ, mas o driver deve reconhecer W25Q16/32/64/128 compatíveis.

Sem filesystem na primeira versão. Assets são empacotados em formato simples com tabela, offsets, tamanho, formato e CRC.

## Entrada do usuário

Três botões: UP, DOWN e OK. Encoder foi descartado para reduzir mecânica e hardware.

## Backlight e buzzer

PB0 controla o backlight.

PB1 controla um piezo passivo remoto, montado na carcaça, via BC817. O piezo não fica sobre a PCB analógica.

## Segurança

K1 é fail-safe e o hardware de detecção residual existe antes do AFE.

`D_TVS` e `R_TVS_LINK` permanecem DNP inicialmente.

O objetivo atual é tolerar/detectar tensão residual dentro de aproximadamente ±100 V, não medir alta tensão energizada.

## Alta tensão futura

Medição AC ~400 Vrms e DC 600–800 V foi discutida como possível evolução, mas **não faz parte da Rev.1**. Se implementada, deverá usar frontend e conectores próprios, novo isolamento/clearance e nova análise de segurança.

## 4 fios

A Rev.1 é dois fios. Kelvin/4-wire permanece uma possibilidade de revisão futura.

## Debug e programação

Não há necessidade de conector SWD dedicado na carrier board; os sinais continuam disponíveis no módulo Blue Pill.

A Rev.1 não pode usar o USB nativo porque PA11/PA12 foram reutilizados. UART e SWD/bootloader do módulo são caminhos de bring-up.
