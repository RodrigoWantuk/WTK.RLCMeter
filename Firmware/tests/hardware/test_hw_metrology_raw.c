#include "hardware/hw_metrology_raw.h"

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
    hw_metrology_adc_profile_t profile;

    failures += expect_true(hw_metrology_adc_profile(HW_EXCITATION_FREQ_100HZ, &profile) == BSP_STATUS_OK, "100 Hz ADC");
    failures += expect_u32(profile.sample_rate_hz, 6400u, "100 Hz SPS");
    failures += expect_u32(profile.tim2_arr, 11249u, "100 Hz ARR");
    failures += expect_u32(profile.tim2_ccr2, 5625u, "100 Hz CCR2");
    failures += expect_u32(profile.samples_per_cycle, 64u, "100 Hz spc");
    failures += expect_u32(profile.cycles_per_block, 4u, "100 Hz cycles");

    failures += expect_true(hw_metrology_adc_profile(HW_EXCITATION_FREQ_1KHZ, &profile) == BSP_STATUS_OK, "1 kHz ADC");
    failures += expect_u32(profile.sample_rate_hz, 64000u, "1 kHz SPS");
    failures += expect_u32(profile.tim2_arr, 1124u, "1 kHz ARR");
    failures += expect_u32(profile.tim2_ccr2, 562u, "1 kHz CCR2");
    failures += expect_u32(profile.samples_per_cycle, 64u, "1 kHz spc");
    failures += expect_u32(profile.cycles_per_block, 4u, "1 kHz cycles");

    failures += expect_true(hw_metrology_adc_profile(HW_EXCITATION_FREQ_10KHZ, &profile) == BSP_STATUS_OK, "10 kHz ADC");
    failures += expect_u32(profile.sample_rate_hz, 160000u, "10 kHz SPS");
    failures += expect_u32(profile.tim2_arr, 449u, "10 kHz ARR");
    failures += expect_u32(profile.tim2_ccr2, 225u, "10 kHz CCR2");
    failures += expect_u32(profile.samples_per_cycle, 16u, "10 kHz spc");
    failures += expect_u32(profile.cycles_per_block, 16u, "10 kHz cycles");

    failures += expect_u32(HW_METROLOGY_SAMPLES_PER_BLOCK, 256u, "samples");
    failures += expect_u32(HW_METROLOGY_WORDS_PER_SAMPLE, 3u, "words/sample");
    failures += expect_u32(HW_METROLOGY_RAW_WORD_COUNT, 768u, "raw words");
    failures += expect_u32(HW_METROLOGY_RAW_BUFFER_BYTES, 3072u, "raw bytes");

    uint32_t words[HW_METROLOGY_RAW_WORD_COUNT];
    for (uint32_t n = 0u; n < HW_METROLOGY_SAMPLES_PER_BLOCK; n++)
    {
        const uint16_t base = (uint16_t)(100u + n);
        words[(3u * n) + 0u] = hw_metrology_pack_word((uint16_t)(base + 0u), (uint16_t)(base + 10u));
        words[(3u * n) + 1u] = hw_metrology_pack_word((uint16_t)(base + 1u), (uint16_t)(base + 11u));
        words[(3u * n) + 2u] = hw_metrology_pack_word((uint16_t)(base + 2u), (uint16_t)(base + 12u));
    }

    const uint32_t indexes[] = {0u, 1u, 17u, 255u};
    for (uint32_t i = 0u; i < 4u; i++)
    {
        const uint32_t n = indexes[i];
        hw_metrology_sample_t sample;
        failures += expect_true(hw_metrology_unpack_sample(words, n, &sample) == BSP_STATUS_OK, "unpack");
        const uint16_t base = (uint16_t)(100u + n);
        failures += expect_u32(sample.vexc_1, (uint32_t)(base + 0u) & 0x0FFFu, "vexc1");
        failures += expect_u32(sample.ret_1x, (uint32_t)(base + 10u) & 0x0FFFu, "ret1x");
        failures += expect_u32(sample.vexc_2, (uint32_t)(base + 1u) & 0x0FFFu, "vexc2");
        failures += expect_u32(sample.ret_hg, (uint32_t)(base + 11u) & 0x0FFFu, "rethg");
        failures += expect_u32(sample.vmid_adc1, (uint32_t)(base + 2u) & 0x0FFFu, "vmid1");
        failures += expect_u32(sample.vmid_adc2, (uint32_t)(base + 12u) & 0x0FFFu, "vmid2");
    }

    uint16_t adc1 = 0u;
    uint16_t adc2 = 0u;
    hw_metrology_unpack_word(0xABCDE123u, &adc1, &adc2);
    failures += expect_u32(adc1, 0x0123u, "mask low 12");
    failures += expect_u32(adc2, 0x0BCDu, "mask high 12");

    failures += expect_true(hw_metrology_raw_is_hard_clipped(0u), "raw 0 clipped");
    failures += expect_true(hw_metrology_raw_is_hard_clipped(16u), "raw 16 clipped");
    failures += expect_true(!hw_metrology_raw_is_hard_clipped(17u), "raw 17 not clipped");
    failures += expect_true(!hw_metrology_raw_is_hard_clipped(2048u), "raw 2048 not clipped");
    failures += expect_true(!hw_metrology_raw_is_hard_clipped(4078u), "raw 4078 not clipped");
    failures += expect_true(hw_metrology_raw_is_hard_clipped(4079u), "raw 4079 clipped");
    failures += expect_true(hw_metrology_raw_is_hard_clipped(4095u), "raw 4095 clipped");

    words[0] = hw_metrology_pack_word(0u, 2048u);
    words[1] = hw_metrology_pack_word(17u, 4079u);
    words[2] = hw_metrology_pack_word(2048u, 16u);
    hw_metrology_block_t block = {0};
    hw_metrology_analyze_block(words, 1u, &block);
    failures += expect_true(block.streams[HW_METROLOGY_STREAM_VEXC_1].hard_clipped, "vexc1 clip");
    failures += expect_u32(block.streams[HW_METROLOGY_STREAM_VEXC_1].min_raw, 0u, "vexc1 min");
    failures += expect_u32(block.streams[HW_METROLOGY_STREAM_RET_1X].max_raw, 2048u, "ret1x max");
    failures += expect_true(block.streams[HW_METROLOGY_STREAM_RET_HG].hard_clipped, "rethg clip");
    failures += expect_true(!block.streams[HW_METROLOGY_STREAM_VEXC_2].hard_clipped, "vexc2 not clip");
    failures += expect_true(block.clipped, "block clipped");

    return (failures == 0) ? 0 : 1;
}
