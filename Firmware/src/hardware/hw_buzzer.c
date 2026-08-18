#include "hardware/hw_buzzer.h"

#include "bsp/bsp_timers.h"
#include "hardware/hw_buzzer_policy.h"

static hw_buzzer_policy_t g_policy;

static void timer_stop(void)
{
    bsp_timer4_buzzer_stop();
}

bsp_status_t hw_buzzer_init(void)
{
    const bsp_status_t status = bsp_timer4_buzzer_init();
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    hw_buzzer_policy_init(&g_policy);
    timer_stop();

    return BSP_STATUS_OK;
}

bsp_status_t hw_buzzer_play_tone(uint16_t frequency_hz, uint16_t duration_ms, uint32_t now_ms)
{
    const bsp_status_t policy_status = hw_buzzer_policy_play(&g_policy, frequency_hz, duration_ms, now_ms);
    if (policy_status != BSP_STATUS_OK)
    {
        timer_stop();
        return policy_status;
    }

    if (!hw_buzzer_policy_is_active(&g_policy))
    {
        timer_stop();
        return BSP_STATUS_OK;
    }

    const bsp_status_t status = bsp_timer4_buzzer_start(frequency_hz);
    if (status != BSP_STATUS_OK)
    {
        hw_buzzer_policy_mute(&g_policy);
        return status;
    }

    return BSP_STATUS_OK;
}

void hw_buzzer_step(uint32_t now_ms)
{
    if (hw_buzzer_policy_step(&g_policy, now_ms))
    {
        timer_stop();
    }
}

void hw_buzzer_mute(void)
{
    hw_buzzer_policy_mute(&g_policy);
    timer_stop();
}

void hw_buzzer_set_enabled(bool enabled)
{
    hw_buzzer_policy_set_enabled(&g_policy, enabled);
    if (!hw_buzzer_policy_is_active(&g_policy))
    {
        timer_stop();
    }
}

void hw_buzzer_on_quiet_changed(bool requested)
{
    if (hw_buzzer_policy_set_quiet(&g_policy, requested))
    {
        timer_stop();
    }
}

bool hw_buzzer_is_active(void)
{
    return hw_buzzer_policy_is_active(&g_policy);
}
