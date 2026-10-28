#include "measurement/measurement_waveform.h"

#include <math.h>
#include <stddef.h>

float measurement_waveform_mv(const measurement_waveform_snapshot_t *s, unsigned ch, unsigned i)
{
    if (s == NULL || ch > 1u || i >= HW_METROLOGY_SAMPLES_PER_BLOCK) return 0.0f;
    return 1000.0f * ((float)s->adc[ch][i] * s->scale[ch].code_to_volts +
                     s->scale[ch].offset_volts - s->vmid_v[ch]);
}

float measurement_waveform_division(float peak_mv)
{
    const float needed = fabsf(peak_mv) * (1.1f / 3.0f);
    float decade = 1.0f;
    if (!isfinite(needed)) return 1.0f;
    while (decade * 5.0f < needed && decade < 1000000.0f) decade *= 10.0f;
    if (needed <= decade) return decade;
    if (needed <= 2.0f * decade) return 2.0f * decade;
    return 5.0f * decade;
}

float measurement_waveform_sample_at(const measurement_waveform_snapshot_t *s, float fraction)
{
    const float period = (float)s->sample_rate_hz / (float)s->frequency_hz;
    return s->trigger_sample + (2.0f * fraction - 1.0f) * period;
}

bool measurement_waveform_capture(measurement_waveform_snapshot_t *s,
    const hw_metrology_block_t *b, const measurement_adc_calibration_t *adc,
    const measurement_calibrated_result_t *p, const measurement_attempt_config_t *a,
    uint32_t session)
{
    if (s == NULL || b == NULL || adc == NULL || p == NULL || a == NULL ||
        !b->valid || !b->dma_complete || b->dma_error || b->timeout || b->raw_words == NULL ||
        b->sample_count != HW_METROLOGY_SAMPLES_PER_BLOCK || b->words_per_sample != 3u ||
        b->range_id != a->range_id || b->sample_rate_hz == 0u ||
        (b->samples_per_cycle != 16u && b->samples_per_cycle != 64u) ||
        b->sample_rate_hz != b->excitation_frequency_hz * b->samples_per_cycle ||
        b->excitation_frequency_hz != hw_excitation_frequency_hz(a->frequency) ||
        b->requested_amplitude_mvrms != hw_excitation_amplitude_mvrms(a->amplitude) ||
        p->result.status != MEASUREMENT_STATUS_OK || !p->result.phasors.valid ||
        (p->result.selected_channel != MEASUREMENT_RETURN_1X &&
         p->result.selected_channel != MEASUREMENT_RETURN_HG)) return false;
    const bool hg = p->result.selected_channel == MEASUREMENT_RETURN_HG;
    s->scale[0] = hg ? adc->vexc_2 : adc->vexc_1;
    s->scale[1] = hg ? adc->ret_hg : adc->ret_1x;
    s->vmid_v[0] = 0.0f; s->vmid_v[1] = 0.0f;
    s->clipped = false;
    for (unsigned i = 0u; i < HW_METROLOGY_SAMPLES_PER_BLOCK; ++i)
    {
        hw_metrology_sample_t v;
        if (hw_metrology_unpack_sample(b->raw_words, i, &v) != BSP_STATUS_OK) return false;
        s->adc[0][i] = hg ? v.vexc_2 : v.vexc_1;
        s->adc[1][i] = hg ? v.ret_hg : v.ret_1x;
        s->vmid_v[0] += (float)v.vmid_adc1 * adc->vmid_adc1.code_to_volts + adc->vmid_adc1.offset_volts;
        s->vmid_v[1] += (float)v.vmid_adc2 * adc->vmid_adc2.code_to_volts + adc->vmid_adc2.offset_volts;
        s->clipped = s->clipped || hw_metrology_raw_is_hard_clipped(s->adc[0][i]) ||
                                  hw_metrology_raw_is_hard_clipped(s->adc[1][i]);
    }
    s->vmid_v[0] /= 256.0f; s->vmid_v[1] /= 256.0f;
    for (unsigned ch = 0u; ch < 2u; ++ch)
    {
        if (!isfinite(s->scale[ch].code_to_volts) || s->scale[ch].code_to_volts <= 0.0f ||
            !isfinite(s->scale[ch].offset_volts) || !isfinite(s->vmid_v[ch])) return false;
        /* This is an ADC-plane visualization, not an extra measurement gate.
         * Reject impossible/nonfinite display coordinates before any integer
         * raster conversion. +/-5 V also bounds both signed mV axes to 4 digits. */
        for (unsigned i = 0u; i < 256u; ++i)
        {
            const float mv = measurement_waveform_mv(s, ch, i);
            if (!isfinite(mv) || fabsf(mv) > 5000.0f) return false;
        }
    }
    s->session_sequence = session; s->block_sequence = b->sequence;
    s->sample_rate_hz = b->sample_rate_hz;
    s->frequency_hz = (uint16_t)b->excitation_frequency_hz;
    s->range_id = (uint8_t)a->range_id; s->amplitude = (uint8_t)a->amplitude;
    s->channel = (uint8_t)p->result.selected_channel;
    s->calibration_sequence = p->provenance.set_sequence;
    s->lead_sequence = p->provenance.lead_sequence;
    const float period = (float)b->sample_rate_hz / (float)b->excitation_frequency_hz;
    if (!isfinite(period) || period < 2.0f || period > 127.0f) return false;
    s->trigger_sample = 127.5f; s->triggered = false;
    float best = 256.0f;
    for (unsigned i = 1u; i < 256u; ++i)
    {
        const float left = measurement_waveform_mv(s, 0u, i - 1u);
        const float right = measurement_waveform_mv(s, 0u, i);
        if (left <= 0.0f && right > 0.0f)
        {
            const float crossing = (float)(i - 1u) - left / (right - left);
            const float distance = fabsf(crossing - 127.5f);
            if (crossing >= period && crossing + period <= 255.0f && distance < best)
            { s->trigger_sample = crossing; s->triggered = true; best = distance; }
        }
    }
    const measurement_phasor_set_t *ph = &p->result.phasors;
    const measurement_complex_t exc = hg ? ph->vexc_2 : ph->vexc_1;
    const measurement_complex_t ret = hg ? ph->ret_hg : ph->ret_1x;
    s->phase_rad = measurement_complex_phase_rad(ret) - measurement_complex_phase_rad(exc);
    if (s->phase_rad > 3.141592654f) s->phase_rad -= 6.283185307f;
    if (s->phase_rad < -3.141592654f) s->phase_rad += 6.283185307f;
    s->phase_valid = s->triggered && !s->clipped && isfinite(s->phase_rad) &&
        measurement_complex_mag(exc) >= 0.002f && measurement_complex_mag(ret) >= 0.002f;
    return true;
}

bool measurement_waveform_matches(const measurement_waveform_snapshot_t *s,
                                  const measurement_session_result_t *r)
{
    if (s == NULL || r == NULL || r->status != MEASUREMENT_AUTO_STATUS_FINAL_OK) return false;
    const measurement_attempt_result_t *a = &r->primary_attempt;
    return s->session_sequence == r->session_sequence &&
        s->frequency_hz == hw_excitation_frequency_hz(a->config.frequency) &&
        s->range_id == (uint8_t)a->config.range_id && s->amplitude == (uint8_t)a->config.amplitude &&
        s->channel == (uint8_t)a->selected_channel &&
        s->calibration_sequence == a->calibration.set_sequence &&
        s->lead_sequence == a->calibration.lead_sequence;
}
