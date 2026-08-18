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

If HSE/PLL startup fails, the BSP restores a deterministic HSI-safe clock tree before reporting fallback to HSI rather than silently claiming the 72 MHz baseline. This matters for later peripherals that derive baud rates from `bsp_clock_summary_t`: the recorded `PCLK1`/`PCLK2` values must match hardware after every fallback path.

Public APIs are project-level functions such as `bsp_time_now_ms()`, `bsp_uart_write()`, `bsp_reset_get_reason()`, and `bsp_watchdog_service()`. Higher layers should not reach around these APIs to manipulate RCC/GPIO/USART/IWDG registers directly.

Phase 02 implementation remains `REQUIRES_BENCH_VALIDATION` until real hardware confirms UART output, reset cause reporting, SWD preservation after JTAG remap, watchdog reset behavior, and safe pin levels for K1/K2/RANGE_EN/buzzer/excitation.

## Phase 03 additions

Phase 03 adds SPI2 and timer support behind BSP APIs:

```text
bsp_spi2_init()
bsp_spi2_configure()
bsp_spi2_transfer()
bsp_quiet_requested()
bsp_timer3_pwm_ch3_init()
bsp_timer3_pwm_ch3_set_duty()
bsp_timer4_buzzer_init()
bsp_timer4_buzzer_start()
bsp_timer4_buzzer_stop()
```

Higher layers continue to avoid STM32 register access directly. Drivers and hardware services use these BSP calls plus limited non-safety GPIO outputs for TFT/Flash chip selects, TFT control pins, backlight, and buzzer only.

SPI2 starts in mode 0 at `PCLK1 / 8`. PB13/PB15 are configured as alternate-function push-pull outputs at 10 MHz so the conservative 4.5 MHz baseline clock is not driven through a 2 MHz GPIO mode. The setting deliberately avoids the 50 MHz output mode until the physical bus is qualified.

`bsp_quiet_request()` is the low-level shared peripheral gate used by the hardware layer. New SPI bus acquisitions return `BSP_STATUS_BUSY` while quiet mode is requested. Application and acquisition code must use the hardware-layer `hw_peripherals_request_quiet()` wrapper so active buzzer tones are stopped at the same time and backlight PWM remains unchanged. The SPI bus no longer exposes an independent public quiet-mode setter.

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
