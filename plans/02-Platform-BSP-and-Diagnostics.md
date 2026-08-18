# 02 — Platform BSP and Diagnostics Foundation

STATUS: IMPLEMENTED_REQUIRES_BENCH_VALIDATION

## Goal

Create the STM32 platform layer required by all later hardware work, with **safe boot behavior first** and diagnostics available before complex peripherals are introduced.

## Prerequisites

- Phase 01 complete;
- embedded CMake target builds reproducibly;
- CMSIS/device/HAL strategy frozen;
- linker/startup path proven.

## In scope

- clock tree;
- GPIO safe defaults;
- JTAG disable / SWD preservation;
- USART1 diagnostics;
- monotonic time base;
- watchdog;
- reset-cause reporting;
- common BSP error/status conventions;
- minimal cooperative scheduler/application shell;
- firmware/version banner.

## Out of scope

- SPI/TFT/W25Q;
- relay energization as a functional feature;
- ADC metrology acquisition;
- final state machine;
- DSP.

## Task 1 — Freeze clock-tree baseline

Determine and document:

- HSE/HSI source actually expected on the Blue Pill baseline;
- SYSCLK target;
- AHB/APB1/APB2 clocks;
- timer clock implications when APB prescalers are not 1;
- ADC clock constraints for later phases;
- SysTick or alternate low-resolution timebase.

Do not optimize clocks for maximum speed before verifying timer/ADC implications.

Deliver a clock summary that later phases can reference.

## Task 2 — Safe GPIO initialization

Implement `bsp_gpio` so the earliest practical initialization guarantees safe output intent.

Required initial states:

```text
RANGE_EN = 0
K1_CMD    = 0
K2_CMD    = 0
BUZZER    = off
TFT_CS    = 1
FLASH_CS  = 1
PWM_EXC   = inactive / safe
```

Pins not yet used should not be configured in ways that could energize external hardware.

Verify output polarity against schematic, not naming assumptions.

## Task 3 — JTAG/SWD remap

Disable JTAG while retaining SWD so PA15/PB3/PB4 can later be GPIO.

Acceptance:

- PA13/PA14 remain SWD;
- no code path disables SWD unintentionally;
- remap occurs before application use of PA15/PB3/PB4.

## Task 4 — USART1 diagnostics

Implement a small non-fragile UART diagnostic layer on PA9/PA10.

Initial boot output should include:

```text
WTK.RLCMeter
firmware version
build profile
hardware revision constant
reset cause
clock summary
boot state
```

Requirements:

- do not block indefinitely if UART is disconnected;
- keep formatting simple;
- no heap dependency;
- logging API should permit compile-time/runtime level reduction later.

## Task 5 — Timebase

Provide a monotonic millisecond or microsecond-capable time API suitable for:

- debounce;
- state-machine timeouts;
- settling delays implemented non-blockingly;
- diagnostics timestamps.

This timebase is **not** the metrology sample clock.

## Task 6 — Watchdog

Add watchdog support with explicit policy:

- when it starts;
- which layer is responsible for servicing it;
- how boot reports watchdog resets;
- no long blocking operation may require disabling the watchdog.

Do not start the watchdog before initialization can reliably service it unless the reset behavior is intentionally designed and tested.

## Task 7 — Reset reason

Decode STM32 reset flags into a stable project enum/string representation, for example:

```text
POWER_ON
PIN_RESET
SOFTWARE
WATCHDOG
BROWNOUT/POWER_RELATED (where distinguishable)
UNKNOWN
```

Clear flags after capturing them.

## Task 8 — Application shell

Create the minimal cooperative application skeleton without implementing final measurement behavior.

Expected shape:

```c
for (;;)
{
    app_step();
    diagnostics_step();
    watchdog_service();
}
```

Establish the convention that future modules expose non-blocking `*_step()` functions where appropriate.

## Task 9 — BSP API discipline

Headers should expose stable project APIs rather than HAL internals where possible.

Examples:

```text
bsp_time_now_ms()
bsp_reset_get_reason()
bsp_uart_write()
bsp_watchdog_service()
```

Do not let every future module access global HAL handles directly unless there is a deliberate low-level driver boundary.

