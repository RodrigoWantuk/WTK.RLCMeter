#ifndef WTK_BSP_METROLOGY_ADC_H
#define WTK_BSP_METROLOGY_ADC_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"

enum
{
    BSP_METROLOGY_RAW_WORD_COUNT = 768u,
};

bsp_status_t bsp_metrology_adc_acquire(uint32_t now_ms);
bsp_status_t bsp_metrology_adc_start_capture(uint32_t *raw_words,
                                             uint32_t word_count,
                                             uint16_t tim2_arr,
                                             uint16_t tim2_ccr2);
void bsp_metrology_adc_stop(void);
bsp_status_t bsp_metrology_adc_restore(uint32_t now_ms);
bool bsp_metrology_adc_dma_complete(void);
bool bsp_metrology_adc_dma_error(void);
void DMA1_Channel1_IRQHandler(void);

#endif
