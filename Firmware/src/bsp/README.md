# `bsp`

Board Support Package for the STM32F103C8T6 target.

## Responsibilities

- clock tree;
- GPIO pin-mux and safe defaults;
- disable JTAG while preserving SWD to free PA15/PB3/PB4;
- ADC1/ADC2;
- DMA;
- TIM1/TIM2/TIM3/TIM4;
- SPI2;
- USART1;
- watchdog;
- monotonic time;
- reset reason.

## Planned files

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

## Rules

- this is the primary layer allowed to own STM32 HAL/LL handles and register details;
- ISR callbacks remain minimal;
- boot defaults must keep `RANGE_EN=0`, K1/K2 de-energized, buzzer off, and SPI chip selects inactive;
- `docs/05-Pinout-and-Interfaces.md` is the firmware pinout contract;
- platform code should be deterministic and testable at API boundaries even when it cannot run in host tests.
