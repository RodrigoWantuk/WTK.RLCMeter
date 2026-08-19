#ifndef WTK_HW_EXCITATION_H
#define WTK_HW_EXCITATION_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "hardware/hw_range.h"

enum
{
    HW_EXCITATION_LUT_POINTS = 45u,
    HW_EXCITATION_PWM_PSC = 0u,
    HW_EXCITATION_PWM_ARR = 159u,
    HW_EXCITATION_PWM_PERIOD_TICKS = 160u,
    HW_EXCITATION_PWM_CENTER = 80u,
    HW_EXCITATION_CARRIER_HZ = 450000u,
    HW_EXCITATION_PEAK_Q8_100MVRMS = 1755u,
    HW_EXCITATION_PEAK_Q8_500MVRMS = 8777u,
    HW_EXCITATION_NEUTRAL_SETTLE_MS = 1u,
    HW_EXCITATION_SINE_Q15_MAX = 32767,
};

_Static_assert(HW_EXCITATION_LUT_POINTS == 45u, "frozen LUT length");
_Static_assert(HW_EXCITATION_PWM_ARR == 159u, "frozen TIM1 ARR");
_Static_assert(HW_EXCITATION_PWM_PERIOD_TICKS == (HW_EXCITATION_PWM_ARR + 1u), "PWM period");

typedef enum
{
    HW_EXCITATION_FREQ_100HZ = 0,
    HW_EXCITATION_FREQ_1KHZ,
    HW_EXCITATION_FREQ_10KHZ,
    HW_EXCITATION_FREQ_INVALID = 255,
} hw_excitation_freq_t;

typedef enum
{
    HW_EXCITATION_AMP_100MVRMS = 0,
    HW_EXCITATION_AMP_500MVRMS,
    HW_EXCITATION_AMP_INVALID = 255,
} hw_excitation_amp_t;

typedef enum
{
    HW_EXCITATION_MODE_OFF = 0,
    HW_EXCITATION_MODE_NEUTRAL,
    HW_EXCITATION_MODE_SINE,
} hw_excitation_mode_t;

typedef struct
{
    uint32_t frequency_hz;
    uint8_t rcr;
    uint32_t ccr_update_hz;
    uint32_t sine_settle_ms;
} hw_excitation_freq_profile_t;

typedef struct
{
    hw_excitation_mode_t mode;
} hw_excitation_t;

void hw_excitation_init(hw_excitation_t *excitation);
hw_excitation_mode_t hw_excitation_mode(const hw_excitation_t *excitation);
void hw_excitation_set_mode(hw_excitation_t *excitation, hw_excitation_mode_t mode);

const int16_t *hw_excitation_sine_q15(void);
uint16_t hw_excitation_peak_q8(hw_excitation_amp_t amplitude);
uint16_t hw_excitation_amplitude_mvrms(hw_excitation_amp_t amplitude);
bsp_status_t hw_excitation_validate_amplitude(hw_range_id_t range_id, hw_excitation_amp_t amplitude);
bsp_status_t hw_excitation_freq_profile(hw_excitation_freq_t frequency,
                                        hw_excitation_freq_profile_t *profile);
bsp_status_t hw_excitation_fill_ccr_table(uint16_t *ccr,
                                          uint32_t count,
                                          hw_excitation_amp_t amplitude);
bool hw_excitation_parse_freq_token(const char *token, hw_excitation_freq_t *frequency);
bool hw_excitation_parse_amp_token(const char *token, hw_excitation_amp_t *amplitude);
bool hw_excitation_parse_range_token(const char *token, hw_range_id_t *range_id);
const char *hw_excitation_freq_token(hw_excitation_freq_t frequency);
const char *hw_excitation_amp_token(hw_excitation_amp_t amplitude);
const char *hw_excitation_mode_string(hw_excitation_mode_t mode);

#endif
