#ifndef WTK_BSP_ADC_H
#define WTK_BSP_ADC_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"

enum
{
    BSP_ADC_RAW_MAX = 4095u,
    BSP_ADC_CALIBRATION_TIMEOUT_MS = 10u,
    BSP_ADC_CONVERSION_TIMEOUT_MS = 2u,
    BSP_ADC_SAMPLE_TIME_CYCLES_X10 = 2395u,
    BSP_ADC_POWER_STABILIZATION_US = 2u,
};

#define BSP_ADC_VDDA_NOMINAL_V (3.300f)

typedef enum
{
    BSP_ADC_CHANNEL_VMID = 1,
    BSP_ADC_CHANNEL_OV_HI = 4,
    BSP_ADC_CHANNEL_OV_LO = 5,
    BSP_ADC_CHANNEL_BAT = 6,
    BSP_ADC_CHANNEL_NTC = 7,
    BSP_ADC_CHANNEL_INVALID = 255,
} bsp_adc_channel_t;

typedef enum
{
    BSP_ADC_STATE_IDLE = 0,
    BSP_ADC_STATE_BUSY,
} bsp_adc_state_t;

bsp_status_t bsp_adc_init(uint32_t now_ms);
bsp_status_t bsp_adc_start(bsp_adc_channel_t channel, uint32_t now_ms);
bsp_status_t bsp_adc_poll(uint16_t *raw, uint32_t now_ms);
void bsp_adc_cancel(void);
bool bsp_adc_is_busy(void);
bsp_adc_channel_t bsp_adc_current_channel(void);
bsp_status_t bsp_adc_last_status(void);
bsp_status_t bsp_adc_channel_number(bsp_adc_channel_t channel, uint8_t *number);
float bsp_adc_raw_to_voltage(uint16_t raw);
const char *bsp_adc_state_string(bsp_adc_state_t state);
const char *bsp_adc_channel_string(bsp_adc_channel_t channel);

#endif
