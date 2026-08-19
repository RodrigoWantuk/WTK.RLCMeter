#include "hardware/hw_excitation.h"

#include <stdio.h>

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int expect_u32(uint32_t actual, uint32_t expected, const char *message)
{
    if (actual != expected)
    {
        (void)fprintf(stderr, "FAIL: %s (got %lu expected %lu)\n",
                      message,
                      (unsigned long)actual,
                      (unsigned long)expected);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;
    const int16_t *sine = hw_excitation_sine_q15();
    failures += expect_true(sine != NULL, "LUT pointer");
    failures += expect_u32(sine[0], 0u, "LUT[0]");
    failures += expect_u32((uint32_t)sine[1], 4560u, "LUT[1]");
    failures += expect_u32((uint32_t)sine[2], 9032u, "LUT[2]");

    for (uint32_t i = 1u; i < HW_EXCITATION_LUT_POINTS; i++)
    {
        if (sine[i] != (int16_t)(-sine[HW_EXCITATION_LUT_POINTS - i]))
        {
            failures += expect_true(false, "LUT odd symmetry");
            break;
        }
    }

    hw_excitation_freq_profile_t profile;
    failures += expect_true(hw_excitation_freq_profile(HW_EXCITATION_FREQ_100HZ, &profile) == BSP_STATUS_OK,
                            "100 Hz profile");
    failures += expect_u32(profile.rcr, 99u, "100 Hz RCR");
    failures += expect_u32(profile.ccr_update_hz, 4500u, "100 Hz CCR update");
    failures += expect_u32(profile.sine_settle_ms, 80u, "100 Hz settle");

    failures += expect_true(hw_excitation_freq_profile(HW_EXCITATION_FREQ_1KHZ, &profile) == BSP_STATUS_OK,
                            "1 kHz profile");
    failures += expect_u32(profile.rcr, 9u, "1 kHz RCR");
    failures += expect_u32(profile.ccr_update_hz, 45000u, "1 kHz CCR update");
    failures += expect_u32(profile.sine_settle_ms, 8u, "1 kHz settle");

    failures += expect_true(hw_excitation_freq_profile(HW_EXCITATION_FREQ_10KHZ, &profile) == BSP_STATUS_OK,
                            "10 kHz profile");
    failures += expect_u32(profile.rcr, 0u, "10 kHz RCR");
    failures += expect_u32(profile.ccr_update_hz, 450000u, "10 kHz CCR update");
    failures += expect_u32(profile.sine_settle_ms, 5u, "10 kHz settle");

    failures += expect_u32(HW_EXCITATION_PWM_PSC, 0u, "PSC");
    failures += expect_u32(HW_EXCITATION_PWM_ARR, 159u, "ARR");
    failures += expect_u32(HW_EXCITATION_CARRIER_HZ, 450000u, "carrier");
    failures += expect_u32(HW_EXCITATION_PEAK_Q8_100MVRMS, 1755u, "100m PEAK_Q8");
    failures += expect_u32(HW_EXCITATION_PEAK_Q8_500MVRMS, 8777u, "500m PEAK_Q8");

    const hw_range_id_t ranges[] = {
        HW_RANGE_ID_10R, HW_RANGE_ID_100R, HW_RANGE_ID_1K, HW_RANGE_ID_10K, HW_RANGE_ID_100K, HW_RANGE_ID_1M,
    };
    for (uint32_t i = 0u; i < 6u; i++)
    {
        failures += expect_true(hw_excitation_validate_amplitude(ranges[i], HW_EXCITATION_AMP_100MVRMS) ==
                                    BSP_STATUS_OK,
                                "100m valid on all ranges");
    }
    failures += expect_true(hw_excitation_validate_amplitude(HW_RANGE_ID_10R, HW_EXCITATION_AMP_500MVRMS) ==
                                BSP_STATUS_NOT_SUPPORTED,
                            "500m forbidden on 10R");
    for (uint32_t i = 1u; i < 6u; i++)
    {
        failures += expect_true(hw_excitation_validate_amplitude(ranges[i], HW_EXCITATION_AMP_500MVRMS) ==
                                    BSP_STATUS_OK,
                                "500m valid except 10R");
    }

    uint16_t ccr[HW_EXCITATION_LUT_POINTS];
    failures += expect_true(hw_excitation_fill_ccr_table(ccr, HW_EXCITATION_LUT_POINTS, HW_EXCITATION_AMP_100MVRMS) ==
                                BSP_STATUS_OK,
                            "fill 100m");
    failures += expect_u32(ccr[0], HW_EXCITATION_PWM_CENTER, "100m center");
    failures += expect_true(hw_excitation_fill_ccr_table(ccr, HW_EXCITATION_LUT_POINTS, HW_EXCITATION_AMP_500MVRMS) ==
                                BSP_STATUS_OK,
                            "fill 500m");
    failures += expect_u32(ccr[0], HW_EXCITATION_PWM_CENTER, "500m center");
    for (uint32_t i = 0u; i < HW_EXCITATION_LUT_POINTS; i++)
    {
        if (ccr[i] > HW_EXCITATION_PWM_ARR)
        {
            failures += expect_true(false, "CCR exceeds ARR");
            break;
        }
    }

    hw_excitation_t excitation;
    hw_excitation_init(&excitation);
    failures += expect_true(hw_excitation_mode(&excitation) == HW_EXCITATION_MODE_OFF, "boot OFF");

    hw_excitation_freq_t freq = HW_EXCITATION_FREQ_INVALID;
    hw_excitation_amp_t amp = HW_EXCITATION_AMP_INVALID;
    hw_range_id_t range = HW_RANGE_ID_INVALID;
    failures += expect_true(hw_excitation_parse_freq_token("1k", &freq) && (freq == HW_EXCITATION_FREQ_1KHZ),
                            "parse 1k");
    failures += expect_true(hw_excitation_parse_amp_token("100m", &amp) && (amp == HW_EXCITATION_AMP_100MVRMS),
                            "parse 100m");
    failures += expect_true(hw_excitation_parse_range_token("10k", &range) && (range == HW_RANGE_ID_10K),
                            "parse 10k range");

    return (failures == 0) ? 0 : 1;
}
