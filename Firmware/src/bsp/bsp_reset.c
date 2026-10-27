#include "bsp/bsp_reset.h"

#include "stm32f1xx.h"
#include "wtk_build_config.h"

static bsp_reset_reason_t g_reset_reason = BSP_RESET_REASON_UNKNOWN;
static uint32_t g_reset_raw_flags = 0u;

bsp_reset_reason_t bsp_reset_capture_reason(void)
{
    g_reset_raw_flags = RCC->CSR;

    if ((g_reset_raw_flags & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF)) != 0u)
    {
        g_reset_reason = BSP_RESET_REASON_WATCHDOG;
    }
    else if ((g_reset_raw_flags & RCC_CSR_SFTRSTF) != 0u)
    {
        g_reset_reason = BSP_RESET_REASON_SOFTWARE;
    }
    else if ((g_reset_raw_flags & RCC_CSR_PORRSTF) != 0u)
    {
        g_reset_reason = BSP_RESET_REASON_POWER_ON;
    }
    else if ((g_reset_raw_flags & RCC_CSR_LPWRRSTF) != 0u)
    {
        g_reset_reason = BSP_RESET_REASON_BROWNOUT_OR_LOW_POWER;
    }
    else if ((g_reset_raw_flags & RCC_CSR_PINRSTF) != 0u)
    {
        g_reset_reason = BSP_RESET_REASON_PIN;
    }
    else
    {
        g_reset_reason = BSP_RESET_REASON_UNKNOWN;
    }

    RCC->CSR |= RCC_CSR_RMVF;
    return g_reset_reason;
}

bsp_reset_reason_t bsp_reset_get_reason(void)
{
    return g_reset_reason;
}

uint32_t bsp_reset_get_raw_flags(void)
{
    return g_reset_raw_flags;
}

void bsp_reset_read_device_uid(uint8_t uid[12])
{
    if (uid == 0) return;
    const volatile uint8_t *source = (const volatile uint8_t *)UID_BASE;
    for (uint32_t i = 0u; i < 12u; i++) uid[i] = source[i];
}

bool bsp_reset_memory_profile_valid(void)
{
#if WTK_INTERNAL_FLASH_BYTES > 65536u
    static const char expected[] = WTK_EXTENDED_FLASH_UID;
    static const char hex[] = "0123456789abcdef";
    uint8_t actual[12];
    bsp_reset_read_device_uid(actual);
    for (unsigned i = 0u; i < 12u; ++i)
        if (expected[2u*i] != hex[actual[i] >> 4u] || expected[2u*i+1u] != hex[actual[i] & 15u]) return false;
#endif
    return true;
}

const char *bsp_reset_reason_string(bsp_reset_reason_t reason)
{
    switch (reason)
    {
    case BSP_RESET_REASON_POWER_ON:
        return "POWER_ON";
    case BSP_RESET_REASON_PIN:
        return "PIN_RESET";
    case BSP_RESET_REASON_SOFTWARE:
        return "SOFTWARE";
    case BSP_RESET_REASON_WATCHDOG:
        return "WATCHDOG";
    case BSP_RESET_REASON_BROWNOUT_OR_LOW_POWER:
        return "BROWNOUT_OR_LOW_POWER";
    case BSP_RESET_REASON_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}
