#include "hardware/hw_buzzer_policy.h"

#include <stddef.h>

enum
{
    BUZZER_POLICY_MIN_HZ = 100u,
    BUZZER_POLICY_MAX_HZ = 4000u,
};

void hw_buzzer_policy_init(hw_buzzer_policy_t *policy)
{
    if (policy == NULL)
    {
        return;
    }

    policy->enabled = true;
    policy->active = false;
    policy->quiet_requested = false;
    policy->stop_ms = 0u;
}

bsp_status_t hw_buzzer_policy_play(hw_buzzer_policy_t *policy,
                                   uint16_t frequency_hz,
                                   uint16_t duration_ms,
                                   uint32_t now_ms)
{
    if (policy == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    if ((frequency_hz < BUZZER_POLICY_MIN_HZ) || (frequency_hz > BUZZER_POLICY_MAX_HZ) ||
        (duration_ms == 0u))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    if (policy->quiet_requested)
    {
        policy->active = false;
        return BSP_STATUS_BUSY;
    }

    if (!policy->enabled)
    {
        policy->active = false;
        return BSP_STATUS_OK;
    }

    policy->stop_ms = now_ms + duration_ms;
    policy->active = true;
    return BSP_STATUS_OK;
}

bool hw_buzzer_policy_step(hw_buzzer_policy_t *policy, uint32_t now_ms)
{
    if ((policy == NULL) || !policy->active)
    {
        return false;
    }

    if ((now_ms - policy->stop_ms) < 0x80000000u)
    {
        policy->active = false;
        return true;
    }

    return false;
}

bool hw_buzzer_policy_set_quiet(hw_buzzer_policy_t *policy, bool requested)
{
    if (policy == NULL)
    {
        return false;
    }

    policy->quiet_requested = requested;
    if (requested && policy->active)
    {
        policy->active = false;
        return true;
    }

    return false;
}

void hw_buzzer_policy_mute(hw_buzzer_policy_t *policy)
{
    if (policy == NULL)
    {
        return;
    }

    policy->active = false;
}

void hw_buzzer_policy_set_enabled(hw_buzzer_policy_t *policy, bool enabled)
{
    if (policy == NULL)
    {
        return;
    }

    policy->enabled = enabled;
    if (!enabled)
    {
        policy->active = false;
    }
}

bool hw_buzzer_policy_is_active(const hw_buzzer_policy_t *policy)
{
    return (policy != NULL) && policy->active;
}
