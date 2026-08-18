#ifndef WTK_HW_BUZZER_POLICY_H
#define WTK_HW_BUZZER_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"

typedef struct
{
    bool enabled;
    bool active;
    bool quiet_requested;
    uint32_t stop_ms;
} hw_buzzer_policy_t;

void hw_buzzer_policy_init(hw_buzzer_policy_t *policy);
bsp_status_t hw_buzzer_policy_play(hw_buzzer_policy_t *policy,
                                   uint16_t frequency_hz,
                                   uint16_t duration_ms,
                                   uint32_t now_ms);
bool hw_buzzer_policy_step(hw_buzzer_policy_t *policy, uint32_t now_ms);
bool hw_buzzer_policy_set_quiet(hw_buzzer_policy_t *policy, bool requested);
void hw_buzzer_policy_mute(hw_buzzer_policy_t *policy);
void hw_buzzer_policy_set_enabled(hw_buzzer_policy_t *policy, bool enabled);
bool hw_buzzer_policy_is_active(const hw_buzzer_policy_t *policy);

#endif
