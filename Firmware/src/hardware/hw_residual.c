#include "hardware/hw_residual.h"

#include <stddef.h>

static float absf_local(float value)
{
    return (value < 0.0f) ? -value : value;
}

static bool within_release(const hw_residual_policy_input_t *input)
{
    return (absf_local(input->safe_hi_v) <= HW_RESIDUAL_RELEASE_V) &&
           (absf_local(input->safe_lo_v) <= HW_RESIDUAL_RELEASE_V) &&
           (absf_local(input->residual_diff_v) <= HW_RESIDUAL_RELEASE_V);
}

static bool reaches_block(const hw_residual_policy_input_t *input)
{
    return (absf_local(input->safe_hi_v) >= HW_RESIDUAL_BLOCK_V) ||
           (absf_local(input->safe_lo_v) >= HW_RESIDUAL_BLOCK_V) ||
           (absf_local(input->residual_diff_v) >= HW_RESIDUAL_BLOCK_V);
}

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
    if ((policy == NULL) || (input == NULL) || !input->valid)
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

    if (reaches_block(input))
    {
        policy->consecutive_safe_count = 0u;
        policy->state = HW_RESIDUAL_UNSAFE;
        return policy->state;
    }

    if (within_release(input))
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

float hw_residual_terminal_voltage(float vmid_v, float sense_v)
{
    return vmid_v + ((sense_v - vmid_v) * HW_RESIDUAL_TRANSFER_K);
}

float hw_residual_differential_voltage(float safe_hi_v, float safe_lo_v)
{
    return safe_hi_v - safe_lo_v;
}

bool hw_residual_raw_is_saturated(uint16_t raw)
{
    return (raw <= HW_RESIDUAL_ADC_SATURATED_LOW_MAX) || (raw >= HW_RESIDUAL_ADC_SATURATED_HIGH_MIN);
}
