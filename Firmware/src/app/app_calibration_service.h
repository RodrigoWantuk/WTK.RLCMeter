#ifndef WTK_APP_CALIBRATION_SERVICE_H
#define WTK_APP_CALIBRATION_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app/app_calibration_runtime.h"
#include "app/app_calibration_campaign.h"
#include "app/app_calibration_workflow.h"
#include "app/app_io_workspace.h"
#include "bsp/bsp_status.h"
#include "measurement/measurement_calibration_store.h"

typedef enum
{
    APP_CAL_SERVICE_UNINITIALIZED = 0,
    APP_CAL_SERVICE_READY,
    APP_CAL_SERVICE_STORAGE_UNAVAILABLE,
    APP_CAL_SERVICE_NO_VALID_CALIBRATION,
    APP_CAL_SERVICE_ACTIVE_VALID,
    APP_CAL_SERVICE_WORKFLOW_ACTIVE,
    APP_CAL_SERVICE_STORE_BUSY,
    APP_CAL_SERVICE_CANDIDATE_DIRTY,
    APP_CAL_SERVICE_ERROR,
} app_cal_service_status_t;

typedef enum
{
    APP_CAL_CANDIDATE_NONE = 0,
    APP_CAL_CANDIDATE_BUILDING,
    APP_CAL_CANDIDATE_PARTIAL,
    APP_CAL_CANDIDATE_COMPLETE,
    APP_CAL_CANDIDATE_COMMITTING,
    APP_CAL_CANDIDATE_ACTIVATED,
    APP_CAL_CANDIDATE_FAILED,
} app_cal_candidate_state_t;

typedef struct
{
    app_calibration_runtime_t runtime;
    measurement_cal_store_t store;
    app_calibration_workflow_t workflow;
    app_calibration_campaign_t campaign;
    app_cal_service_status_t status;
    app_cal_candidate_state_t candidate_state;
    bsp_status_t last_store_status;
    uint32_t capacity_bytes;
    uint32_t workflow_sequence;
    app_io_workspace_t *workspace;
    bool initialized;
    bool storage_available;
    bool store_workspace_held;
} app_calibration_service_t;

void app_calibration_service_init(app_calibration_service_t *service);
void app_calibration_service_attach_workspace(app_calibration_service_t *service,
                                              app_io_workspace_t *workspace);
bsp_status_t app_calibration_service_load(app_calibration_service_t *service,
                                          const measurement_cal_store_io_t *io,
                                          uint32_t capacity_bytes);
void app_calibration_service_mark_storage_unavailable(app_calibration_service_t *service);
app_cal_service_status_t app_calibration_service_status(const app_calibration_service_t *service);
bool app_calibration_service_busy(const app_calibration_service_t *service);

app_calibration_runtime_t *app_calibration_service_runtime(app_calibration_service_t *service);
const app_calibration_runtime_t *app_calibration_service_runtime_const(
    const app_calibration_service_t *service);
bool app_calibration_service_active_valid(const app_calibration_service_t *service);
uint32_t app_calibration_service_active_sequence(const app_calibration_service_t *service);
const measurement_cal_set_t *app_calibration_service_active_set(
    const app_calibration_service_t *service);
app_calibration_workflow_t *app_calibration_service_workflow(app_calibration_service_t *service);
const app_calibration_workflow_t *app_calibration_service_workflow_const(
    const app_calibration_service_t *service);
app_calibration_campaign_t *app_calibration_service_campaign(app_calibration_service_t *service);
const app_calibration_campaign_t *app_calibration_service_campaign_const(
    const app_calibration_service_t *service);

bsp_status_t app_calibration_service_start_workflow(app_calibration_service_t *service,
                                                    const app_cal_workflow_request_t *request);
bsp_status_t app_calibration_service_candidate_begin(app_calibration_service_t *service);
bsp_status_t app_calibration_service_candidate_discard(app_calibration_service_t *service);
measurement_cal_set_t *app_calibration_service_candidate_set(app_calibration_service_t *service);
const measurement_cal_set_t *app_calibration_service_candidate_set_const(
    const app_calibration_service_t *service);
measurement_cal_validity_t app_calibration_service_candidate_validity(
    const app_calibration_service_t *service);
bsp_status_t app_calibration_service_campaign_begin_condition(app_calibration_service_t *service,
                                                              const measurement_cal_key_t *key);
bsp_status_t app_calibration_service_campaign_submit_evidence(app_calibration_service_t *service,
                                                              const app_cal_evidence_t *evidence);
measurement_cal_solver_status_t app_calibration_service_campaign_solve_condition(
    app_calibration_service_t *service,
    measurement_cal_record_t *record);
bsp_status_t app_calibration_service_candidate_insert_record(app_calibration_service_t *service,
                                                             const measurement_cal_record_t *record);
app_cal_candidate_state_t app_calibration_service_candidate_state(const app_calibration_service_t *service);
bsp_status_t app_calibration_service_candidate_commit_start(app_calibration_service_t *service);
bsp_status_t app_calibration_service_step(app_calibration_service_t *service, uint32_t now_ms);
uint32_t app_calibration_service_context_size_bytes(void);
const char *app_calibration_service_status_string(app_cal_service_status_t status);
const char *app_calibration_candidate_state_string(app_cal_candidate_state_t state);

#endif
