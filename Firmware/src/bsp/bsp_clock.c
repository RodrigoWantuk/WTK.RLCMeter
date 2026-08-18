#include "bsp/bsp_clock.h"

#include "stm32f1xx.h"

enum
{
    WTK_HSI_HZ = 8000000u,
    WTK_HSE_HZ = 8000000u,
    WTK_SYSCLK_HZ = 72000000u,
    WTK_PCLK1_HZ = 36000000u,
    WTK_PCLK2_HZ = 72000000u,
    WTK_ADC_HZ = 12000000u,
    WTK_SYSTICK_HZ = 1000u,
    WTK_CLOCK_READY_TIMEOUT = 1000000u,
};

extern uint32_t SystemCoreClock;

static bsp_clock_summary_t g_clock_summary = {
    .source = BSP_CLOCK_SOURCE_HSI,
    .hse_ready = false,
    .sysclk_hz = WTK_HSI_HZ,
    .hclk_hz = WTK_HSI_HZ,
    .pclk1_hz = WTK_HSI_HZ,
    .pclk2_hz = WTK_HSI_HZ,
    .tim_apb1_hz = WTK_HSI_HZ,
    .tim_apb2_hz = WTK_HSI_HZ,
    .adc_hz = WTK_HSI_HZ / 2u,
    .systick_hz = WTK_SYSTICK_HZ,
};

static bool wait_until_set(volatile uint32_t *const reg, uint32_t mask)
{
    uint32_t timeout = WTK_CLOCK_READY_TIMEOUT;

    while (((*reg & mask) == 0u) && (timeout > 0u))
    {
        timeout--;
    }

    return ((*reg & mask) != 0u);
}

static void record_hsi_summary(void)
{
    g_clock_summary.source = BSP_CLOCK_SOURCE_HSI;
    g_clock_summary.hse_ready = false;
    g_clock_summary.sysclk_hz = WTK_HSI_HZ;
    g_clock_summary.hclk_hz = WTK_HSI_HZ;
    g_clock_summary.pclk1_hz = WTK_HSI_HZ;
    g_clock_summary.pclk2_hz = WTK_HSI_HZ;
    g_clock_summary.tim_apb1_hz = WTK_HSI_HZ;
    g_clock_summary.tim_apb2_hz = WTK_HSI_HZ;
    g_clock_summary.adc_hz = WTK_HSI_HZ / 2u;
    g_clock_summary.systick_hz = WTK_SYSTICK_HZ;
    SystemCoreClock = WTK_HSI_HZ;
}

bsp_status_t bsp_clock_init(void)
{
    RCC->CR |= RCC_CR_HSION;
    if (!wait_until_set(&RCC->CR, RCC_CR_HSIRDY))
    {
        record_hsi_summary();
        return BSP_STATUS_TIMEOUT;
    }

    RCC->CR |= RCC_CR_HSEON;
    if (!wait_until_set(&RCC->CR, RCC_CR_HSERDY))
    {
        record_hsi_summary();
        return BSP_STATUS_TIMEOUT;
    }

    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 |
                   RCC_CFGR_ADCPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE |
                   RCC_CFGR_PLLMULL);
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 |
                 RCC_CFGR_PPRE1_DIV2 |
                 RCC_CFGR_PPRE2_DIV1 |
                 RCC_CFGR_ADCPRE_DIV6 |
                 RCC_CFGR_PLLSRC |
                 RCC_CFGR_PLLMULL9;

    RCC->CR |= RCC_CR_PLLON;
    if (!wait_until_set(&RCC->CR, RCC_CR_PLLRDY))
    {
        record_hsi_summary();
        return BSP_STATUS_TIMEOUT;
    }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    if (!wait_until_set(&RCC->CFGR, RCC_CFGR_SWS_PLL))
    {
        record_hsi_summary();
        return BSP_STATUS_TIMEOUT;
    }

    g_clock_summary.source = BSP_CLOCK_SOURCE_HSE_PLL;
    g_clock_summary.hse_ready = true;
    g_clock_summary.sysclk_hz = WTK_SYSCLK_HZ;
    g_clock_summary.hclk_hz = WTK_SYSCLK_HZ;
    g_clock_summary.pclk1_hz = WTK_PCLK1_HZ;
    g_clock_summary.pclk2_hz = WTK_PCLK2_HZ;
    g_clock_summary.tim_apb1_hz = WTK_PCLK1_HZ * 2u;
    g_clock_summary.tim_apb2_hz = WTK_PCLK2_HZ;
    g_clock_summary.adc_hz = WTK_ADC_HZ;
    g_clock_summary.systick_hz = WTK_SYSTICK_HZ;
    SystemCoreClock = WTK_SYSCLK_HZ;

    return BSP_STATUS_OK;
}

const bsp_clock_summary_t *bsp_clock_get_summary(void)
{
    return &g_clock_summary;
}

const char *bsp_clock_source_string(bsp_clock_source_t source)
{
    switch (source)
    {
    case BSP_CLOCK_SOURCE_HSI:
        return "HSI";
    case BSP_CLOCK_SOURCE_HSE_PLL:
        return "HSE_PLL";
    default:
        return "UNKNOWN";
    }
}
