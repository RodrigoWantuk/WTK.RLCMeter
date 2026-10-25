#include "app/app_cal_integration.h"

#include <stddef.h>
#include <math.h>

static measurement_complex_t scaled(measurement_complex_t value, float factor)
{
    return measurement_complex(value.re * factor, value.im * factor);
}

bsp_status_t app_cal_integration_push(app_cal_integration_t *integration,
    uint16_t required, const hw_metrology_block_t *block,
    const app_cal_workflow_request_t *request, app_cal_capture_sample_t *sample)
{
    if (integration == NULL || block == NULL || request == NULL || sample == NULL ||
        required < 2u || required > 96u || integration->count >= required)
        return BSP_STATUS_INVALID_ARG;
    const measurement_complex_t vmid = scaled(
        measurement_complex_add(sample->vmid_adc1_v, sample->vmid_adc2_v), 0.5f);
    const measurement_complex_t values[6] = {
        measurement_complex_add(sample->vexc_1_v, vmid),
        measurement_complex_add(sample->ret_1x_v, vmid),
        measurement_complex_add(sample->vexc_2_v, vmid),
        measurement_complex_add(sample->ret_hg_raw_v, vmid),
        sample->vmid_adc1_v, sample->vmid_adc2_v};
    const bool first = sample->vexc_1_peak_v > 1.0e-6f;
    const measurement_complex_t source = first ? sample->vexc_1_v : sample->vexc_2_v;
    const float magnitude = measurement_complex_mag(source);
    if (!block->valid || block->dma_error || block->timeout ||
        sample->reject_flags != APP_CAL_REJECT_NONE ||
        !isfinite(magnitude) ||
        !(magnitude >= (float)APP_CAL_WORKFLOW_SOURCE_MIN_UV_PEAK / 1000000.0f))
        return BSP_STATUS_ERROR;
    for (size_t i = 0u; i < 6u; i++)
        if (!measurement_complex_is_finite(values[i])) return BSP_STATUS_ERROR;
    const measurement_complex_t rotation = measurement_complex(source.re / magnitude,
                                                                 -source.im / magnitude);
    integration->count++;
    uint32_t current_clip_mask = 0u;
    for (size_t i = 0u; i < 6u; i++)
    {
        const measurement_complex_t aligned = measurement_complex_mul(values[i], rotation);
        integration->mean[i] = measurement_complex_add(integration->mean[i], scaled(
            measurement_complex_sub(aligned, integration->mean[i]), 1.0f/(float)integration->count));
        if (block->streams[i].hard_clipped) current_clip_mask |= 1u << i;
    }
    integration->clip_mask |= current_clip_mask;
    if (block->clipped && current_clip_mask == 0u)
        return BSP_STATUS_ERROR; /* Incomplete clipping provenance cannot be averaged away. */
    if (integration->count < required) return BSP_STATUS_BUSY;

    measurement_phasor_set_t phasors = {.vexc_1 = integration->mean[0],
        .ret_1x = integration->mean[1], .vexc_2 = integration->mean[2],
        .ret_hg = integration->mean[3], .vmid_adc1 = integration->mean[4],
        .vmid_adc2 = integration->mean[5], .valid = true};
    phasors.vmid = scaled(measurement_complex_add(phasors.vmid_adc1, phasors.vmid_adc2), 0.5f);
    const measurement_dsp_config_t config = measurement_dsp_config_ideal(request->key.range_id);
    measurement_complex_t delta;
    if (measurement_complex_div(measurement_complex_sub(phasors.ret_hg, phasors.vmid),
                                config.ret_hg_transfer, &delta) != MEASUREMENT_STATUS_OK)
        return BSP_STATUS_ERROR;
    phasors.ret_hg_reconstructed = measurement_complex_add(phasors.vmid, delta);
    hw_metrology_block_t metadata = *block;
    for (size_t i = 0u; i < 6u; i++)
        metadata.streams[i].hard_clipped = (integration->clip_mask & (1u << i)) != 0u;
    metadata.clipped = integration->clip_mask != 0u;
    return app_calibration_workflow_sample_from_phasors(&metadata, request, &phasors, sample);
}
