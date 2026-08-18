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

## Phase 02 baseline

The BSP owns the first hardware-facing boot foundation:

```text
safe GPIO defaults
JTAG disabled / SWD preserved
8 MHz HSE -> 72 MHz SYSCLK clock setup
1 kHz SysTick timebase
USART1 diagnostics on PA9/PA10 at 115200 8N1
independent watchdog service
reset-cause capture and clearing
```

If HSE/PLL startup fails, the BSP keeps the instrument in safe GPIO state and reports fallback to HSI rather than silently claiming the 72 MHz baseline.

Public APIs are project-level functions such as `bsp_time_now_ms()`, `bsp_uart_write()`, `bsp_reset_get_reason()`, and `bsp_watchdog_service()`. Higher layers should not reach around these APIs to manipulate RCC/GPIO/USART/IWDG registers directly.

Phase 02 implementation remains `REQUIRES_BENCH_VALIDATION` until real hardware confirms UART output, reset cause reporting, SWD preservation after JTAG remap, watchdog reset behavior, and safe pin levels for K1/K2/RANGE_EN/buzzer/excitation.

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
