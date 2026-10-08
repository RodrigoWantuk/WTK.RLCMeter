#include "measurement/measurement_dc.h"

#include <math.h>
#include <stddef.h>

enum
{
    DC_PILOT_RREF_OHMS = 1000000u,
};

static float dc_adc_voltage(float mean_raw, measurement_adc_scale_t scale)
{
    return mean_raw * scale.code_to_volts + scale.offset_volts;
}

measurement_dc_result_t measurement_dc_analyze_pilot(const hw_metrology_block_t *block,
                                                     const measurement_adc_calibration_t *adc_cal)
{
    const measurement_dc_result_t invalid = {.status = MEASUREMENT_DC_INVALID};
    if ((block == NULL) || (adc_cal == NULL) || !block->valid ||
        (block->mode != HW_METROLOGY_MODE_DC_PILOT) ||
        (block->range_id != HW_RANGE_ID_1M) ||
        (block->sample_count != HW_METROLOGY_SAMPLES_PER_BLOCK) ||
        (block->raw_words == NULL))
    {
        return invalid;
    }

    uint32_t vexc_sum = 0u;
    uint32_t ret_sum = 0u;
    uint32_t vmid_sum = 0u;
    for (uint32_t i = 0u; i < HW_METROLOGY_SAMPLES_PER_BLOCK; i++)
    {
        hw_metrology_sample_t sample;
        if (hw_metrology_unpack_sample(block->raw_words, i, &sample) != BSP_STATUS_OK)
        {
            return invalid;
        }
        if (hw_metrology_raw_is_hard_clipped(sample.vexc_1) ||
            hw_metrology_raw_is_hard_clipped(sample.ret_1x) ||
            hw_metrology_raw_is_hard_clipped(sample.vexc_2) ||
            hw_metrology_raw_is_hard_clipped(sample.ret_hg) ||
            hw_metrology_raw_is_hard_clipped(sample.vmid_adc1) ||
            hw_metrology_raw_is_hard_clipped(sample.vmid_adc2))
        {
            return (measurement_dc_result_t){.status = MEASUREMENT_DC_CLIPPED};
        }
        vexc_sum += sample.vexc_1;
        ret_sum += sample.ret_1x;
        vmid_sum += sample.vmid_adc1;
    }
    const float samples = (float)HW_METROLOGY_SAMPLES_PER_BLOCK;
    const measurement_dc_input_t input = {
        .vexc_v = dc_adc_voltage((float)vexc_sum / samples, adc_cal->vexc_1),
        .ret_v = dc_adc_voltage((float)ret_sum / samples, adc_cal->ret_1x),
        .vmid_v = dc_adc_voltage((float)vmid_sum / samples, adc_cal->vmid_adc1),
        .rref_ohms = (float)DC_PILOT_RREF_OHMS,
        .minimum_source_v = 4.0f * 3.3f / 4095.0f,
        .minimum_resolved_current_a = (4.0f * 3.3f / 4095.0f) /
                                      (float)DC_PILOT_RREF_OHMS,
        .minimum_resolved_dut_v = 4.0f * 3.3f / 4095.0f,
        .adc_valid = true,
    };
    const float source = input.vexc_v - input.vmid_v;
    if (!isfinite(source) || (source <= 0.0f) || (source > 0.05f))
    {
        return (measurement_dc_result_t){.status = MEASUREMENT_DC_SOURCE_OUT_OF_RANGE};
    }
    return measurement_dc_estimate(&input);
}

static float magnitude(float value)
{
    return (value < 0.0f) ? -value : value;
}

measurement_dc_result_t measurement_dc_estimate(const measurement_dc_input_t *input)
{
    measurement_dc_result_t result = {0};
    result.status = MEASUREMENT_DC_INVALID;

    if ((input == NULL) || !input->adc_valid ||
        !isfinite(input->vexc_v) || !isfinite(input->ret_v) ||
        !isfinite(input->vmid_v) || !isfinite(input->rref_ohms) ||
        !isfinite(input->minimum_source_v) ||
        !isfinite(input->minimum_resolved_current_a) ||
        !isfinite(input->minimum_resolved_dut_v) ||
        (input->rref_ohms <= 0.0f) ||
        (input->minimum_source_v <= 0.0f) ||
        (input->minimum_resolved_current_a <= 0.0f) ||
        (input->minimum_resolved_dut_v <= 0.0f))
    {
        return result;
    }

    result.source_v = input->vexc_v - input->vmid_v;
    result.dut_v = input->ret_v - input->vmid_v;
    result.current_a = (input->vexc_v - input->ret_v) / input->rref_ohms;

    if (!isfinite(result.source_v) || !isfinite(result.dut_v) ||
        !isfinite(result.current_a) ||
        (magnitude(result.source_v) < input->minimum_source_v) ||
        (result.current_a * result.source_v < 0.0f) ||
        (result.dut_v * result.source_v < 0.0f))
    {
        return (measurement_dc_result_t){0};
    }

    if (magnitude(result.current_a) < input->minimum_resolved_current_a)
    {
        result.status = MEASUREMENT_DC_CURRENT_UNRESOLVED;
        return result;
    }

    if (magnitude(result.dut_v) < input->minimum_resolved_dut_v)
    {
        result.status = MEASUREMENT_DC_DUT_VOLTAGE_UNRESOLVED;
        return result;
    }

    result.resistance_ohms = result.dut_v / result.current_a;
    if (!isfinite(result.resistance_ohms) || (result.resistance_ohms < 0.0f))
    {
        return (measurement_dc_result_t){0};
    }

    result.status = MEASUREMENT_DC_VALID;
    return result;
}

bool measurement_dc_pilot_gate_allows(const measurement_dc_pilot_gate_t *gate)
{
    return (gate != NULL) && gate->ac_capture_valid && gate->ac_teardown_safe &&
           gate->charger_absent && gate->residual_safe && gate->k1_safe &&
           gate->range_disabled && gate->no_safety_fault &&
           gate->electrical_limits_approved && gate->one_megohm_pilot_selected;
}
