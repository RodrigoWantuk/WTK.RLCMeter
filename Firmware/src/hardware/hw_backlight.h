#ifndef WTK_HW_BACKLIGHT_H
#define WTK_HW_BACKLIGHT_H

#include <stdint.h>

#include "bsp/bsp_status.h"

bsp_status_t hw_backlight_init(void);
bsp_status_t hw_backlight_set_percent(uint8_t percent);
uint8_t hw_backlight_get_percent(void);
uint32_t hw_backlight_pwm_hz(void);

#endif
