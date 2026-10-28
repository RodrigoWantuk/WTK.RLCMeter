#ifndef WTK_APP_MEASUREMENT_SESSION_H
#define WTK_APP_MEASUREMENT_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_clock.h"
#include "bsp/bsp_status.h"
#include "hardware/hw_metrology_measure.h"
#include "measurement/measurement_dsp.h"
#include "measurement/measurement_engine.h"
#include "app/app_waveform_store.h"

typedef enum
{
    APP_MEASUREMENT_SESSION_IDLE = 0,
    APP_MEASUREMENT_SESSION_START_ATTEMPT,
    APP_MEASUREMENT_SESSION_WAIT_ATTEMPT,
    APP_MEASUREMENT_SESSION_PROCESS_DSP,
    APP_MEASUREMENT_SESSION_DONE,
    APP_MEASUREMENT_SESSION_CANCELING,
#if WTK_ENABLE_MEASURE_SESSION_HOLD
    APP_MEASUREMENT_SESSION_FINISHING,
#if WTK_ENABLE_FREQUENCY_SURVEY
    APP_MEASUREMENT_SESSION_DRAINING,
#endif
#endif
} app_measurement_session_state_t;

typedef enum
{
    APP_MEASUREMENT_EVENT_NONE = 0,
    APP_MEASUREMENT_EVENT_AUTO_BEGIN,
    APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN,
    APP_MEASUREMENT_EVENT_PARTIAL_RESULT,
    APP_MEASUREMENT_EVENT_FINAL_RESULT,
    APP_MEASUREMENT_EVENT_ERROR,
} app_measurement_event_t;

typedef struct
{
    bsp_status_t (*start_attempt)(const hw_metrology_measure_request_t *request,
                                  uint32_t now_ms,
                                  void *user);
    bsp_status_t (*step_attempt)(uint32_t now_ms, void *user);
    bool (*attempt_active)(void *user);
    bool (*attempt_done)(void *user);
    bool (*attempt_dumpable)(void *user);
    const hw_metrology_block_t *(*attempt_block)(void *user);
    hw_metrology_measure_error_t (*attempt_error)(void *user);
    void (*attempt_acknowledge)(void *user);
    bsp_status_t (*attempt_abort)(void *user);
#if WTK_ENABLE_MEASURE_SESSION_HOLD
    bsp_status_t (*finish_session)(void *user);
#if WTK_ENABLE_MEASUREMENT_TRIMMED
    bool trimmed_enabled;
#if WTK_ENABLE_FREQUENCY_SURVEY
    bool frequency_survey_enabled;
#if WTK_ENABLE_CONTINUOUS_SURVEY
    bool continuous_survey_enabled;
#endif
#endif
#endif
#endif
    bsp_status_t (*process_block)(const hw_metrology_block_t *block,
                                  const measurement_attempt_config_t *attempt,
                                  measurement_calibrated_result_t *result,
                                  void *user);
    app_waveform_store_t *waveform_store;
    const measurement_adc_calibration_t *(*waveform_adc)(void *user);
    void *user;
} app_measurement_session_io_t;

typedef struct
{
    const app_measurement_session_io_t *io;
    measurement_auto_session_t policy;
    measurement_attempt_config_t current_attempt;
    measurement_calibrated_result_t dsp_result;
    const bsp_clock_summary_t *clock_summary;
    bsp_status_t clock_status;
    app_measurement_session_state_t state;
    app_measurement_event_t pending_event;
    measurement_auto_event_t pending_policy_event;
    measurement_auto_mode_t mode;
    uint32_t session_sequence;
    bool have_partial;
    bool have_final;
    bool cancel_requested;
    bool attempt_started;
#if WTK_ENABLE_MEASUREMENT_TRIMMED
    measurement_trimmed_t trimmed;
    bool trimming;
#if WTK_ENABLE_FREQUENCY_SURVEY
    uint32_t survey_deadline_ms;
    uint8_t segment_captures;
#endif
#endif
} app_measurement_session_t;

bsp_status_t app_measurement_session_init(app_measurement_session_t *session,
                                          const app_measurement_session_io_t *io);
bsp_status_t app_measurement_session_start(app_measurement_session_t *session,
                                           measurement_auto_mode_t mode,
                                           uint32_t sequence,
                                           measurement_qualification_t qualification,
                                           const measurement_auto_hint_t *hint,
                                           const bsp_clock_summary_t *clock_summary,
                                           bsp_status_t clock_status,
                                           uint32_t now_ms);
app_measurement_event_t app_measurement_session_step(app_measurement_session_t *session,
                                                     uint32_t now_ms);
bsp_status_t app_measurement_session_cancel(app_measurement_session_t *session);

bool app_measurement_session_active(const app_measurement_session_t *session);
bool app_measurement_session_final_ready(const app_measurement_session_t *session);
bool app_measurement_session_partial_ready(const app_measurement_session_t *session);
const measurement_attempt_config_t *app_measurement_session_current_attempt(
    const app_measurement_session_t *session);
const measurement_session_result_t *app_measurement_session_partial(
    const app_measurement_session_t *session);
const measurement_session_result_t *app_measurement_session_final(
    const app_measurement_session_t *session);
app_measurement_session_state_t app_measurement_session_state(
    const app_measurement_session_t *session);

measurement_attempt_result_t app_measurement_attempt_from_dsp(
    const measurement_attempt_config_t *attempt,
    const measurement_calibrated_result_t *processed,
    bool phase05_failed,
    bool safety_abort,
    bool canceled);

const char *app_measurement_event_string(app_measurement_event_t event);
const char *app_measurement_session_state_string(app_measurement_session_state_t state);
uint32_t app_measurement_session_size_bytes(void);

#endif
