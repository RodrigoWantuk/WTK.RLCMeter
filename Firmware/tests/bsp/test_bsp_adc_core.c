#include "bsp/bsp_adc_core.h"

#include <stdbool.h>
#include <stdint.h>
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

int main(void)
{
    int failures = 0;
    bsp_adc_core_t core;
    uint8_t channel_number = 0u;
    uint16_t raw = 0u;

    bsp_adc_core_init(&core);
    failures += expect_true(core.state == BSP_ADC_STATE_IDLE, "ADC core starts idle");
    failures += expect_true(BSP_ADC_POWER_STABILIZATION_US == 2u, "ADC stabilization delay is fixed at 2 us");
    failures += expect_true(bsp_adc_channel_number(BSP_ADC_CHANNEL_VMID, &channel_number) == BSP_STATUS_OK &&
                                channel_number == 1u,
                            "VMID channel maps to ADC1 channel 1");
    failures += expect_true(bsp_adc_channel_number(BSP_ADC_CHANNEL_OV_HI, &channel_number) == BSP_STATUS_OK &&
                                channel_number == 4u,
                            "OV_HI channel maps to ADC1 channel 4");
    failures += expect_true(bsp_adc_channel_number(BSP_ADC_CHANNEL_OV_LO, &channel_number) == BSP_STATUS_OK &&
                                channel_number == 5u,
                            "OV_LO channel maps to ADC1 channel 5");
    failures += expect_true(bsp_adc_channel_number(BSP_ADC_CHANNEL_BAT, &channel_number) == BSP_STATUS_OK &&
                                channel_number == 6u,
                            "BAT channel maps to ADC1 channel 6");
    failures += expect_true(bsp_adc_channel_number(BSP_ADC_CHANNEL_NTC, &channel_number) == BSP_STATUS_OK &&
                                channel_number == 7u,
                            "NTC channel maps to ADC1 channel 7");
    failures += expect_true(bsp_adc_channel_number(BSP_ADC_CHANNEL_INVALID, &channel_number) ==
                                BSP_STATUS_INVALID_ARG,
                            "invalid ADC channel rejected");

    failures += expect_true(bsp_adc_core_start(&core, BSP_ADC_CHANNEL_VMID, 10u) == BSP_STATUS_OK,
                            "start from idle succeeds");
    failures += expect_true(core.state == BSP_ADC_STATE_BUSY, "start enters busy");
    failures += expect_true(bsp_adc_core_start(&core, BSP_ADC_CHANNEL_BAT, 10u) == BSP_STATUS_BUSY,
                            "start while busy rejected");
    failures += expect_true(bsp_adc_core_poll(&core, false, 0u, &raw, 11u) == BSP_STATUS_BUSY,
                            "poll before EOC is busy");
    failures += expect_true(bsp_adc_core_poll(&core, true, 2048u, &raw, 11u) == BSP_STATUS_OK,
                            "poll complete succeeds");
    failures += expect_true(raw == 2048u, "completed raw is returned");
    failures += expect_true(core.state == BSP_ADC_STATE_IDLE, "completion returns idle");

    failures += expect_true(bsp_adc_core_start(&core, BSP_ADC_CHANNEL_OV_HI, 20u) == BSP_STATUS_OK,
                            "second start succeeds");
    failures += expect_true(bsp_adc_core_poll(&core, false, 0u, &raw, 22u) == BSP_STATUS_TIMEOUT,
                            "conversion timeout returns timeout");
    failures += expect_true(core.state == BSP_ADC_STATE_IDLE, "timeout returns idle");

    failures += expect_true(bsp_adc_core_start(&core, BSP_ADC_CHANNEL_NTC, 30u) == BSP_STATUS_OK,
                            "start before cancel succeeds");
    bsp_adc_core_cancel(&core);
    failures += expect_true(core.state == BSP_ADC_STATE_IDLE, "cancel returns idle");
    failures += expect_true(core.channel == BSP_ADC_CHANNEL_INVALID, "cancel clears current channel");
    failures += expect_true(core.last_status == BSP_STATUS_OK, "cancel clears stale status");
    failures += expect_true(bsp_adc_core_start(&core, BSP_ADC_CHANNEL_VMID, 40u) == BSP_STATUS_OK,
                            "start after cancel is accepted from IDLE");
    failures += expect_true(bsp_adc_raw_to_voltage(4095u) > 3.299f, "raw full-scale uses denominator 4095");

    return (failures == 0) ? 0 : 1;
}
