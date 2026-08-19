#include "hardware/hw_excitation.h"

#include <stddef.h>

static const int16_t g_sine_q15[HW_EXCITATION_LUT_POINTS] = {
    0,     4560,  9032,  13328, 17364, 21062, 24351, 27165, 29451, 31163,
    32269, 32747, 32587, 31794, 30381, 28377, 25821, 22762, 19260, 15383,
    11207, 6813,  2286,  -2286, -6813, -11207, -15383, -19260, -22762, -25821,
    -28377, -30381, -31794, -32587, -32747, -32269, -31163, -29451, -27165, -24351,
    -21062, -17364, -13328, -9032, -4560,
};

_Static_assert((sizeof(g_sine_q15) / sizeof(g_sine_q15[0])) == HW_EXCITATION_LUT_POINTS,
               "sine LUT length");

static int32_t rounded_div(int32_t numerator, int32_t denominator)
{
    if (denominator <= 0)
    {
        return 0;
    }
    if (numerator >= 0)
    {
        return (numerator + (denominator / 2)) / denominator;
    }
    return (numerator - (denominator / 2)) / denominator;
}

void hw_excitation_init(hw_excitation_t *excitation)
{
    if (excitation == NULL)
    {
        return;
    }
    excitation->mode = HW_EXCITATION_MODE_OFF;
}

hw_excitation_mode_t hw_excitation_mode(const hw_excitation_t *excitation)
{
    return (excitation == NULL) ? HW_EXCITATION_MODE_OFF : excitation->mode;
}

void hw_excitation_set_mode(hw_excitation_t *excitation, hw_excitation_mode_t mode)
{
    if (excitation == NULL)
    {
        return;
    }
    excitation->mode = mode;
}

const int16_t *hw_excitation_sine_q15(void)
{
    return g_sine_q15;
}

uint16_t hw_excitation_peak_q8(hw_excitation_amp_t amplitude)
{
    switch (amplitude)
    {
    case HW_EXCITATION_AMP_100MVRMS:
        return (uint16_t)HW_EXCITATION_PEAK_Q8_100MVRMS;
    case HW_EXCITATION_AMP_500MVRMS:
        return (uint16_t)HW_EXCITATION_PEAK_Q8_500MVRMS;
    case HW_EXCITATION_AMP_INVALID:
    default:
        return 0u;
    }
}

uint16_t hw_excitation_amplitude_mvrms(hw_excitation_amp_t amplitude)
{
    switch (amplitude)
    {
    case HW_EXCITATION_AMP_100MVRMS:
        return 100u;
    case HW_EXCITATION_AMP_500MVRMS:
        return 500u;
    case HW_EXCITATION_AMP_INVALID:
    default:
        return 0u;
    }
}

bsp_status_t hw_excitation_validate_amplitude(hw_range_id_t range_id, hw_excitation_amp_t amplitude)
{
    if ((range_id == HW_RANGE_ID_INVALID) || (amplitude == HW_EXCITATION_AMP_INVALID))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    if ((amplitude == HW_EXCITATION_AMP_500MVRMS) && (range_id == HW_RANGE_ID_10R))
    {
        return BSP_STATUS_NOT_SUPPORTED;
    }

    if ((amplitude != HW_EXCITATION_AMP_100MVRMS) && (amplitude != HW_EXCITATION_AMP_500MVRMS))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    switch (range_id)
    {
    case HW_RANGE_ID_10R:
    case HW_RANGE_ID_100R:
    case HW_RANGE_ID_1K:
    case HW_RANGE_ID_10K:
    case HW_RANGE_ID_100K:
    case HW_RANGE_ID_1M:
        return BSP_STATUS_OK;
    case HW_RANGE_ID_INVALID:
    default:
        return BSP_STATUS_INVALID_ARG;
    }
}

