#include "hardware/hw_metrology_clock.h"

#include <stdio.h>

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static bsp_clock_summary_t production_clock(void)
{
    bsp_clock_summary_t summary = {
        .source = BSP_CLOCK_SOURCE_HSE_PLL,
        .hse_ready = true,
        .sysclk_hz = 72000000u,
        .hclk_hz = 72000000u,
        .pclk1_hz = 36000000u,
        .pclk2_hz = 72000000u,
        .tim_apb1_hz = 72000000u,
        .tim_apb2_hz = 72000000u,
        .adc_hz = 12000000u,
        .systick_hz = 1000u,
    };
    return summary;
}

int main(void)
{
    int failures = 0;
    bsp_clock_summary_t summary = production_clock();
    failures += expect_true(hw_metrology_clock_ready(&summary, BSP_STATUS_OK), "production clock ready");

    summary = production_clock();
    summary.source = BSP_CLOCK_SOURCE_HSI;
    failures += expect_true(!hw_metrology_clock_ready(&summary, BSP_STATUS_OK), "HSI source");

    summary = production_clock();
    summary.hse_ready = false;
    failures += expect_true(!hw_metrology_clock_ready(&summary, BSP_STATUS_OK), "hse_ready false");

    summary = production_clock();
    summary.sysclk_hz = 8000000u;
    failures += expect_true(!hw_metrology_clock_ready(&summary, BSP_STATUS_OK), "sysclk != 72M");

    summary = production_clock();
    summary.pclk1_hz = 72000000u;
    failures += expect_true(!hw_metrology_clock_ready(&summary, BSP_STATUS_OK), "pclk1 != 36M");

    summary = production_clock();
    summary.pclk2_hz = 36000000u;
    failures += expect_true(!hw_metrology_clock_ready(&summary, BSP_STATUS_OK), "pclk2 != 72M");

    summary = production_clock();
    summary.tim_apb1_hz = 36000000u;
    failures += expect_true(!hw_metrology_clock_ready(&summary, BSP_STATUS_OK), "tim_apb1 != 72M");

    summary = production_clock();
    summary.tim_apb2_hz = 36000000u;
    failures += expect_true(!hw_metrology_clock_ready(&summary, BSP_STATUS_OK), "tim_apb2 != 72M");

    summary = production_clock();
    summary.adc_hz = 8000000u;
    failures += expect_true(!hw_metrology_clock_ready(&summary, BSP_STATUS_OK), "adc != 12M");

    summary = production_clock();
    failures += expect_true(!hw_metrology_clock_ready(&summary, BSP_STATUS_TIMEOUT), "init timeout");
    failures += expect_true(!hw_metrology_clock_ready(NULL, BSP_STATUS_OK), "NULL summary");

    return (failures == 0) ? 0 : 1;
}
