# `bsp`

Board Support Package do STM32F103C8T6.

## Responsabilidades

- clock tree;
- pin mux e estados seguros de GPIO;
- desabilitar JTAG mantendo SWD para liberar PA15/PB3/PB4;
- ADC1/ADC2;
- DMA;
- TIM1/TIM2/TIM3/TIM4;
- SPI2;
- USART1;
- watchdog;
- monotonic time e reset reason.

## Arquivos planejados

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

## Regras

- somente esta camada conhece handles HAL/LL e registradores do STM32;
- callbacks de ISR devem ser mínimos;
- defaults de boot precisam deixar `RANGE_EN=0`, K1/K2 desligados e CSs SPI inativos;
- pinout documentado em `docs/05-Pinout-e-Interfaces.md` deve ser tratado como contrato de hardware.
