#include "bsp/bsp_adc.h"

#include <stddef.h>

bsp_status_t bsp_adc_channel_number(bsp_adc_channel_t channel, uint8_t *number)
{
    if (number == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    switch (channel)
    {
    case BSP_ADC_CHANNEL_VMID:
        *number = 1u;
        return BSP_STATUS_OK;
    case BSP_ADC_CHANNEL_OV_HI:
        *number = 4u;
        return BSP_STATUS_OK;
    case BSP_ADC_CHANNEL_OV_LO:
        *number = 5u;
        return BSP_STATUS_OK;
    case BSP_ADC_CHANNEL_BAT:
        *number = 6u;
        return BSP_STATUS_OK;
    case BSP_ADC_CHANNEL_NTC:
        *number = 7u;
        return BSP_STATUS_OK;
    case BSP_ADC_CHANNEL_INVALID:
    default:
        return BSP_STATUS_INVALID_ARG;
    }
}

float bsp_adc_raw_to_voltage(uint16_t raw)
{
    const uint16_t bounded = (raw > BSP_ADC_RAW_MAX) ? BSP_ADC_RAW_MAX : raw;
    return ((float)bounded * BSP_ADC_VDDA_NOMINAL_V) / (float)BSP_ADC_RAW_MAX;
}

const char *bsp_adc_state_string(bsp_adc_state_t state)
{
    switch (state)
    {
    case BSP_ADC_STATE_IDLE:
        return "IDLE";
    case BSP_ADC_STATE_BUSY:
        return "BUSY";
    default:
        return "UNKNOWN";
    }
}

const char *bsp_adc_channel_string(bsp_adc_channel_t channel)
{
    switch (channel)
    {
    case BSP_ADC_CHANNEL_VMID:
        return "VMID";
    case BSP_ADC_CHANNEL_OV_HI:
        return "OV_HI";
    case BSP_ADC_CHANNEL_OV_LO:
        return "OV_LO";
    case BSP_ADC_CHANNEL_BAT:
        return "BAT";
    case BSP_ADC_CHANNEL_NTC:
        return "NTC";
    case BSP_ADC_CHANNEL_INVALID:
    default:
        return "INVALID";
    }
}
