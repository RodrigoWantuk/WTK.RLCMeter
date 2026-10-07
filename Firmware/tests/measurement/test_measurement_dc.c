#include "measurement/measurement_dc.h"

#include <assert.h>
#include <stddef.h>

static float absf_local(float value)
{
    return (value < 0.0f) ? -value : value;
}

int main(void)
{
    const measurement_dc_input_t nominal = {
        .vexc_v = 1.75f,
        .ret_v = 1.6590909f,
        .vmid_v = 1.65f,
        .rref_ohms = 1000000.0f,
        .minimum_source_v = 0.01f,
        .minimum_resolved_current_a = 1.0e-9f,
        .minimum_resolved_dut_v = 0.001f,
        .adc_valid = true,
    };
    measurement_dc_result_t result = measurement_dc_estimate(&nominal);
    assert(result.status == MEASUREMENT_DC_VALID);
    assert(absf_local(result.resistance_ohms - 100000.0f) < 10.0f);

    measurement_dc_input_t input = nominal;
    input.vexc_v = 1.55f;
    input.ret_v = 1.6409091f;
    result = measurement_dc_estimate(&input);
    assert(result.status == MEASUREMENT_DC_VALID);
    assert(absf_local(result.resistance_ohms - 100000.0f) < 10.0f);

    input = nominal;
    input.ret_v = 1.6500999f;
    result = measurement_dc_estimate(&input);
    assert(result.status == MEASUREMENT_DC_DUT_VOLTAGE_UNRESOLVED);

    input = nominal;
    input.ret_v = input.vexc_v;
    result = measurement_dc_estimate(&input);
    assert(result.status == MEASUREMENT_DC_CURRENT_UNRESOLVED);

    input = nominal;
    input.ret_v = input.vmid_v;
    result = measurement_dc_estimate(&input);
    assert(result.status == MEASUREMENT_DC_DUT_VOLTAGE_UNRESOLVED);

    input = nominal;
    input.adc_valid = false;
    assert(measurement_dc_estimate(&input).status == MEASUREMENT_DC_INVALID);

    input = nominal;
    input.ret_v = 1.80f;
    result = measurement_dc_estimate(&input);
    assert(result.status == MEASUREMENT_DC_INVALID);
    assert(result.resistance_ohms == 0.0f);
    assert(result.current_a == 0.0f);

    input = nominal;
    input.rref_ohms = 0.0f;
    assert(measurement_dc_estimate(&input).status == MEASUREMENT_DC_INVALID);
    assert(measurement_dc_estimate(NULL).status == MEASUREMENT_DC_INVALID);

    measurement_dc_pilot_gate_t gate = {
        .ac_capture_valid = true,
        .ac_teardown_safe = true,
        .charger_absent = true,
        .residual_safe = true,
        .k1_safe = true,
        .range_disabled = true,
        .no_safety_fault = true,
        .electrical_limits_approved = false,
        .one_megohm_pilot_selected = true,
    };
    assert(!measurement_dc_pilot_gate_allows(&gate));
    gate.electrical_limits_approved = true;
    assert(measurement_dc_pilot_gate_allows(&gate));
    gate.one_megohm_pilot_selected = false;
    assert(!measurement_dc_pilot_gate_allows(&gate));
    gate.one_megohm_pilot_selected = true;
    gate.ac_capture_valid = false;
    assert(!measurement_dc_pilot_gate_allows(&gate));
    assert(!measurement_dc_pilot_gate_allows(NULL));
    return 0;
}
