#ifndef WTK_APP_CALIBRATION_SESSION_H
#define WTK_APP_CALIBRATION_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "app/app_calibration_service.h"
#include "bsp/bsp_clock.h"
#include "bsp/bsp_status.h"
#include "hardware/hw_metrology_measure.h"
#include "wtk_build_config.h"
#if WTK_ENABLE_CAL_INTEGRATION
#include "app/app_cal_integration.h"
#endif

typedef enum
{
    APP_CAL_SESSION_IDLE = 0,
    APP_CAL_SESSION_START_CAPTURE,
    APP_CAL_SESSION_WAIT_CAPTURE,
    APP_CAL_SESSION_CANCELING,
    APP_CAL_SESSION_DONE,
} app_cal_session_state_t;

typedef enum
{
    APP_CAL_SESSION_EVENT_NONE = 0,
    APP_CAL_SESSION_EVENT_BEGIN,
    APP_CAL_SESSION_EVENT_CAPTURE_BEGIN,
    APP_CAL_SESSION_EVENT_CAPTURE_ACCEPTED,
    APP_CAL_SESSION_EVENT_CAPTURE_REJECTED,
    APP_CAL_SESSION_EVENT_COMPLETE,
    APP_CAL_SESSION_EVENT_FAILED,
    APP_CAL_SESSION_EVENT_CANCELED,
    APP_CAL_SESSION_EVENT_ERROR,
} app_cal_session_event_t;

typedef struct
{
    bsp_status_t (*start_capture)(const hw_metrology_measure_request_t *request,
                                  uint32_t now_ms,
                                  void *user);
    bsp_status_t (*step_capture)(uint32_t now_ms, void *user);
    /* True until all hardware/ADC/workspace teardown is complete, including abort. */
    bool (*capture_active)(void *user);
    bool (*capture_done)(void *user);
    bool (*capture_dumpable)(void *user);
    const hw_metrology_block_t *(*capture_block)(void *user);
    hw_metrology_measure_error_t (*capture_error)(void *user);
    void (*capture_acknowledge)(void *user);
    bsp_status_t (*capture_abort)(void *user);
    void *user;
} app_cal_session_io_t;

typedef struct
{
    const app_cal_session_io_t *io;
    app_calibration_service_t *service;
    const bsp_clock_summary_t *clock_summary;
    bsp_status_t clock_status;
    app_cal_session_state_t state;
    app_cal_session_event_t pending_event;
    app_cal_capture_sample_t last_sample;
    bsp_status_t last_sample_status;
    uint32_t last_reject_flags;
    bool started_capture;
#if WTK_ENABLE_CAL_INTEGRATION
    app_cal_integration_t integration;
    uint32_t integration_last_clip_mask;
    uint16_t integration_blocks;
    uint16_t integration_attempts;
#endif
} app_calibration_session_t;

#if WTK_ENABLE_CAL_INTEGRATION
/* Idle-only configuration. One preserves the original six-block workflow. */
bsp_status_t app_calibration_session_set_integration(app_calibration_session_t *session,
                                                     uint16_t blocks_per_group);
#endif

bsp_status_t app_calibration_session_init(app_calibration_session_t *session,
                                          app_calibration_service_t *service,
                                          const app_cal_session_io_t *io);
bsp_status_t app_calibration_session_start(app_calibration_session_t *session,
                                           const app_cal_workflow_request_t *request,
                                           const bsp_clock_summary_t *clock_summary,
                                           bsp_status_t clock_status,
                                           uint32_t now_ms);
app_cal_session_event_t app_calibration_session_step(app_calibration_session_t *session,
                                                     uint32_t now_ms);
bsp_status_t app_calibration_session_cancel(app_calibration_session_t *session);

bool app_calibration_session_active(const app_calibration_session_t *session);
app_cal_session_state_t app_calibration_session_state(const app_calibration_session_t *session);
const app_cal_evidence_t *app_calibration_session_evidence(const app_calibration_session_t *session);
uint32_t app_calibration_session_last_reject_flags(const app_calibration_session_t *session);
bsp_status_t app_calibration_session_last_sample_status(const app_calibration_session_t *session);
uint32_t app_calibration_session_context_size_bytes(void);
const char *app_cal_session_event_string(app_cal_session_event_t event);
const char *app_cal_session_state_string(app_cal_session_state_t state);

#endif
