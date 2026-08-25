#ifndef WTK_APP_CALIBRATION_WORKFLOW_H
#define WTK_APP_CALIBRATION_WORKFLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "hardware/hw_metrology_raw.h"
#include "measurement/measurement_calibration.h"
#include "measurement/measurement_dsp.h"

enum
{
    APP_CAL_WORKFLOW_REQUIRED_ACCEPTED = 6u,
    APP_CAL_WORKFLOW_MAX_ATTEMPTS = 10u,
    APP_CAL_WORKFLOW_STABILITY_LIMIT_PPM = 20000u,
    APP_CAL_WORKFLOW_SOURCE_MIN_UV_PEAK = 5000u,
    APP_CAL_WORKFLOW_DENOMINATOR_MIN_UV_PEAK = 1000u,
};

typedef enum
{
    APP_CAL_STANDARD_OPEN = 0,
    APP_CAL_STANDARD_SHORT,
    APP_CAL_STANDARD_LOAD,
} app_cal_standard_type_t;

typedef enum
{
    APP_CAL_WORKFLOW_IDLE = 0,
    APP_CAL_WORKFLOW_CAPTURE_REQUESTED,
    APP_CAL_WORKFLOW_WAIT_CAPTURE,
    APP_CAL_WORKFLOW_COMPLETE,
    APP_CAL_WORKFLOW_FAILED,
    APP_CAL_WORKFLOW_CANCELING,
    APP_CAL_WORKFLOW_CANCELED,
} app_cal_workflow_state_t;

typedef enum
{
    APP_CAL_WORKFLOW_RESULT_NONE = 0,
    APP_CAL_WORKFLOW_RESULT_OK,
    APP_CAL_WORKFLOW_RESULT_INVALID_REQUEST,
    APP_CAL_WORKFLOW_RESULT_UNSUPPORTED_CONDITION,
    APP_CAL_WORKFLOW_RESULT_UNSTABLE,
    APP_CAL_WORKFLOW_RESULT_TOO_MANY_REJECTS,
    APP_CAL_WORKFLOW_RESULT_PHASE05_ERROR,
    APP_CAL_WORKFLOW_RESULT_SAFETY_ABORT,
    APP_CAL_WORKFLOW_RESULT_CANCELED,
} app_cal_workflow_result_t;

typedef enum
{
    APP_CAL_REJECT_NONE = 0u,
    APP_CAL_REJECT_PHASE05 = 1u << 0,
    APP_CAL_REJECT_SAFETY_ABORT = 1u << 1,
    APP_CAL_REJECT_DSP = 1u << 2,
    APP_CAL_REJECT_CLIPPED = 1u << 3,
    APP_CAL_REJECT_NONFINITE = 1u << 4,
    APP_CAL_REJECT_SOURCE_TOO_SMALL = 1u << 5,
    APP_CAL_REJECT_NO_USABLE_CHANNEL = 1u << 6,
    APP_CAL_REJECT_DENOMINATOR_TOO_SMALL = 1u << 7,
} app_cal_reject_flags_t;

typedef struct
{
    app_cal_standard_type_t type;
    measurement_complex_t z_ohms;
    bool z_valid;
} app_cal_standard_t;

typedef struct
{
    measurement_cal_key_t key;
    app_cal_standard_t standard;
    int32_t temperature_mC;
    bool temperature_valid;
} app_cal_workflow_request_t;

typedef struct
{
    measurement_cal_key_t key;
    app_cal_standard_type_t standard_type;
    uint32_t timestamp_ms;
    int32_t temperature_mC;
    bool temperature_valid;
    measurement_complex_t source_v;
    measurement_complex_t vexc_1_v;
    measurement_complex_t vexc_2_v;
    measurement_complex_t ret_1x_v;
    measurement_complex_t ret_hg_raw_v;
    measurement_complex_t ret_hg_reconstructed_v;
    measurement_complex_t ret_hg_v;
    measurement_complex_t vmid_adc1_v;
    measurement_complex_t vmid_adc2_v;
    measurement_complex_t open_y_1x;
    measurement_complex_t open_y_hg;
    measurement_complex_t hg_observed_transfer;
    measurement_complex_t z_1x_ohms;
    measurement_complex_t z_hg_ohms;
    float source_peak_v;
    float vexc_1_peak_v;
    float vexc_2_peak_v;
    float ret_1x_peak_v;
    float ret_hg_raw_peak_v;
    float ret_hg_reconstructed_peak_v;
    float ret_hg_peak_v;
    float denominator_1x_peak_v;
    float denominator_hg_peak_v;
    bool ret_1x_clipped;
    bool ret_hg_clipped;
    bool ret_1x_usable;
    bool ret_hg_usable;
    bool open_y_1x_valid;
    bool open_y_hg_valid;
    bool hg_overlap_valid;
    bool z_1x_valid;
    bool z_hg_valid;
    bool clipped;
    uint32_t reject_flags;
} app_cal_capture_sample_t;

