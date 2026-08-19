#ifndef WTK_BSP_ADC_CORE_H
#define WTK_BSP_ADC_CORE_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_adc.h"

typedef struct
{
    bsp_adc_state_t state;
    bsp_adc_channel_t channel;
    uint32_t deadline_ms;
    bsp_status_t last_status;
} bsp_adc_core_t;

void bsp_adc_core_init(bsp_adc_core_t *core);
bsp_status_t bsp_adc_core_start(bsp_adc_core_t *core, bsp_adc_channel_t channel, uint32_t now_ms);
bsp_status_t bsp_adc_core_poll(bsp_adc_core_t *core,
                               bool conversion_complete,
                               uint16_t completed_raw,
                               uint16_t *raw,
                               uint32_t now_ms);
void bsp_adc_core_cancel(bsp_adc_core_t *core);

#endif
