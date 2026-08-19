#include "hardware/hw_measure_permit.h"

#include <stddef.h>

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (now_ms - deadline_ms) < 0x80000000u;
}

static bool range_id_valid(hw_range_id_t range_id)
{
    uint8_t address = 0u;
    return hw_range_id_to_address(range_id, &address) == BSP_STATUS_OK;
}

void hw_measure_permit_init(hw_measure_permit_t *permit)
{
    if (permit == NULL)
    {
        return;
    }

    permit->valid = false;
    permit->consumed = false;
    permit->issued_at_ms = 0u;
    permit->range_id = HW_RANGE_ID_INVALID;
}

hw_measure_permit_issue_result_t hw_measure_permit_check_issue(const hw_measure_permit_issue_input_t *input)
{
    if (input == NULL)
    {
        return (hw_measure_permit_issue_result_t){.issued = false, .reason = HW_MEASURE_PERMIT_REJECT_INVALID};
    }
    if (input->safety_fault_mask != 0u)
    {
        return (hw_measure_permit_issue_result_t){.issued = false, .reason = HW_MEASURE_PERMIT_REJECT_FAULT};
    }
    if (input->charger != HW_CHARGER_ABSENT)
    {
        return (hw_measure_permit_issue_result_t){.issued = false, .reason = HW_MEASURE_PERMIT_REJECT_CHARGER};
    }
    if (input->residual != HW_RESIDUAL_SAFE)
    {
        return (hw_measure_permit_issue_result_t){.issued = false, .reason = HW_MEASURE_PERMIT_REJECT_RESIDUAL};
    }
    if (input->residual_age_ms > HW_MEASURE_PERMIT_RESIDUAL_MAX_AGE_MS)
    {
        return (hw_measure_permit_issue_result_t){.issued = false,
                                                  .reason = HW_MEASURE_PERMIT_REJECT_RESIDUAL_STALE};
    }
    if ((input->battery != HW_BATTERY_OK) && (input->battery != HW_BATTERY_LOW))
    {
        return (hw_measure_permit_issue_result_t){.issued = false, .reason = HW_MEASURE_PERMIT_REJECT_BATTERY};
    }
    if (input->battery_age_ms > HW_MEASURE_PERMIT_BATTERY_MAX_AGE_MS)
    {
        return (hw_measure_permit_issue_result_t){.issued = false,
                                                  .reason = HW_MEASURE_PERMIT_REJECT_BATTERY_STALE};
    }
    if (input->range != HW_RANGE_READY)
    {
        return (hw_measure_permit_issue_result_t){.issued = false, .reason = HW_MEASURE_PERMIT_REJECT_RANGE};
    }
    if (!range_id_valid(input->range_id))
    {
        return (hw_measure_permit_issue_result_t){.issued = false, .reason = HW_MEASURE_PERMIT_REJECT_RANGE_ID};
    }
    if (input->k1_state != HW_K1_STATE_SAFE)
    {
        return (hw_measure_permit_issue_result_t){.issued = false, .reason = HW_MEASURE_PERMIT_REJECT_K1};
    }

    const hw_safety_input_t safety_input = {
        .charger = input->charger,
        .residual = input->residual,
        .battery = input->battery,
        .range = input->range,
        .application_fault = false,
    };
    const hw_safety_result_t safety = hw_safety_evaluate(&safety_input);
    if (!safety.measure_allowed)
    {
        return (hw_measure_permit_issue_result_t){.issued = false, .reason = HW_MEASURE_PERMIT_REJECT_SAFETY};
    }

    return (hw_measure_permit_issue_result_t){.issued = true, .reason = HW_MEASURE_PERMIT_OK};
}

hw_measure_permit_issue_result_t hw_measure_permit_issue(hw_measure_permit_t *permit,
                                                         const hw_measure_permit_issue_input_t *input,
                                                         uint32_t now_ms)
{
    const hw_measure_permit_issue_result_t result = hw_measure_permit_check_issue(input);
    if (permit == NULL)
    {
        return (hw_measure_permit_issue_result_t){.issued = false, .reason = HW_MEASURE_PERMIT_REJECT_INVALID};
    }
    if (!result.issued)
    {
        hw_measure_permit_init(permit);
        return result;
    }

    permit->valid = true;
    permit->consumed = false;
    permit->issued_at_ms = now_ms;
    permit->range_id = input->range_id;
    return result;
}

