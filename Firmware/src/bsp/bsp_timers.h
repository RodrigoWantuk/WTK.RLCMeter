#ifndef WTK_BSP_TIMERS_H
#define WTK_BSP_TIMERS_H

#include <stdint.h>

#include "bsp/bsp_status.h"

bsp_status_t bsp_timer3_pwm_ch3_init(uint32_t pwm_hz, uint16_t steps);
bsp_status_t bsp_timer3_pwm_ch3_set_duty(uint16_t duty_steps);
bsp_status_t bsp_timer4_buzzer_init(void);
bsp_status_t bsp_timer4_buzzer_start(uint16_t frequency_hz);
void bsp_timer4_buzzer_stop(void);

#endif
