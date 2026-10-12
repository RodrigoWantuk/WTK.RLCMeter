#include "hardware/hw_residual.h"

#include <stddef.h>
#include <math.h>

void hw_residual_policy_init(hw_residual_policy_t *policy)
{
    if (policy == NULL)
    {
        return;
    }

    policy->consecutive_safe_count = 0u;
    policy->state = HW_RESIDUAL_UNKNOWN;
}

hw_residual_state_t hw_residual_policy_evaluate(hw_residual_policy_t *policy,
                                                const hw_residual_policy_input_t *input)
{
    if ((policy == NULL) || (input == NULL))
    {
        if (policy != NULL)
        {
            policy->consecutive_safe_count = 0u;
            policy->state = HW_RESIDUAL_UNKNOWN;
        }
        return HW_RESIDUAL_UNKNOWN;
    }

    if (input->saturated)
    {
        policy->consecutive_safe_count = 0u;
        policy->state = HW_RESIDUAL_SATURATED;
        return policy->state;
    }

    if (!input->valid || !isfinite(input->terminal_hi_to_vmid_v) ||
        !isfinite(input->terminal_lo_to_vmid_v) || !isfinite(input->residual_diff_v))
    {
        policy->consecutive_safe_count = 0u;
        policy->state = HW_RESIDUAL_UNKNOWN;
        return policy->state;
    }

    const float hi = fabsf(input->terminal_hi_to_vmid_v);
    const float lo = fabsf(input->terminal_lo_to_vmid_v);
    const float diff = fabsf(input->residual_diff_v);
    const float terminal_max = (hi > lo) ? hi : lo;
    const float maximum = (terminal_max > diff) ? terminal_max : diff;
    if (maximum >= HW_RESIDUAL_BLOCK_V)
    {
        policy->consecutive_safe_count = 0u;
        policy->state = HW_RESIDUAL_UNSAFE;
        return policy->state;
    }

    if (policy->state == HW_RESIDUAL_SAFE)
    {
        policy->consecutive_safe_count = HW_RESIDUAL_REQUIRED_SAFE_COUNT;
        return policy->state;
    }

    if (maximum <= HW_RESIDUAL_RELEASE_V)
    {
        if (policy->consecutive_safe_count < HW_RESIDUAL_REQUIRED_SAFE_COUNT)
        {
            policy->consecutive_safe_count++;
        }
        policy->state = (policy->consecutive_safe_count >= HW_RESIDUAL_REQUIRED_SAFE_COUNT) ? HW_RESIDUAL_SAFE
                                                                                            : HW_RESIDUAL_UNKNOWN;
        return policy->state;
    }

    policy->consecutive_safe_count = 0u;
    if (policy->state != HW_RESIDUAL_SAFE)
    {
        policy->state = HW_RESIDUAL_UNKNOWN;
    }
    return policy->state;
}

float hw_residual_terminal_to_vmid_voltage(float vmid_v, float sense_v)
{
    return (sense_v - vmid_v) * HW_RESIDUAL_TRANSFER_K;
}

float hw_residual_terminal_voltage(float vmid_v, float sense_v)
{
    return vmid_v + hw_residual_terminal_to_vmid_voltage(vmid_v, sense_v);
}

float hw_residual_differential_voltage(float safe_hi_v, float safe_lo_v)
{
    return safe_hi_v - safe_lo_v;
}

bool hw_residual_raw_is_saturated(uint16_t raw)
{
    return (raw <= HW_RESIDUAL_ADC_SATURATED_LOW_MAX) || (raw >= HW_RESIDUAL_ADC_SATURATED_HIGH_MIN);
}
