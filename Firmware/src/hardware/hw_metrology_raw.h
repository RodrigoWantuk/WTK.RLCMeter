#ifndef WTK_HW_METROLOGY_RAW_H
#define WTK_HW_METROLOGY_RAW_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "hardware/hw_excitation.h"
#include "hardware/hw_range.h"
#include "hardware/hw_residual.h"
#include "hardware/hw_safety.h"

enum
{
    HW_METROLOGY_SAMPLES_PER_BLOCK = 256u,
    HW_METROLOGY_WORDS_PER_SAMPLE = 3u,
    HW_METROLOGY_RAW_WORD_COUNT = 768u,
    HW_METROLOGY_RAW_BUFFER_BYTES = 3072u,
    HW_METROLOGY_ADC_SAMPLE_TIME_CYCLES_X2 = 15u,
    HW_METROLOGY_ADC_SAMPLE_TIME_CYCLES = 75u / 10u,
    HW_METROLOGY_CLIP_LOW_MAX = HW_RESIDUAL_ADC_SATURATED_LOW_MAX,
    HW_METROLOGY_CLIP_HIGH_MIN = HW_RESIDUAL_ADC_SATURATED_HIGH_MIN,
    HW_METROLOGY_ADC_DATA_MASK = 0x0FFFu,
};

_Static_assert(HW_METROLOGY_SAMPLES_PER_BLOCK == 256u, "frozen block length");
_Static_assert(HW_METROLOGY_WORDS_PER_SAMPLE == 3u, "frozen packed words per sample");
_Static_assert(HW_METROLOGY_RAW_WORD_COUNT ==
                   (HW_METROLOGY_SAMPLES_PER_BLOCK * HW_METROLOGY_WORDS_PER_SAMPLE),
               "frozen raw word count");
_Static_assert(HW_METROLOGY_RAW_BUFFER_BYTES == (HW_METROLOGY_RAW_WORD_COUNT * 4u),
               "frozen raw buffer bytes");
_Static_assert(HW_METROLOGY_CLIP_LOW_MAX == 16u, "frozen low rail");
_Static_assert(HW_METROLOGY_CLIP_HIGH_MIN == 4079u, "frozen high rail");

typedef enum
{
    HW_METROLOGY_STREAM_VEXC_1 = 0,
    HW_METROLOGY_STREAM_RET_1X,
    HW_METROLOGY_STREAM_VEXC_2,
    HW_METROLOGY_STREAM_RET_HG,
    HW_METROLOGY_STREAM_VMID_ADC1,
    HW_METROLOGY_STREAM_VMID_ADC2,
    HW_METROLOGY_STREAM_COUNT,
} hw_metrology_stream_t;

typedef struct
{
    uint32_t sample_rate_hz;
    uint16_t tim2_arr;
    uint16_t tim2_ccr2;
    uint16_t samples_per_cycle;
    uint16_t cycles_per_block;
    uint32_t capture_us;
} hw_metrology_adc_profile_t;

typedef struct
{
    uint16_t vexc_1;
    uint16_t ret_1x;
    uint16_t vexc_2;
    uint16_t ret_hg;
    uint16_t vmid_adc1;
    uint16_t vmid_adc2;
} hw_metrology_sample_t;

typedef struct
{
    uint16_t min_raw;
    uint16_t max_raw;
    bool hard_clipped;
} hw_metrology_stream_stats_t;

typedef struct
{
    bool valid;
    uint32_t sequence;
    uint32_t excitation_frequency_hz;
    uint16_t requested_amplitude_mvrms;
    hw_range_id_t range_id;
    hw_charger_state_t charger;
    uint32_t adc_clock_hz;
    uint32_t sample_time_cycles_x2;
    uint32_t sample_rate_hz;
    uint16_t samples_per_cycle;
    uint16_t cycles_per_block;
    uint16_t sample_count;
    uint16_t words_per_sample;
    const uint32_t *raw_words;
    bool dma_complete;
    bool dma_error;
    bool timeout;
    bool clipped;
    hw_metrology_stream_stats_t streams[HW_METROLOGY_STREAM_COUNT];
} hw_metrology_block_t;

uint32_t hw_metrology_pack_word(uint16_t adc1_raw, uint16_t adc2_raw);
void hw_metrology_unpack_word(uint32_t word, uint16_t *adc1_raw, uint16_t *adc2_raw);
bsp_status_t hw_metrology_unpack_sample(const uint32_t *raw_words,
                                        uint32_t sample_index,
                                        hw_metrology_sample_t *sample);
bool hw_metrology_raw_is_hard_clipped(uint16_t raw);
bsp_status_t hw_metrology_adc_profile(hw_excitation_freq_t frequency,
                                      hw_metrology_adc_profile_t *profile);
void hw_metrology_analyze_block(const uint32_t *raw_words,
                                uint32_t sample_count,
                                hw_metrology_block_t *block);

#endif
