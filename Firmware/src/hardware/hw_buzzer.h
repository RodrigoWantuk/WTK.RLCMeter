#ifndef WTK_HW_BUZZER_H
#define WTK_HW_BUZZER_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"

typedef struct
{
    uint16_t frequency_hz;
    uint16_t duration_ms;
} hw_buzzer_tone_t;

bsp_status_t hw_buzzer_init(void);
bsp_status_t hw_buzzer_play_tone(uint16_t frequency_hz, uint16_t duration_ms, uint32_t now_ms);
void hw_buzzer_step(uint32_t now_ms);
void hw_buzzer_mute(void);
void hw_buzzer_set_enabled(bool enabled);
bool hw_buzzer_is_active(void);

#endif
