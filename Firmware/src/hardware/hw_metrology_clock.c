#include "hardware/hw_metrology_clock.h"

#include <stddef.h>

bool hw_metrology_clock_ready(const bsp_clock_summary_t *summary, bsp_status_t init_status)
{
    if ((init_status != BSP_STATUS_OK) || (summary == NULL))
    {
        return false;
    }

    return (summary->source == BSP_CLOCK_SOURCE_HSE_PLL) &&
           summary->hse_ready &&
           (summary->sysclk_hz == HW_METROLOGY_SYSCLK_HZ) &&
           (summary->hclk_hz == HW_METROLOGY_HCLK_HZ) &&
           (summary->pclk1_hz == HW_METROLOGY_PCLK1_HZ) &&
           (summary->pclk2_hz == HW_METROLOGY_PCLK2_HZ) &&
           (summary->tim_apb1_hz == HW_METROLOGY_TIM_APB1_HZ) &&
           (summary->tim_apb2_hz == HW_METROLOGY_TIM_APB2_HZ) &&
           (summary->adc_hz == HW_METROLOGY_ADC_HZ);
}
