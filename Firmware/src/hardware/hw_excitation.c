#include "hardware/hw_excitation.h"

#include <stddef.h>
#include <string.h>

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
    if (((unsigned)range_id > (unsigned)HW_RANGE_ID_1M) ||
        ((unsigned)amplitude > (unsigned)HW_EXCITATION_AMP_500MVRMS))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    return ((range_id == HW_RANGE_ID_10R) && (amplitude == HW_EXCITATION_AMP_500MVRMS)) ?
               BSP_STATUS_NOT_SUPPORTED : BSP_STATUS_OK;
}

uint32_t hw_excitation_frequency_hz(hw_excitation_freq_t frequency)
{
    static const uint16_t frequencies[] = {10u, 1000u, 10000u};
    return ((unsigned)frequency < 3u) ? frequencies[(unsigned)frequency] : 0u;
}

bsp_status_t hw_excitation_freq_profile(hw_excitation_freq_t frequency,
                                        hw_excitation_freq_profile_t *profile)
{
    if ((profile == NULL) || ((unsigned)frequency >= 3u))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    profile->frequency_hz = hw_excitation_frequency_hz(frequency);
    profile->table_points = (frequency == HW_EXCITATION_FREQ_10HZ) ? 180u : 45u;
    profile->ccr_update_hz = profile->frequency_hz * profile->table_points;
    profile->rcr = (uint8_t)(HW_EXCITATION_CARRIER_HZ / profile->ccr_update_hz - 1u);
    profile->sine_settle_ms = (frequency == HW_EXCITATION_FREQ_10KHZ) ?
                                 5u : 8000u / profile->frequency_hz;
    return BSP_STATUS_OK;
}

bsp_status_t hw_excitation_fill_ccr_table(uint16_t *ccr, uint32_t count, hw_excitation_amp_t amplitude)
{
    const uint16_t peak_q8 = hw_excitation_peak_q8(amplitude);
    const int32_t denominator = (int32_t)HW_EXCITATION_SINE_Q15_MAX * 256;

    if ((ccr == NULL) || ((count != HW_EXCITATION_LUT_POINTS) && (count != HW_EXCITATION_LUT_POINTS_10HZ)) || (peak_q8 == 0u))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    for (uint32_t i = 0u; i < count; i++)
    {
        int32_t sine = g_sine_q15[(count == HW_EXCITATION_LUT_POINTS) ? i : (i / 4u)];
        if (count == HW_EXCITATION_LUT_POINTS_10HZ)
        {
            /* Four equally spaced samples per existing 45-point segment. */
            const uint32_t base = i / 4u;
            const int32_t next = g_sine_q15[(base + 1u) % HW_EXCITATION_LUT_POINTS];
            sine += ((next - sine) * (int32_t)(i % 4u)) / 4;
        }
        const int32_t numerator = sine * (int32_t)peak_q8;
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

static const char *const frequency_tokens[] = {"10", "1k", "10k"};
static const char *const amplitude_tokens[] = {"100m", "500m"};
static const char *const range_tokens[] = {"10r", "100r", "1k", "10k", "100k", "1m"};

static uint8_t token_index(const char *token, const char *const *tokens, uint8_t count)
{
    if (token != NULL)
    {
        for (uint8_t i = 0u; i < count; i++)
        {
            if (strcmp(token, tokens[i]) == 0) { return i; }
        }
    }
    return 255u;
}

bool hw_excitation_parse_freq_token(const char *token, hw_excitation_freq_t *frequency)
{
    const uint8_t index = token_index(token, frequency_tokens, 3u);
    if (frequency == NULL || index == 255u) { return false; }
    *frequency = (hw_excitation_freq_t)index;
    return true;
}

bool hw_excitation_parse_amp_token(const char *token, hw_excitation_amp_t *amplitude)
{
    const uint8_t index = token_index(token, amplitude_tokens, 2u);
    if (amplitude == NULL || index == 255u) { return false; }
    *amplitude = (hw_excitation_amp_t)index;
    return true;
}

bool hw_excitation_parse_range_token(const char *token, hw_range_id_t *range_id)
{
    const uint8_t index = token_index(token, range_tokens, 6u);
    if (range_id == NULL || index == 255u) { return false; }
    *range_id = (hw_range_id_t)index;
    return true;
}

const char *hw_excitation_freq_token(hw_excitation_freq_t frequency)
{
    return ((unsigned)frequency < 3u) ? frequency_tokens[(unsigned)frequency] : "INVALID";
}

const char *hw_excitation_amp_token(hw_excitation_amp_t amplitude)
{
    return ((unsigned)amplitude < 2u) ? amplitude_tokens[(unsigned)amplitude] : "INVALID";
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
    case HW_EXCITATION_MODE_DC_PILOT:
        return "DC_PILOT";
    default:
        return "UNKNOWN";
    }
}
