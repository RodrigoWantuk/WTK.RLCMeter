#ifndef WTK_MEASUREMENT_WAVEFORM_H
#define WTK_MEASUREMENT_WAVEFORM_H

#include "measurement/measurement_engine.h"

/* Physical ADC plane: HG is deliberately not de-embedded. Immutable once published. */
typedef struct
{
    uint16_t adc[2][HW_METROLOGY_SAMPLES_PER_BLOCK];
    measurement_adc_scale_t scale[2];
    float vmid_v[2];
    float trigger_sample;
    float phase_rad;
    uint32_t session_sequence, block_sequence, sample_rate_hz;
    uint32_t calibration_sequence, lead_sequence;
    uint16_t frequency_hz;
    uint8_t range_id, amplitude, channel;
    bool triggered, phase_valid, clipped;
} measurement_waveform_snapshot_t;

bool measurement_waveform_capture(measurement_waveform_snapshot_t *out,
    const hw_metrology_block_t *block, const measurement_adc_calibration_t *adc,
    const measurement_calibrated_result_t *processed,
    const measurement_attempt_config_t *condition, uint32_t session);
float measurement_waveform_mv(const measurement_waveform_snapshot_t *snapshot,
                              unsigned channel, unsigned sample);
float measurement_waveform_division(float peak_mv);
/* Continuous sample coordinate; clipping to a display pixel is UI policy. */
float measurement_waveform_sample_at(const measurement_waveform_snapshot_t *snapshot, float fraction);
bool measurement_waveform_matches(const measurement_waveform_snapshot_t *snapshot,
                                  const measurement_session_result_t *result);

#endif