## Task 10 — Diagnostics build behavior

Define at least:

- `Debug`: useful logging/assertions;
- `Lab`: future high-detail diagnostics;
- `Release`: reduced log overhead.

The exact logger can remain simple, but level filtering must not require broad source edits later.

## Automated acceptance criteria

- `stm32-debug` build succeeds;
- host tests from Phase 01 still pass;
- no new C++/Arduino dependencies;
- BSP interfaces compile with strict project warnings;
- no higher layer directly manipulates output pins introduced in this phase.

## Bench acceptance criteria

With a Blue Pill/carrier available:

1. flash firmware through SWD or supported bootloader path;
2. verify UART banner;
3. verify reset-cause reporting for power cycle and software reset;
4. confirm SWD remains functional after JTAG remap;
5. measure K1/K2/RANGE_EN/buzzer/excitation command pins during boot and confirm safe levels;
6. confirm watchdog reset is detectable in a controlled test.

Until these checks pass, status should be `IMPLEMENTED_REQUIRES_BENCH_VALIDATION`.

## Handoff

Provide:

- final clock tree;
- BSP public APIs;
- UART parameters;
- watchdog policy;
- measured safe boot pin states if hardware was available;
- any pin-polarity discrepancy discovered against documentation;
- readiness for Phases 03 and 04.

## Phase 02 implementation status

Implemented on 2026-08-18 as a firmware/platform foundation. Bench validation remains required before this phase can be considered complete.

### Clock tree

```text
Expected source: 8 MHz HSE on Blue Pill
SYSCLK:          72 MHz via HSE PLL x9
AHB/HCLK:        72 MHz
APB1/PCLK1:      36 MHz
APB2/PCLK2:      72 MHz
APB1 timers:     72 MHz because APB1 prescaler is not 1
APB2 timers:     72 MHz
ADC clock:       12 MHz from PCLK2 / 6
SysTick:         1 kHz low-resolution timebase
Fallback:        HSI 8 MHz with safe GPIO retained and clock failure reported
```

### BSP public APIs

```text
bsp_clock_init()
bsp_clock_get_summary()
bsp_gpio_init_safe()
bsp_gpio_swd_preserved()
bsp_reset_capture_reason()
bsp_reset_get_reason()
bsp_time_init()
bsp_time_now_ms()
bsp_uart_init()
bsp_uart_write()
bsp_watchdog_start()
bsp_watchdog_service()
bsp_diagnostics_boot_banner()
bsp_diagnostics_step()
```

### UART and diagnostics

USART1 uses PA9/PA10 at 115200 baud, 8N1. The boot banner reports firmware version, Git identifier, build type, hardware compatibility, reset cause, clock status/source/frequencies, SWD-remap state, boot state, and watchdog policy.

Diagnostic default log levels:

```text
Debug:   DEBUG
Lab:     TRACE
Release: WARN
```

### Watchdog policy

The independent watchdog starts after safe GPIO initialization, clock/timebase setup, USART1 initialization, and UART boot banner emission. The cooperative shell services it every loop. Future long operations must be non-blocking or decomposed so watchdog service is not starved.

### Validation

Automated validation passed:

```bash
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug

cmake --preset host-release
cmake --build --preset host-release
ctest --preset host-release

cmake --preset stm32-debug
cmake --build --preset stm32-debug

cmake --preset stm32-release
cmake --build --preset stm32-release

cmake --preset stm32-lab
cmake --build --preset stm32-lab
```

Release memory report from the implemented firmware:

```text
FLASH: 4840 B / 64 KiB, 7.39%
RAM:   2112 B / 20 KiB, 10.31%
```

### REQUIRES_BENCH_VALIDATION

- UART banner on PA9/PA10.
- Power-cycle/software/watchdog reset cause reporting.
- SWD access after disabling JTAG.
- Boot pin levels for K1_CMD, K2_CMD, RANGE_EN, buzzer, PWM_EXC, TFT_CS, and FLASH_CS.
- Watchdog reset detection under controlled test.

No pin-polarity discrepancy was discovered from documentation during implementation. Physical polarity and safe levels still require bench confirmation.
