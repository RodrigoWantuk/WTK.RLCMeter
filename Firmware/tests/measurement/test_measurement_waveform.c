#include "app/app_waveform_store.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { ++failures; (void)fprintf(stderr, "line %d: %s\n", __LINE__, #c); } } while (0)
static uint16_t code(float v) { return (uint16_t)lroundf(v * (4095.0f / 3.3f)); }

int main(void)
{
    static app_waveform_store_t store;
    uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    const hw_excitation_freq_t freqs[] = {HW_EXCITATION_FREQ_10HZ, HW_EXCITATION_FREQ_1KHZ, HW_EXCITATION_FREQ_10KHZ};
    const float phases[] = {-1.2f, 0.0f, 0.9f};
    const measurement_adc_calibration_t adc = measurement_adc_calibration_ideal();
    for (unsigned f = 0u; f < 3u; ++f)
    for (unsigned path = 0u; path < 2u; ++path)
    for (unsigned phase = 0u; phase < 3u; ++phase)
    {
        hw_metrology_adc_profile_t profile;
        CHECK(hw_metrology_adc_profile(freqs[f], &profile) == BSP_STATUS_OK);
        const measurement_attempt_config_t condition = {.range_id = HW_RANGE_ID_1K,
            .frequency = freqs[f], .amplitude = HW_EXCITATION_AMP_100MVRMS};
        hw_metrology_block_t block = {.valid = true, .dma_complete = true, .sequence = 17u,
            .range_id = condition.range_id, .sample_count = 256u, .words_per_sample = 3u,
            .raw_words = raw, .sample_rate_hz = profile.sample_rate_hz,
            .samples_per_cycle = profile.samples_per_cycle, .cycles_per_block = profile.cycles_per_block,
            .excitation_frequency_hz = hw_excitation_frequency_hz(freqs[f]), .requested_amplitude_mvrms = 100u};
        for (unsigned i = 0u; i < 256u; ++i)
        {
            const float t = 6.283185307f * (float)i / (float)profile.samples_per_cycle;
            /* Distinct rank phases expose accidental cross-rank pairing. */
            raw[i * 3u] = hw_metrology_pack_word(code(1.65f + 0.7f * sinf(t)),
                code(1.72f + 0.015f * sinf(t + phases[phase])));
            raw[i * 3u + 1u] = hw_metrology_pack_word(code(1.65f + 0.4f * sinf(t + 0.35f)),
                code(1.72f + 0.09f * sinf(t + 0.35f + phases[phase])));
            raw[i * 3u + 2u] = hw_metrology_pack_word(code(1.65f), code(1.72f));
        }
        measurement_calibrated_result_t processed = {.result = {.status = MEASUREMENT_STATUS_OK,
            .selected_channel = (measurement_return_channel_t)path}, .provenance = {.set_sequence = 23u, .lead_sequence = 4u}};
        const measurement_dsp_config_t dsp = measurement_dsp_config_ideal(condition.range_id);
        CHECK(measurement_extract_phasors(&block, &adc, &dsp, &processed.result.phasors) == BSP_STATUS_OK);
        app_waveform_invalidate(&store);
        app_waveform_stage(&store, &block, &adc, &processed, &condition, 42u);
        CHECK(store.candidate_valid);
        const measurement_waveform_snapshot_t *s = &store.candidate;
        CHECK(s->triggered && s->phase_valid && !s->clipped);
        CHECK(fabsf(s->phase_rad - phases[phase]) < 0.015f);
        CHECK(fabsf(s->vmid_v[1] - 1.72f) < 0.001f);
        CHECK(fabsf(measurement_waveform_sample_at(s, 1.0f) - measurement_waveform_sample_at(s, 0.0f) -
                    2.0f * (float)profile.samples_per_cycle) < 0.001f);
        CHECK(measurement_waveform_sample_at(s, 0.0f) >= 0.0f && measurement_waveform_sample_at(s, 1.0f) <= 255.0f);
        for (unsigned i = 0u; i < 256u; ++i)
        {
            hw_metrology_sample_t sample;
            CHECK(hw_metrology_unpack_sample(raw, i, &sample) == BSP_STATUS_OK);
            CHECK(s->adc[0][i] == (path == 0u ? sample.vexc_1 : sample.vexc_2));
            CHECK(s->adc[1][i] == (path == 0u ? sample.ret_1x : sample.ret_hg));
        }
        measurement_session_result_t result = {.status = MEASUREMENT_AUTO_STATUS_FINAL_OK,
            .session_sequence = 42u, .primary_attempt = {.config = condition,
                .selected_channel = (measurement_return_channel_t)path,
                .calibration = processed.provenance}};
#if WTK_ENABLE_MEASUREMENT_TRIMMED
        result.trimmed.applied = true; result.trimmed.samples = 7u;
        app_waveform_publish(&store, &result);
        CHECK(store.published_valid && store.published.block_sequence == 17u);
        memset(raw, 0, sizeof(raw));
        CHECK(store.published.adc[0][0] > 1000u); /* No dangling acquisition buffer. */
        store.candidate = store.published; store.candidate_valid = true;
        ++result.primary_attempt.calibration.lead_sequence;
        app_waveform_publish(&store, &result);
        CHECK(!store.published_valid);
        store.candidate_valid = true; result.status = MEASUREMENT_AUTO_STATUS_CANCELED;
        app_waveform_publish(&store, &result); CHECK(!store.published_valid);
#endif
        app_waveform_invalidate(&store); CHECK(!store.published_valid && !store.candidate_valid);
        block.dma_complete = false;
        CHECK(!measurement_waveform_capture(&store.candidate, &block, &adc, &processed, &condition, 42u));
        block.dma_complete = true;
        for (unsigned i = 0u; i < 256u; ++i)
        {
            raw[3u * i] = hw_metrology_pack_word(3000u, 2048u);
            raw[3u * i + 1u] = raw[3u * i];
            raw[3u * i + 2u] = hw_metrology_pack_word(2048u, 2048u);
        }
        CHECK(measurement_waveform_capture(&store.candidate, &block, &adc, &processed, &condition, 42u));
        CHECK(!store.candidate.triggered && !store.candidate.phase_valid);
        raw[path] = hw_metrology_pack_word(4095u, 0u);
        CHECK(measurement_waveform_capture(&store.candidate, &block, &adc, &processed, &condition, 42u));
        CHECK(store.candidate.clipped && !store.candidate.phase_valid);
    }
    CHECK(measurement_waveform_division(0.1f) == 1.0f);
    CHECK(measurement_waveform_division(4.0f) == 2.0f);
    CHECK(measurement_waveform_division(10.0f) == 5.0f);
    CHECK(measurement_waveform_division(25.0f) == 10.0f);
    CHECK(measurement_waveform_division(700.0f) == 500.0f);
    (void)printf("waveform tests: %d failures; store=%u bytes\n", failures, (unsigned)sizeof(store));
    return failures == 0 ? 0 : 1;
}
