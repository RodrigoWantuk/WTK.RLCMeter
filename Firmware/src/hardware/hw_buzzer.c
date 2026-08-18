#include "hardware/hw_buzzer.h"

#include "bsp/bsp_timers.h"

enum
{
    BUZZER_MIN_HZ = 100u,
    BUZZER_MAX_HZ = 4000u,
};

static bool g_enabled = true;
static bool g_active = false;
static uint32_t g_stop_ms = 0u;

static void timer_stop(void)
{
    bsp_timer4_buzzer_stop();
    g_active = false;
}

bsp_status_t hw_buzzer_init(void)
{
    const bsp_status_t status = bsp_timer4_buzzer_init();
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    timer_stop();

    return BSP_STATUS_OK;
}

bsp_status_t hw_buzzer_play_tone(uint16_t frequency_hz, uint16_t duration_ms, uint32_t now_ms)
{
    if ((frequency_hz < BUZZER_MIN_HZ) || (frequency_hz > BUZZER_MAX_HZ) || (duration_ms == 0u))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    if (!g_enabled)
    {
        timer_stop();
        return BSP_STATUS_OK;
    }

    const bsp_status_t status = bsp_timer4_buzzer_start(frequency_hz);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }

    g_stop_ms = now_ms + duration_ms;
    g_active = true;
    return BSP_STATUS_OK;
}

void hw_buzzer_step(uint32_t now_ms)
{
    if (g_active && ((now_ms - g_stop_ms) < 0x80000000u))
    {
        timer_stop();
    }
}

void hw_buzzer_mute(void)
{
    timer_stop();
}

void hw_buzzer_set_enabled(bool enabled)
{
    g_enabled = enabled;
    if (!enabled)
    {
        timer_stop();
    }
}

bool hw_buzzer_is_active(void)
{
    return g_active;
}
