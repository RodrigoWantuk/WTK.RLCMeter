#ifndef WTK_APP_CALIBRATION_WIZARD_H
#define WTK_APP_CALIBRATION_WIZARD_H

#include <stdbool.h>
#include <stdint.h>

#include "app/app_calibration_campaign.h"
#include "app/app_calibration_session.h"
#include "hardware/hw_safety.h"

enum
{
    APP_CAL_WIZARD_RANGE_COUNT = 6u,
    APP_CAL_WIZARD_MAX_CONDITIONS_PER_RANGE = 6u,
};

typedef enum
{
    APP_CAL_WIZARD_MODE_MANDATORY = 0,
    APP_CAL_WIZARD_MODE_MANUAL,
} app_cal_wizard_mode_t;

typedef enum
{
    APP_CAL_WIZARD_IDLE = 0,
    APP_CAL_WIZARD_INTRO,
    APP_CAL_WIZARD_WAIT_OPEN_FIXTURE,
    APP_CAL_WIZARD_CAPTURE_OPEN,
    APP_CAL_WIZARD_WAIT_SHORT_FIXTURE,
    APP_CAL_WIZARD_CAPTURE_SHORT,
    APP_CAL_WIZARD_WAIT_LOAD_FIXTURE,
    APP_CAL_WIZARD_CAPTURE_LOAD,
    APP_CAL_WIZARD_RANGE_COMPLETE,
    APP_CAL_WIZARD_CONFIRM_SAVE,
    APP_CAL_WIZARD_COMMITTING,
    APP_CAL_WIZARD_COMPLETE,
    APP_CAL_WIZARD_FAILED,
    APP_CAL_WIZARD_SAFETY_BLOCKED,
    APP_CAL_WIZARD_CANCELING,
    APP_CAL_WIZARD_CANCELED,
} app_cal_wizard_state_t;

typedef enum
{
    APP_CAL_WIZARD_ERROR_NONE = 0,
    APP_CAL_WIZARD_ERROR_INVALID_ARG,
    APP_CAL_WIZARD_ERROR_STORAGE,
    APP_CAL_WIZARD_ERROR_CANDIDATE_BUSY,
    APP_CAL_WIZARD_ERROR_CONDITION,
    APP_CAL_WIZARD_ERROR_PHASE05,
    APP_CAL_WIZARD_ERROR_SOLVER,
    APP_CAL_WIZARD_ERROR_CANDIDATE_INCOMPLETE,
    APP_CAL_WIZARD_ERROR_COMMIT,
    APP_CAL_WIZARD_ERROR_CANCELED,
} app_cal_wizard_error_t;

typedef struct
{
    bsp_status_t (*load_z)(hw_range_id_t range_id,
                           hw_excitation_freq_t frequency,
                           measurement_complex_t *z_ohms,
                           void *user);
    void *user;
} app_cal_fixture_profile_t;

typedef struct
{
    app_cal_wizard_state_t state;
    app_cal_wizard_mode_t mode;
    app_cal_standard_type_t standard;
    app_cal_wizard_error_t error;
    app_cal_workflow_result_t workflow_result;
    measurement_cal_solver_status_t solver_status;
    hw_range_id_t range_id;
    uint8_t range_index;
    uint8_t range_count;
    uint8_t condition_index;
    uint8_t condition_count;
    uint8_t solved_count;
    uint8_t total_conditions;
    uint8_t accepted;
    uint8_t attempts;
    uint32_t reject_flags;
    int32_t open_temperature_mC;
    int32_t short_temperature_mC;
    int32_t load_temperature_mC;
    int32_t temperature_min_mC;
    int32_t temperature_max_mC;
    int32_t temperature_span_mC;
    hw_excitation_freq_t frequency;
    hw_excitation_amp_t amplitude;
    bool mandatory;
    bool temperature_span_valid;
} app_cal_wizard_snapshot_t;

typedef struct
{
    measurement_cal_key_t key;
    measurement_cal_solver_standard_t standard;
    bool valid;
} app_cal_wizard_cached_standard_t;

typedef struct
{
    app_calibration_session_t session;
    app_calibration_service_t *service;
    app_cal_fixture_profile_t fixture;
    app_cal_wizard_cached_standard_t open_cache[APP_CAL_WIZARD_MAX_CONDITIONS_PER_RANGE];
    app_cal_wizard_cached_standard_t short_cache[APP_CAL_WIZARD_MAX_CONDITIONS_PER_RANGE];
    app_cal_wizard_state_t state;
    app_cal_wizard_state_t state_before_safety;
    app_cal_wizard_mode_t mode;
    app_cal_standard_type_t standard;
    app_cal_wizard_error_t error;
    app_cal_workflow_result_t workflow_result;
    measurement_cal_solver_status_t solver_status;
    uint32_t sequence;
    uint8_t range_index;
    uint8_t condition_index;
    uint8_t condition_count;
    uint8_t solved_count;
    uint8_t total_conditions;
    uint8_t accepted;
    uint8_t attempts;
    int32_t latest_temperature_mC;
    int32_t open_temperature_mC;
    int32_t short_temperature_mC;
    int32_t load_temperature_mC;
    int32_t temperature_min_mC;
    int32_t temperature_max_mC;
    int32_t temperature_span_mC;
    bool latest_temperature_valid;
    bool temperature_span_valid;
    bool initialized;
    bool commit_started;
} app_calibration_wizard_t;

bsp_status_t app_calibration_fixture_profile_default_load(
    hw_range_id_t range_id,
    hw_excitation_freq_t frequency,
    measurement_complex_t *z_ohms,
    void *user);

bsp_status_t app_calibration_wizard_condition_key(hw_range_id_t range_id,
                                                  uint8_t index,
                                                  measurement_cal_key_t *key);
uint8_t app_calibration_wizard_condition_count(hw_range_id_t range_id);
uint8_t app_calibration_wizard_total_condition_count(void);

bsp_status_t app_calibration_wizard_init(app_calibration_wizard_t *wizard,
                                         app_calibration_service_t *service,
                                         const app_cal_session_io_t *io,
                                         const app_cal_fixture_profile_t *fixture);
bsp_status_t app_calibration_wizard_start(app_calibration_wizard_t *wizard,
                                          app_cal_wizard_mode_t mode,
                                          uint32_t sequence,
                                          int32_t temperature_mC,
                                          bool temperature_valid);
void app_calibration_wizard_confirm(app_calibration_wizard_t *wizard);
bsp_status_t app_calibration_wizard_cancel(app_calibration_wizard_t *wizard);
void app_calibration_wizard_step(app_calibration_wizard_t *wizard,
                                 const hw_safety_result_t *safety,
                                 const bsp_clock_summary_t *clock_summary,
                                 bsp_status_t clock_status,
                                 int32_t temperature_mC,
                                 bool temperature_valid,
                                 uint32_t now_ms);
bool app_calibration_wizard_active(const app_calibration_wizard_t *wizard);
bool app_calibration_wizard_terminal(const app_calibration_wizard_t *wizard);
void app_calibration_wizard_snapshot(const app_calibration_wizard_t *wizard,
                                     app_cal_wizard_snapshot_t *snapshot);
uint32_t app_calibration_wizard_context_size_bytes(void);
const char *app_cal_wizard_state_string(app_cal_wizard_state_t state);
const char *app_cal_wizard_error_string(app_cal_wizard_error_t error);

#endif
