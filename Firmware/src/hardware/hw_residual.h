#ifndef WTK_HW_RESIDUAL_H
#define WTK_HW_RESIDUAL_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/hw_safety.h"

enum
{
    HW_RESIDUAL_REQUIRED_SAFE_COUNT = 8u,
    HW_RESIDUAL_ADC_RAW_MAX = 4095u,
    HW_RESIDUAL_ADC_SATURATED_LOW_MAX = 16u,
    HW_RESIDUAL_ADC_SATURATED_HIGH_MIN = 4079u,
};

#define HW_RESIDUAL_RH_OHM (1680000.0f)
#define HW_RESIDUAL_RL_OHM (27000.0f)
#define HW_RESIDUAL_TRANSFER_K (((HW_RESIDUAL_RH_OHM) + (HW_RESIDUAL_RL_OHM)) / (HW_RESIDUAL_RL_OHM))
#define HW_RESIDUAL_RELEASE_V (0.75f)
#define HW_RESIDUAL_BLOCK_V (1.00f)

typedef struct
{
    uint8_t consecutive_safe_count;
    hw_residual_state_t state;
} hw_residual_policy_t;

typedef struct
{
    bool valid;
    bool saturated;
    float safe_hi_v;
    float safe_lo_v;
    float residual_diff_v;
} hw_residual_policy_input_t;

void hw_residual_policy_init(hw_residual_policy_t *policy);
hw_residual_state_t hw_residual_policy_evaluate(hw_residual_policy_t *policy,
                                                const hw_residual_policy_input_t *input);
float hw_residual_terminal_voltage(float vmid_v, float sense_v);
float hw_residual_differential_voltage(float safe_hi_v, float safe_lo_v);
bool hw_residual_raw_is_saturated(uint16_t raw);

#endif