bsp_status_t hw_excitation_freq_profile(hw_excitation_freq_t frequency,
                                        hw_excitation_freq_profile_t *profile)
{
    if (profile == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    switch (frequency)
    {
    case HW_EXCITATION_FREQ_100HZ:
        profile->frequency_hz = 100u;
        profile->rcr = 99u;
        profile->ccr_update_hz = 4500u;
        profile->sine_settle_ms = 80u;
        return BSP_STATUS_OK;
    case HW_EXCITATION_FREQ_1KHZ:
        profile->frequency_hz = 1000u;
        profile->rcr = 9u;
        profile->ccr_update_hz = 45000u;
        profile->sine_settle_ms = 8u;
        return BSP_STATUS_OK;
    case HW_EXCITATION_FREQ_10KHZ:
        profile->frequency_hz = 10000u;
        profile->rcr = 0u;
        profile->ccr_update_hz = 450000u;
        profile->sine_settle_ms = 5u;
        return BSP_STATUS_OK;
    case HW_EXCITATION_FREQ_INVALID:
    default:
        return BSP_STATUS_INVALID_ARG;
    }
}

bsp_status_t hw_excitation_fill_ccr_table(uint16_t *ccr, uint32_t count, hw_excitation_amp_t amplitude)
{
    const uint16_t peak_q8 = hw_excitation_peak_q8(amplitude);
    const int32_t denominator = (int32_t)HW_EXCITATION_SINE_Q15_MAX * 256;

    if ((ccr == NULL) || (count != HW_EXCITATION_LUT_POINTS) || (peak_q8 == 0u))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    for (uint32_t i = 0u; i < HW_EXCITATION_LUT_POINTS; i++)
    {
        const int32_t numerator = (int32_t)g_sine_q15[i] * (int32_t)peak_q8;
        const int32_t delta = rounded_div(numerator, denominator);
        const int32_t value = (int32_t)HW_EXCITATION_PWM_CENTER + delta;
        if ((value < 0) || (value > (int32_t)HW_EXCITATION_PWM_ARR))
        {
            return BSP_STATUS_ERROR;
        }
        ccr[i] = (uint16_t)value;
    }

    return BSP_STATUS_OK;
}

bool hw_excitation_parse_freq_token(const char *token, hw_excitation_freq_t *frequency)
{
    if ((token == NULL) || (frequency == NULL))
    {
        return false;
    }
    if ((token[0] == '1') && (token[1] == '0') && (token[2] == '0') && (token[3] == '\0'))
    {
        *frequency = HW_EXCITATION_FREQ_100HZ;
        return true;
    }
    if ((token[0] == '1') && (token[1] == 'k') && (token[2] == '\0'))
    {
        *frequency = HW_EXCITATION_FREQ_1KHZ;
        return true;
    }
    if ((token[0] == '1') && (token[1] == '0') && (token[2] == 'k') && (token[3] == '\0'))
    {
        *frequency = HW_EXCITATION_FREQ_10KHZ;
        return true;
    }
    return false;
}

bool hw_excitation_parse_amp_token(const char *token, hw_excitation_amp_t *amplitude)
{
    if ((token == NULL) || (amplitude == NULL))
    {
        return false;
    }
    if ((token[0] == '1') && (token[1] == '0') && (token[2] == '0') && (token[3] == 'm') && (token[4] == '\0'))
    {
        *amplitude = HW_EXCITATION_AMP_100MVRMS;
        return true;
    }
    if ((token[0] == '5') && (token[1] == '0') && (token[2] == '0') && (token[3] == 'm') && (token[4] == '\0'))
    {
        *amplitude = HW_EXCITATION_AMP_500MVRMS;
        return true;
    }
    return false;
}

bool hw_excitation_parse_range_token(const char *token, hw_range_id_t *range_id)
{
    if ((token == NULL) || (range_id == NULL))
    {
        return false;
    }
    if ((token[0] == '1') && (token[1] == '0') && (token[2] == 'r') && (token[3] == '\0'))
    {
        *range_id = HW_RANGE_ID_10R;
        return true;
    }
    if ((token[0] == '1') && (token[1] == '0') && (token[2] == '0') && (token[3] == 'r') && (token[4] == '\0'))
    {
        *range_id = HW_RANGE_ID_100R;
        return true;
    }
    if ((token[0] == '1') && (token[1] == 'k') && (token[2] == '\0'))
    {
        *range_id = HW_RANGE_ID_1K;
        return true;
    }
    if ((token[0] == '1') && (token[1] == '0') && (token[2] == 'k') && (token[3] == '\0'))
    {
        *range_id = HW_RANGE_ID_10K;
        return true;
    }
    if ((token[0] == '1') && (token[1] == '0') && (token[2] == '0') && (token[3] == 'k') && (token[4] == '\0'))
    {
        *range_id = HW_RANGE_ID_100K;
        return true;
    }
    if ((token[0] == '1') && (token[1] == 'm') && (token[2] == '\0'))
    {
        *range_id = HW_RANGE_ID_1M;
        return true;
    }
    return false;
}

const char *hw_excitation_freq_token(hw_excitation_freq_t frequency)
{
    switch (frequency)
    {
    case HW_EXCITATION_FREQ_100HZ:
        return "100";
    case HW_EXCITATION_FREQ_1KHZ:
        return "1k";
    case HW_EXCITATION_FREQ_10KHZ:
        return "10k";
    case HW_EXCITATION_FREQ_INVALID:
    default:
        return "INVALID";
    }
}

const char *hw_excitation_amp_token(hw_excitation_amp_t amplitude)
{
    switch (amplitude)
    {
    case HW_EXCITATION_AMP_100MVRMS:
        return "100m";
    case HW_EXCITATION_AMP_500MVRMS:
        return "500m";
    case HW_EXCITATION_AMP_INVALID:
    default:
        return "INVALID";
    }
}

const char *hw_excitation_mode_string(hw_excitation_mode_t mode)
{
    switch (mode)
    {
    case HW_EXCITATION_MODE_OFF:
        return "OFF";
    case HW_EXCITATION_MODE_NEUTRAL:
        return "NEUTRAL";
    case HW_EXCITATION_MODE_SINE:
        return "SINE";
    default:
        return "UNKNOWN";
    }
}
