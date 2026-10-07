#include "measurement/measurement_dc.h"

#include <math.h>
#include <stddef.h>

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
