#ifndef WTK_HW_METROLOGY_CLOCK_H
#define WTK_HW_METROLOGY_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_clock.h"
#include "bsp/bsp_status.h"

enum
{
    HW_METROLOGY_SYSCLK_HZ = 72000000u,
    HW_METROLOGY_HCLK_HZ = 72000000u,
    HW_METROLOGY_PCLK1_HZ = 36000000u,
    HW_METROLOGY_PCLK2_HZ = 72000000u,
    HW_METROLOGY_TIM_APB1_HZ = 72000000u,
    HW_METROLOGY_TIM_APB2_HZ = 72000000u,
    HW_METROLOGY_ADC_HZ = 12000000u,
};

/*
 * Metrology may run only on the frozen HSE/PLL production clock.
 * Diagnostics/UI may continue on HSI fallback; capture must not.
 */
bool hw_metrology_clock_ready(const bsp_clock_summary_t *summary, bsp_status_t init_status);

#endif
