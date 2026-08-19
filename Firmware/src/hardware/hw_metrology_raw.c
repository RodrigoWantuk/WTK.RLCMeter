#include "hardware/hw_metrology_raw.h"

#include <stddef.h>

#include "hardware/hw_metrology_clock.h"

uint32_t hw_metrology_pack_word(uint16_t adc1_raw, uint16_t adc2_raw)
{
    return ((uint32_t)adc1_raw & 0xFFFFu) | (((uint32_t)adc2_raw & 0xFFFFu) << 16u);
}

void hw_metrology_unpack_word(uint32_t word, uint16_t *adc1_raw, uint16_t *adc2_raw)
{
    if (adc1_raw != NULL)
    {
        *adc1_raw = (uint16_t)(word & HW_METROLOGY_ADC_DATA_MASK);
    }
    if (adc2_raw != NULL)
    {
        *adc2_raw = (uint16_t)((word >> 16u) & HW_METROLOGY_ADC_DATA_MASK);
    }
}

bsp_status_t hw_metrology_unpack_sample(const uint32_t *raw_words,
                                        uint32_t sample_index,
                                        hw_metrology_sample_t *sample)
{
    if ((raw_words == NULL) || (sample == NULL) || (sample_index >= HW_METROLOGY_SAMPLES_PER_BLOCK))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    const uint32_t base = sample_index * HW_METROLOGY_WORDS_PER_SAMPLE;
    hw_metrology_unpack_word(raw_words[base + 0u], &sample->vexc_1, &sample->ret_1x);
    hw_metrology_unpack_word(raw_words[base + 1u], &sample->vexc_2, &sample->ret_hg);
    hw_metrology_unpack_word(raw_words[base + 2u], &sample->vmid_adc1, &sample->vmid_adc2);
    return BSP_STATUS_OK;
}

bool hw_metrology_raw_is_hard_clipped(uint16_t raw)
{
    return hw_residual_raw_is_saturated(raw);
}

bsp_status_t hw_metrology_adc_profile(hw_excitation_freq_t frequency,
                                      hw_metrology_adc_profile_t *profile)
{
    if (profile == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    switch (frequency)
    {
    case HW_EXCITATION_FREQ_100HZ:
        profile->sample_rate_hz = 6400u;
        profile->tim2_arr = 11249u;
        profile->tim2_ccr2 = 5625u;
        profile->samples_per_cycle = 64u;
        profile->cycles_per_block = 4u;
        profile->capture_us = 40000u;
        return BSP_STATUS_OK;
    case HW_EXCITATION_FREQ_1KHZ:
        profile->sample_rate_hz = 64000u;
        profile->tim2_arr = 1124u;
        profile->tim2_ccr2 = 562u;
        profile->samples_per_cycle = 64u;
        profile->cycles_per_block = 4u;
        profile->capture_us = 4000u;
        return BSP_STATUS_OK;
    case HW_EXCITATION_FREQ_10KHZ:
        profile->sample_rate_hz = 160000u;
        profile->tim2_arr = 449u;
        profile->tim2_ccr2 = 225u;
        profile->samples_per_cycle = 16u;
        profile->cycles_per_block = 16u;
        profile->capture_us = 1600u;
        return BSP_STATUS_OK;
    case HW_EXCITATION_FREQ_INVALID:
    default:
        return BSP_STATUS_INVALID_ARG;
    }
}

static void update_stream(hw_metrology_stream_stats_t *stats, uint16_t raw, bool *block_clipped)
{
    if (raw < stats->min_raw)
    {
        stats->min_raw = raw;
    }
    if (raw > stats->max_raw)
    {
        stats->max_raw = raw;
    }
    if (hw_metrology_raw_is_hard_clipped(raw))
    {
        stats->hard_clipped = true;
        *block_clipped = true;
    }
}

void hw_metrology_analyze_block(const uint32_t *raw_words,
                                uint32_t sample_count,
                                hw_metrology_block_t *block)
{
    if (block == NULL)
    {
        return;
    }

    for (uint32_t i = 0u; i < (uint32_t)HW_METROLOGY_STREAM_COUNT; i++)
    {
        block->streams[i].min_raw = 0xFFFFu;
        block->streams[i].max_raw = 0u;
        block->streams[i].hard_clipped = false;
    }
    block->clipped = false;

    if ((raw_words == NULL) || (sample_count == 0u) || (sample_count > HW_METROLOGY_SAMPLES_PER_BLOCK))
    {
        return;
    }

    for (uint32_t n = 0u; n < sample_count; n++)
    {
        hw_metrology_sample_t sample;
        if (hw_metrology_unpack_sample(raw_words, n, &sample) != BSP_STATUS_OK)
        {
            return;
        }
        update_stream(&block->streams[HW_METROLOGY_STREAM_VEXC_1], sample.vexc_1, &block->clipped);
        update_stream(&block->streams[HW_METROLOGY_STREAM_RET_1X], sample.ret_1x, &block->clipped);
        update_stream(&block->streams[HW_METROLOGY_STREAM_VEXC_2], sample.vexc_2, &block->clipped);
        update_stream(&block->streams[HW_METROLOGY_STREAM_RET_HG], sample.ret_hg, &block->clipped);
        update_stream(&block->streams[HW_METROLOGY_STREAM_VMID_ADC1], sample.vmid_adc1, &block->clipped);
        update_stream(&block->streams[HW_METROLOGY_STREAM_VMID_ADC2], sample.vmid_adc2, &block->clipped);
    }
}
