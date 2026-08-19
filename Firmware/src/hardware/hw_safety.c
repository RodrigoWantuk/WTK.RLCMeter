#include "hardware/hw_safety.h"

#include <stddef.h>

static hw_safety_primary_blocker_t primary_from_flags(uint32_t flags)
{
    if ((flags & HW_SAFETY_BLOCK_FAULT) != 0u)
    {
        return HW_SAFETY_BLOCKED_FAULT;
    }
    if ((flags & HW_SAFETY_BLOCK_CHARGER) != 0u)
    {
        return HW_SAFETY_BLOCKED_CHARGER;
    }
    if ((flags & HW_SAFETY_BLOCK_SENSOR_INVALID) != 0u)
    {
        return HW_SAFETY_BLOCKED_SENSOR_INVALID;
    }
    if ((flags & HW_SAFETY_BLOCK_RESIDUAL) != 0u)
    {
        return HW_SAFETY_BLOCKED_RESIDUAL;
    }
    if ((flags & HW_SAFETY_BLOCK_SUPPLY) != 0u)
    {
        return HW_SAFETY_BLOCKED_SUPPLY;
    }
    if ((flags & HW_SAFETY_BLOCK_RANGE) != 0u)
    {
        return HW_SAFETY_BLOCKED_RANGE;
    }

    return HW_SAFETY_MEASURE_ALLOWED;
}

hw_safety_result_t hw_safety_evaluate(const hw_safety_input_t *input)
{
    uint32_t flags = HW_SAFETY_BLOCK_SENSOR_INVALID;
    bool battery_low = false;

    if (input != NULL)
    {
        flags = HW_SAFETY_BLOCK_NONE;
        if (input->application_fault)
        {
            flags |= HW_SAFETY_BLOCK_FAULT;
        }

        switch (input->charger)
        {
        case HW_CHARGER_ABSENT:
            break;
        case HW_CHARGER_PRESENT:
            flags |= HW_SAFETY_BLOCK_CHARGER;
            break;
        case HW_CHARGER_UNKNOWN:
        default:
            flags |= HW_SAFETY_BLOCK_SENSOR_INVALID;
            break;
        }

        switch (input->residual)
        {
        case HW_RESIDUAL_SAFE:
            break;
        case HW_RESIDUAL_UNSAFE:
            flags |= HW_SAFETY_BLOCK_RESIDUAL;
            break;
        case HW_RESIDUAL_UNKNOWN:
        case HW_RESIDUAL_SATURATED:
        default:
            flags |= HW_SAFETY_BLOCK_SENSOR_INVALID;
            break;
        }

        switch (input->battery)
        {
        case HW_BATTERY_OK:
            break;
        case HW_BATTERY_LOW:
            battery_low = true;
            break;
        case HW_BATTERY_CRITICAL:
            flags |= HW_SAFETY_BLOCK_SUPPLY;
            break;
        case HW_BATTERY_UNKNOWN:
        default:
            flags |= HW_SAFETY_BLOCK_SUPPLY | HW_SAFETY_BLOCK_SENSOR_INVALID;
            break;
        }

        switch (input->range)
        {
        case HW_RANGE_READY:
            break;
        case HW_RANGE_DISABLED:
        case HW_RANGE_TRANSITIONING:
        case HW_RANGE_INVALID:
        default:
            flags |= HW_SAFETY_BLOCK_RANGE;
            break;
        }
    }

    return (hw_safety_result_t){
        .measure_allowed = (flags == HW_SAFETY_BLOCK_NONE),
        .battery_low = battery_low,
        .blocker_flags = flags,
        .primary_blocker = primary_from_flags(flags),
    };
}

const char *hw_safety_primary_blocker_string(hw_safety_primary_blocker_t blocker)
{
    switch (blocker)
    {
    case HW_SAFETY_MEASURE_ALLOWED:
        return "MEASURE_ALLOWED";
    case HW_SAFETY_BLOCKED_FAULT:
        return "BLOCKED_FAULT";
    case HW_SAFETY_BLOCKED_CHARGER:
        return "BLOCKED_CHARGER";
    case HW_SAFETY_BLOCKED_SENSOR_INVALID:
        return "BLOCKED_SENSOR_INVALID";
    case HW_SAFETY_BLOCKED_RESIDUAL:
        return "BLOCKED_RESIDUAL";
    case HW_SAFETY_BLOCKED_SUPPLY:
        return "BLOCKED_SUPPLY";
    case HW_SAFETY_BLOCKED_RANGE:
        return "BLOCKED_RANGE";
    default:
        return "UNKNOWN";
    }
}

const char *hw_charger_state_string(hw_charger_state_t state)
{
    switch (state)
    {
    case HW_CHARGER_ABSENT:
        return "ABSENT";
    case HW_CHARGER_PRESENT:
        return "PRESENT";
    case HW_CHARGER_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

const char *hw_residual_state_string(hw_residual_state_t state)
{
    switch (state)
    {
    case HW_RESIDUAL_SAFE:
        return "SAFE";
    case HW_RESIDUAL_UNSAFE:
        return "UNSAFE";
    case HW_RESIDUAL_SATURATED:
        return "SATURATED";
    case HW_RESIDUAL_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

const char *hw_battery_state_string(hw_battery_state_t state)
{
    switch (state)
    {
    case HW_BATTERY_OK:
        return "OK";
    case HW_BATTERY_LOW:
        return "LOW";
    case HW_BATTERY_CRITICAL:
        return "CRITICAL";
    case HW_BATTERY_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

const char *hw_safety_range_state_string(hw_safety_range_state_t state)
{
    switch (state)
    {
    case HW_RANGE_READY:
        return "READY";
    case HW_RANGE_DISABLED:
        return "DISABLED";
    case HW_RANGE_TRANSITIONING:
        return "TRANSITIONING";
    case HW_RANGE_INVALID:
    default:
        return "INVALID";
    }
}
