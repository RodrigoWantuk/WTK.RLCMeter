#ifndef WTK_MEASUREMENT_DC_H
#define WTK_MEASUREMENT_DC_H

#include <stdbool.h>

/* Pure DC math only. No caller may use this as a measurement safety permit. */
typedef enum
{
    MEASUREMENT_DC_INVALID = 0,
    MEASUREMENT_DC_CURRENT_UNRESOLVED,
    MEASUREMENT_DC_DUT_VOLTAGE_UNRESOLVED,
    MEASUREMENT_DC_VALID,
} measurement_dc_status_t;

typedef struct
{
    float vexc_v;
    float ret_v;
    float vmid_v;
    float rref_ohms;
    float minimum_source_v;
    float minimum_resolved_current_a;
    float minimum_resolved_dut_v;
    bool adc_valid;
} measurement_dc_input_t;

typedef struct
{
    measurement_dc_status_t status;
    float source_v;
    float dut_v;
    float current_a;
    float resistance_ohms;
} measurement_dc_result_t;

typedef struct
{
    bool ac_capture_valid;
    bool ac_teardown_safe;
    bool charger_absent;
    bool residual_safe;
    bool k1_safe;
    bool range_disabled;
    bool no_safety_fault;
    bool electrical_limits_approved;
    bool one_megohm_pilot_selected;
} measurement_dc_pilot_gate_t;

measurement_dc_result_t measurement_dc_estimate(const measurement_dc_input_t *input);
bool measurement_dc_pilot_gate_allows(const measurement_dc_pilot_gate_t *gate);

#endif
