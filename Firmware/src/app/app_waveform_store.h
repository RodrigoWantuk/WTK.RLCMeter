#ifndef WTK_APP_WAVEFORM_STORE_H
#define WTK_APP_WAVEFORM_STORE_H

#include "measurement/measurement_waveform.h"

/* Foreground-only ownership. UI must abandon a generation before its slot is reused. */
typedef struct
{
    measurement_waveform_snapshot_t candidate, published;
    uint32_t generation;
    bool candidate_valid, published_valid;
} app_waveform_store_t;

_Static_assert(sizeof(app_waveform_store_t) <= 2256u, "waveform store budget");
void app_waveform_invalidate(app_waveform_store_t *store);
void app_waveform_stage(app_waveform_store_t *store, const hw_metrology_block_t *block,
    const measurement_adc_calibration_t *adc, const measurement_calibrated_result_t *processed,
    const measurement_attempt_config_t *condition, uint32_t session);
void app_waveform_publish(app_waveform_store_t *store, const measurement_session_result_t *result);

#endif