typedef struct
{
    uint8_t count;
    measurement_complex_t mean;
    float m2_re;
    float m2_im;
} app_cal_complex_stats_t;

typedef struct
{
    uint8_t sample_count;
    uint8_t usable_count;
    uint8_t rejected_count;
    bool stable;
    bool insufficient;
    bool evidence_valid;
} app_cal_path_evidence_t;

typedef struct
{
    uint8_t count;
    int32_t mean_mC;
    int32_t min_mC;
    int32_t max_mC;
    bool valid;
} app_cal_temperature_evidence_t;

typedef struct
{
    measurement_cal_key_t key;
    app_cal_standard_t standard;
    uint8_t accepted;
    uint8_t rejected;
    uint8_t attempts;
    uint32_t reject_flags;
    uint32_t sequence;
    int32_t last_temperature_mC;
    bool last_temperature_valid;
    bool stable;
    bool ret_1x_consistent;
    bool ret_hg_consistent;
    bool ret_1x_evidence_valid;
    bool ret_hg_evidence_valid;
    bool hg_overlap_valid;
    app_cal_path_evidence_t ret_1x_path;
    app_cal_path_evidence_t ret_hg_path;
    app_cal_temperature_evidence_t temperature;
    app_cal_complex_stats_t source;
    app_cal_complex_stats_t source_1;
    app_cal_complex_stats_t source_2;
    app_cal_complex_stats_t ret_1x;
    app_cal_complex_stats_t ret_hg_raw;
    app_cal_complex_stats_t ret_hg_reconstructed;
    app_cal_complex_stats_t ret_hg;
    app_cal_complex_stats_t open_y_1x;
    app_cal_complex_stats_t open_y_hg;
    app_cal_complex_stats_t hg_observed_transfer;
    app_cal_complex_stats_t z_1x;
    app_cal_complex_stats_t z_hg;
} app_cal_evidence_t;

typedef struct
{
    app_cal_workflow_state_t state;
    app_cal_workflow_result_t result;
    app_cal_workflow_request_t request;
    app_cal_evidence_t evidence;
    uint32_t sequence;
    uint32_t last_reject_flags;
    bool waiting_capture;
} app_calibration_workflow_t;

void app_calibration_workflow_init(app_calibration_workflow_t *workflow);
bsp_status_t app_calibration_workflow_start(app_calibration_workflow_t *workflow,
                                            const app_cal_workflow_request_t *request,
                                            uint32_t sequence);
bool app_calibration_workflow_active(const app_calibration_workflow_t *workflow);
bool app_calibration_workflow_capture_pending(const app_calibration_workflow_t *workflow);
bsp_status_t app_calibration_workflow_capture_request(const app_calibration_workflow_t *workflow,
                                                      measurement_cal_key_t *key);
bsp_status_t app_calibration_workflow_mark_capture_started(app_calibration_workflow_t *workflow);
bsp_status_t app_calibration_workflow_submit_sample(app_calibration_workflow_t *workflow,
                                                    const app_cal_capture_sample_t *sample);
bsp_status_t app_calibration_workflow_submit_failure(app_calibration_workflow_t *workflow,
                                                     uint32_t reject_flags);
bsp_status_t app_calibration_workflow_cancel(app_calibration_workflow_t *workflow);
void app_calibration_workflow_cancel_complete(app_calibration_workflow_t *workflow);

bsp_status_t app_calibration_workflow_sample_from_block(const hw_metrology_block_t *block,
                                                        const app_cal_workflow_request_t *request,
                                                        app_cal_capture_sample_t *sample);

app_cal_workflow_state_t app_calibration_workflow_state(const app_calibration_workflow_t *workflow);
app_cal_workflow_result_t app_calibration_workflow_result(const app_calibration_workflow_t *workflow);
const app_cal_evidence_t *app_calibration_workflow_evidence(const app_calibration_workflow_t *workflow);
uint32_t app_calibration_workflow_last_reject_flags(const app_calibration_workflow_t *workflow);
const char *app_cal_standard_type_string(app_cal_standard_type_t type);
const char *app_cal_workflow_state_string(app_cal_workflow_state_t state);
const char *app_cal_workflow_result_string(app_cal_workflow_result_t result);
uint32_t app_calibration_workflow_context_size_bytes(void);
uint32_t app_cal_evidence_size_bytes(void);

#endif
