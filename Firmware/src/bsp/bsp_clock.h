#ifndef WTK_BSP_CLOCK_H
#define WTK_BSP_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"

typedef enum
{
    BSP_CLOCK_SOURCE_HSI = 0,
    BSP_CLOCK_SOURCE_HSE_PLL,
} bsp_clock_source_t;

typedef struct
{
    bsp_clock_source_t source;
    bool hse_ready;
    uint32_t sysclk_hz;
    uint32_t hclk_hz;
    uint32_t pclk1_hz;
    uint32_t pclk2_hz;
    uint32_t tim_apb1_hz;
    uint32_t tim_apb2_hz;
    uint32_t adc_hz;
    uint32_t systick_hz;
} bsp_clock_summary_t;

bsp_status_t bsp_clock_init(void);
const bsp_clock_summary_t *bsp_clock_get_summary(void);
const char *bsp_clock_source_string(bsp_clock_source_t source);

#endif
