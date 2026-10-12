#include "hardware/hw_excitation.h"

#include <stdio.h>
#include <math.h>

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
    failures += expect_true(hw_excitation_freq_profile(HW_EXCITATION_FREQ_10HZ, &profile) == BSP_STATUS_OK,
                            "10 Hz profile");
    failures += expect_u32(profile.rcr, 249u, "10 Hz RCR");
    failures += expect_u32(profile.ccr_update_hz, 1800u, "10 Hz CCR update");
    failures += expect_u32(profile.sine_settle_ms, 800u, "10 Hz settle");

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


    const uint16_t original_tables[2][45] = {
        {80, 81, 82, 83, 84, 84, 85, 86, 86, 87, 87, 87, 87, 87, 86, 86, 85, 85, 84, 83, 82, 81, 80, 80, 79, 78, 77, 76, 75, 75, 74, 74, 73, 73, 73, 73, 73, 74, 74, 75, 76, 76, 77, 78, 79},
        {80, 85, 89, 94, 98, 102, 105, 108, 111, 113, 114, 114, 114, 113, 112, 110, 107, 104, 100, 96, 92, 87, 82, 78, 73, 68, 64, 60, 56, 53, 50, 48, 47, 46, 46, 46, 47, 49, 52, 55, 58, 62, 66, 71, 75},
    };
    uint16_t slow[HW_EXCITATION_LUT_POINTS_10HZ];
    for (uint32_t amp_id = 0u; amp_id < 2u; amp_id++)
    {
        const hw_excitation_amp_t amplitude = (hw_excitation_amp_t)amp_id;
        failures += expect_true(hw_excitation_fill_ccr_table(slow, 180u, amplitude) == BSP_STATUS_OK,
                                "10 Hz interpolated circular DMA table");
        (void)hw_excitation_fill_ccr_table(ccr, 45u, amplitude);
        for (uint32_t i = 0u; i < 45u; i++)
            failures += expect_true(ccr[i] == original_tables[amp_id][i], "1k/10k duty golden preserved bit-for-bit");
        float cosine = 0.0f, sine_fit = 0.0f, energy = 0.0f;
        int32_t total = 0;
        for (uint32_t i = 0u; i < 180u; i++)
        {
            const float radians = (float)i * 0.03490658504f;
            const int32_t delta = (int32_t)slow[i] - 80;
            total += delta;
            sine_fit += (float)delta * sinf(radians) / 90.0f;
            cosine += (float)delta * cosf(radians) / 90.0f;
            energy += (float)(delta * delta) / 180.0f;
            failures += expect_true(slow[i] <= 159u, "all duty values within physical period");
            if (i % 4u == 0u)
            {
                failures += expect_true(slow[i] == ccr[i / 4u], "45-point samples preserved exactly");
            }
            failures += expect_true(slow[i] + slow[(180u - i) % 180u] == 160u,
                                    "zero DC departure from neutral");
        }
        const float nominal = (float)hw_excitation_peak_q8(amplitude) / 256.0f;
        failures += expect_true(total == 0 && fabsf(cosine) < 0.01f &&
                                fabsf(sine_fit - nominal) < 0.25f,
                                "fundamental phase/amplitude bounded by duty quantization");
        (void)printf("10HZ DUTY SYNTHETIC amp=%u fundamental_ticks=%.6f rms_ticks=%.6f\n",
                     (unsigned)amp_id, (double)sine_fit, (double)sqrtf(energy));
    }
    for (hw_excitation_freq_t f = HW_EXCITATION_FREQ_10HZ; f <= HW_EXCITATION_FREQ_10KHZ;
         f = (hw_excitation_freq_t)((unsigned)f + 1u))
    {
        (void)hw_excitation_freq_profile(f, &profile);
        failures += expect_true(450000u / ((uint32_t)profile.rcr + 1u) == profile.ccr_update_hz &&
                                profile.ccr_update_hz / profile.table_points == profile.frequency_hz,
                                "integer carrier/repetition/DMA-length frequency identity");
    }
    failures += expect_true(hw_excitation_parse_freq_token("10", &freq) && freq == HW_EXCITATION_FREQ_10HZ &&
                            !hw_excitation_parse_freq_token("100", &freq), "retired token cannot alias 10 Hz");

    const char *const range_names[] = {"10r", "100r", "1k", "10k", "100k", "1m"};
    for (uint8_t i = 0u; i < 6u; i++)
        failures += expect_true(hw_excitation_parse_range_token(range_names[i], &range) &&
                                (unsigned)range == i, "shared parser preserves all range IDs");
    failures += expect_true(!hw_excitation_parse_range_token(NULL, &range) &&
                            !hw_excitation_parse_range_token("1kextra", &range) &&
                            !hw_excitation_parse_range_token("", &range) &&
                            !hw_excitation_parse_freq_token("100", &freq) &&
                            !hw_excitation_parse_amp_token("500mextra", &amp) &&
                            !hw_excitation_parse_amp_token("500m", NULL), "malformed tokens rejected");
    failures += expect_true(hw_excitation_validate_amplitude((hw_range_id_t)6, HW_EXCITATION_AMP_100MVRMS) ==
                                BSP_STATUS_INVALID_ARG &&
                            hw_excitation_validate_amplitude(HW_RANGE_ID_1K, (hw_excitation_amp_t)2) ==
                                BSP_STATUS_INVALID_ARG, "compacted validation preserves domain bounds");

    return (failures == 0) ? 0 : 1;
}
