#ifndef WTK_HW_MEASURE_PERMIT_H
#define WTK_HW_MEASURE_PERMIT_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/hw_k1.h"
#include "hardware/hw_range.h"
#include "hardware/hw_safety.h"

enum
{
    HW_MEASURE_PERMIT_TTL_MS = 5u,
    HW_MEASURE_PERMIT_RESIDUAL_MAX_AGE_MS = 20u,
    HW_MEASURE_PERMIT_BATTERY_MAX_AGE_MS = 1000u,
};

typedef enum
{
    HW_MEASURE_PERMIT_OK = 0,
    HW_MEASURE_PERMIT_REJECT_CHARGER,
    HW_MEASURE_PERMIT_REJECT_RESIDUAL,
    HW_MEASURE_PERMIT_REJECT_RESIDUAL_STALE,
    HW_MEASURE_PERMIT_REJECT_BATTERY,
    HW_MEASURE_PERMIT_REJECT_BATTERY_STALE,
    HW_MEASURE_PERMIT_REJECT_RANGE,
    HW_MEASURE_PERMIT_REJECT_RANGE_ID,
    HW_MEASURE_PERMIT_REJECT_K1,
    HW_MEASURE_PERMIT_REJECT_FAULT,
    HW_MEASURE_PERMIT_REJECT_SAFETY,
    HW_MEASURE_PERMIT_REJECT_EXPIRED,
    HW_MEASURE_PERMIT_REJECT_CONSUMED,
    HW_MEASURE_PERMIT_REJECT_INVALID,
} hw_measure_permit_rejection_t;

typedef struct
{
    hw_charger_state_t charger;
    hw_residual_state_t residual;
    uint32_t residual_age_ms;
    hw_battery_state_t battery;
    uint32_t battery_age_ms;
    hw_safety_range_state_t range;
    hw_range_id_t range_id;
    hw_k1_state_t k1_state;
    uint32_t safety_fault_mask;
} hw_measure_permit_issue_input_t;

typedef struct
{
    hw_charger_state_t charger;
    hw_safety_range_state_t range;
    hw_range_id_t range_id;
    hw_k1_state_t k1_state;
    uint32_t safety_fault_mask;
} hw_measure_permit_validate_input_t;

typedef struct
{
    bool issued;
    hw_measure_permit_rejection_t reason;
} hw_measure_permit_issue_result_t;

typedef struct
{
    bool allowed;
    hw_measure_permit_rejection_t reason;
} hw_measure_permit_validate_result_t;

typedef struct
{
    bool valid;
    bool consumed;
    uint32_t issued_at_ms;
    hw_range_id_t range_id;
} hw_measure_permit_t;

void hw_measure_permit_init(hw_measure_permit_t *permit);
hw_measure_permit_issue_result_t hw_measure_permit_check_issue(const hw_measure_permit_issue_input_t *input);
hw_measure_permit_issue_result_t hw_measure_permit_issue(hw_measure_permit_t *permit,
                                                         const hw_measure_permit_issue_input_t *input,
                                                         uint32_t now_ms);
hw_measure_permit_validate_result_t hw_measure_permit_validate(hw_measure_permit_t *permit,
                                                               const hw_measure_permit_validate_input_t *input,
                                                               uint32_t now_ms);
const char *hw_measure_permit_rejection_string(hw_measure_permit_rejection_t reason);

#endif