hw_measure_permit_validate_result_t hw_measure_permit_validate(hw_measure_permit_t *permit,
                                                               const hw_measure_permit_validate_input_t *input,
                                                               uint32_t now_ms)
{
    if ((permit == NULL) || !permit->valid)
    {
        return (hw_measure_permit_validate_result_t){.allowed = false,
                                                     .reason = HW_MEASURE_PERMIT_REJECT_INVALID};
    }

    if (permit->consumed)
    {
        return (hw_measure_permit_validate_result_t){.allowed = false,
                                                     .reason = HW_MEASURE_PERMIT_REJECT_CONSUMED};
    }

    permit->consumed = true;

    if (input == NULL)
    {
        return (hw_measure_permit_validate_result_t){.allowed = false,
                                                     .reason = HW_MEASURE_PERMIT_REJECT_INVALID};
    }

    if (deadline_reached(now_ms, permit->issued_at_ms + HW_MEASURE_PERMIT_TTL_MS + 1u))
    {
        return (hw_measure_permit_validate_result_t){.allowed = false,
                                                     .reason = HW_MEASURE_PERMIT_REJECT_EXPIRED};
    }
    if (input->safety_fault_mask != 0u)
    {
        return (hw_measure_permit_validate_result_t){.allowed = false, .reason = HW_MEASURE_PERMIT_REJECT_FAULT};
    }
    if (input->charger != HW_CHARGER_ABSENT)
    {
        return (hw_measure_permit_validate_result_t){.allowed = false, .reason = HW_MEASURE_PERMIT_REJECT_CHARGER};
    }
    if ((input->range != HW_RANGE_READY) || (input->range_id != permit->range_id))
    {
        return (hw_measure_permit_validate_result_t){.allowed = false, .reason = HW_MEASURE_PERMIT_REJECT_RANGE};
    }
    if (input->k1_state != HW_K1_STATE_SAFE)
    {
        return (hw_measure_permit_validate_result_t){.allowed = false, .reason = HW_MEASURE_PERMIT_REJECT_K1};
    }

    return (hw_measure_permit_validate_result_t){.allowed = true, .reason = HW_MEASURE_PERMIT_OK};
}

const char *hw_measure_permit_rejection_string(hw_measure_permit_rejection_t reason)
{
    switch (reason)
    {
    case HW_MEASURE_PERMIT_OK:
        return "OK";
    case HW_MEASURE_PERMIT_REJECT_CHARGER:
        return "CHARGER";
    case HW_MEASURE_PERMIT_REJECT_RESIDUAL:
        return "RESIDUAL";
    case HW_MEASURE_PERMIT_REJECT_RESIDUAL_STALE:
        return "RESIDUAL_STALE";
    case HW_MEASURE_PERMIT_REJECT_BATTERY:
        return "BATTERY";
    case HW_MEASURE_PERMIT_REJECT_BATTERY_STALE:
        return "BATTERY_STALE";
    case HW_MEASURE_PERMIT_REJECT_RANGE:
        return "RANGE";
    case HW_MEASURE_PERMIT_REJECT_RANGE_ID:
        return "RANGE_ID";
    case HW_MEASURE_PERMIT_REJECT_K1:
        return "K1";
    case HW_MEASURE_PERMIT_REJECT_FAULT:
        return "FAULT";
    case HW_MEASURE_PERMIT_REJECT_SAFETY:
        return "SAFETY";
    case HW_MEASURE_PERMIT_REJECT_EXPIRED:
        return "EXPIRED";
    case HW_MEASURE_PERMIT_REJECT_CONSUMED:
        return "CONSUMED";
    case HW_MEASURE_PERMIT_REJECT_INVALID:
    default:
        return "INVALID";
    }
}
