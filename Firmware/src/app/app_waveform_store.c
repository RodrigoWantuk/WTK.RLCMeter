#include "app/app_waveform_store.h"
#include <stddef.h>

void app_waveform_invalidate(app_waveform_store_t *s)
{
    if (s == NULL) return;
    s->candidate_valid = false; s->published_valid = false; ++s->generation;
}

void app_waveform_stage(app_waveform_store_t *s, const hw_metrology_block_t *b,
    const measurement_adc_calibration_t *adc, const measurement_calibrated_result_t *p,
    const measurement_attempt_config_t *condition, uint32_t session)
{
    if (s != NULL) s->candidate_valid = measurement_waveform_capture(&s->candidate, b, adc, p, condition, session);
}

void app_waveform_publish(app_waveform_store_t *s, const measurement_session_result_t *r)
{
    if (s == NULL) return;
    s->published_valid = false; ++s->generation;
#if WTK_ENABLE_MEASUREMENT_TRIMMED
    if (s->candidate_valid && r != NULL && r->trimmed.applied &&
        r->trimmed.samples == MEASUREMENT_TRIMMED_SAMPLES && measurement_waveform_matches(&s->candidate, r))
    { s->published = s->candidate; s->published_valid = true; }
#else
    (void)r;
#endif
    s->candidate_valid = false;
}
