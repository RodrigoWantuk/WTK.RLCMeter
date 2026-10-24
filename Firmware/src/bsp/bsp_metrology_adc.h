#ifndef WTK_BSP_METROLOGY_ADC_H
#define WTK_BSP_METROLOGY_ADC_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "wtk_build_config.h"

#if WTK_ENABLE_BENCH_SUPPLY
/* Captured before stop/restore destroys timeout evidence. No ADC DR reads. */
typedef struct
{
    bool valid;
    uint32_t captured_ms;
    uint32_t adc1_sr;
    uint32_t adc1_cr1;
    uint32_t adc1_cr2;
    uint32_t adc2_sr;
    uint32_t adc2_cr2;
    uint32_t dma_isr;
    uint32_t dma_ccr;
    uint32_t dma_remaining;
    uint32_t tim2_cr1;
    uint32_t tim2_sr;
    uint32_t tim2_cnt;
    uint32_t tim2_arr;
    uint32_t tim2_ccr2;
    uint32_t primask;
    bool dma_complete;
    bool dma_error;
} bsp_metrology_adc_trace_t;
void bsp_metrology_adc_trace(bsp_metrology_adc_trace_t *out);
#endif

enum
{
    BSP_METROLOGY_RAW_WORD_COUNT = 768u,
};

bsp_status_t bsp_metrology_adc_acquire(uint32_t now_ms);
bsp_status_t bsp_metrology_adc_start_capture(uint32_t *raw_words,
                                             uint32_t word_count,
                                             uint16_t tim2_psc,
                                             uint16_t tim2_arr,
                                             uint16_t tim2_ccr2);
void bsp_metrology_adc_stop(void);
bsp_status_t bsp_metrology_adc_restore(uint32_t now_ms);
bool bsp_metrology_adc_dma_complete(void);
bool bsp_metrology_adc_dma_error(void);
void DMA1_Channel1_IRQHandler(void);

#endif
