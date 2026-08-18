#include "hardware/hw_backlight.h"

#include "bsp/bsp_timers.h"

enum
{
    BACKLIGHT_PWM_HZ = 1000u,
    BACKLIGHT_PWM_STEPS = 1000u,
};

static uint8_t g_backlight_percent = 0u;

bsp_status_t hw_backlight_init(void)
{
    const bsp_status_t status = bsp_timer3_pwm_ch3_init(BACKLIGHT_PWM_HZ, BACKLIGHT_PWM_STEPS);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    g_backlight_percent = 0u;
    return BSP_STATUS_OK;
}

bsp_status_t hw_backlight_set_percent(uint8_t percent)
{
    if (percent > 100u)
    {
        percent = 100u;
    }

    const bsp_status_t status =
        bsp_timer3_pwm_ch3_set_duty((uint16_t)(((uint32_t)percent * BACKLIGHT_PWM_STEPS) / 100u));
    if (status != BSP_STATUS_OK)
    {
        return status;
    }

    g_backlight_percent = percent;
    return BSP_STATUS_OK;
}

uint8_t hw_backlight_get_percent(void)
{
    return g_backlight_percent;
}

uint32_t hw_backlight_pwm_hz(void)
{
    return BACKLIGHT_PWM_HZ;
}
