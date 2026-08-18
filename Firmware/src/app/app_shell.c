#include "app/app_shell.h"

#include "bsp/bsp_clock.h"
#include "bsp/bsp_diagnostics.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_reset.h"
#include "bsp/bsp_status.h"
#include "bsp/bsp_time.h"
#include "bsp/bsp_uart.h"
#include "bsp/bsp_watchdog.h"

static void app_step(void)
{
}

void app_shell_run(void)
{
    const bsp_reset_reason_t reset_reason = bsp_reset_capture_reason();

    (void)bsp_gpio_init_safe();
    const bsp_status_t clock_status = bsp_clock_init();
    (void)bsp_time_init();
    (void)bsp_uart_init(115200u);

    bsp_diagnostics_boot_banner(reset_reason, clock_status);
    (void)bsp_watchdog_start();

    for (;;)
    {
        app_step();
        bsp_diagnostics_step();
        bsp_watchdog_service();
    }
}
