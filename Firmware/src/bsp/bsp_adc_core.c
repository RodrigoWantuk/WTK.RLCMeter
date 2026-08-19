#include "bsp/bsp_adc_core.h"

#include <stddef.h>

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (now_ms - deadline_ms) < 0x80000000u;
}

void bsp_adc_core_init(bsp_adc_core_t *core)
{
    if (core == NULL)
    {
        return;
    }

    core->state = BSP_ADC_STATE_IDLE;
    core->channel = BSP_ADC_CHANNEL_INVALID;
    core->deadline_ms = 0u;
    core->last_status = BSP_STATUS_OK;
}

bsp_status_t bsp_adc_core_start(bsp_adc_core_t *core, bsp_adc_channel_t channel, uint32_t now_ms)
{
    uint8_t channel_number = 0u;
    if ((core == NULL) || (bsp_adc_channel_number(channel, &channel_number) != BSP_STATUS_OK))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (core->state != BSP_ADC_STATE_IDLE)
    {
        core->last_status = BSP_STATUS_BUSY;
        return BSP_STATUS_BUSY;
    }

    core->state = BSP_ADC_STATE_BUSY;
    core->channel = channel;
    core->deadline_ms = now_ms + BSP_ADC_CONVERSION_TIMEOUT_MS;
    core->last_status = BSP_STATUS_BUSY;
    return BSP_STATUS_OK;
}

bsp_status_t bsp_adc_core_poll(bsp_adc_core_t *core,
                               bool conversion_complete,
                               uint16_t completed_raw,
                               uint16_t *raw,
                               uint32_t now_ms)
{
    if ((core == NULL) || (raw == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (core->state != BSP_ADC_STATE_BUSY)
    {
        core->last_status = BSP_STATUS_INVALID_ARG;
        return BSP_STATUS_INVALID_ARG;
    }
    if (conversion_complete)
    {
        *raw = (completed_raw > BSP_ADC_RAW_MAX) ? BSP_ADC_RAW_MAX : completed_raw;
        core->state = BSP_ADC_STATE_IDLE;
        core->channel = BSP_ADC_CHANNEL_INVALID;
        core->last_status = BSP_STATUS_OK;
        return BSP_STATUS_OK;
    }
    if (deadline_reached(now_ms, core->deadline_ms))
    {
        core->state = BSP_ADC_STATE_IDLE;
        core->channel = BSP_ADC_CHANNEL_INVALID;
        core->last_status = BSP_STATUS_TIMEOUT;
        return BSP_STATUS_TIMEOUT;
    }

    core->last_status = BSP_STATUS_BUSY;
    return BSP_STATUS_BUSY;
}

void bsp_adc_core_cancel(bsp_adc_core_t *core)
{
    if (core == NULL)
    {
        return;
    }

    core->state = BSP_ADC_STATE_IDLE;
    core->channel = BSP_ADC_CHANNEL_INVALID;
    core->last_status = BSP_STATUS_OK;
}
