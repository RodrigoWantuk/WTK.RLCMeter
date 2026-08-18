#include "bsp/bsp_time.h"

#include "stm32f1xx.h"

extern uint32_t SystemCoreClock;

static volatile uint32_t g_time_ms = 0u;

bsp_status_t bsp_time_init(void)
{
    g_time_ms = 0u;

    if (SysTick_Config(SystemCoreClock / 1000u) != 0u)
    {
        return BSP_STATUS_ERROR;
    }

    return BSP_STATUS_OK;
}

uint32_t bsp_time_now_ms(void)
{
    return g_time_ms;
}

void SysTick_Handler(void)
{
    g_time_ms++;
}
