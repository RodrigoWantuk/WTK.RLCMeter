#include "bsp/bsp_watchdog.h"

#include "stm32f1xx.h"

enum
{
    WTK_IWDG_KEY_ENABLE = 0x0000CCCCu,
    WTK_IWDG_KEY_RELOAD = 0x0000AAAAu,
    WTK_IWDG_KEY_WRITE_ACCESS = 0x00005555u,
    WTK_IWDG_PRESCALER_64 = 0x00000004u,
    WTK_IWDG_RELOAD_FOR_2S = 1250u,
    WTK_IWDG_TIMEOUT_POLLS = 100000u,
};

static bool g_watchdog_started = false;

static bool wait_iwdg_not_busy(void)
{
    uint32_t timeout = WTK_IWDG_TIMEOUT_POLLS;

    while (((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0u) && (timeout > 0u))
    {
        timeout--;
    }

    return ((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) == 0u);
}

bsp_status_t bsp_watchdog_start(void)
{
    RCC->CSR |= RCC_CSR_LSION;

    uint32_t timeout = WTK_IWDG_TIMEOUT_POLLS;
    while (((RCC->CSR & RCC_CSR_LSIRDY) == 0u) && (timeout > 0u))
    {
        timeout--;
    }

    if (timeout == 0u)
    {
        return BSP_STATUS_TIMEOUT;
    }

    IWDG->KR = WTK_IWDG_KEY_WRITE_ACCESS;
    IWDG->PR = WTK_IWDG_PRESCALER_64;
    IWDG->RLR = WTK_IWDG_RELOAD_FOR_2S;

    if (!wait_iwdg_not_busy())
    {
        return BSP_STATUS_TIMEOUT;
    }

    IWDG->KR = WTK_IWDG_KEY_RELOAD;
    IWDG->KR = WTK_IWDG_KEY_ENABLE;
    g_watchdog_started = true;

    return BSP_STATUS_OK;
}

void bsp_watchdog_service(void)
{
    if (g_watchdog_started)
    {
        IWDG->KR = WTK_IWDG_KEY_RELOAD;
    }
}

bool bsp_watchdog_is_started(void)
{
    return g_watchdog_started;
}
