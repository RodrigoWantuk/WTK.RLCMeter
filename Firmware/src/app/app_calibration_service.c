#include "app/app_calibration_service.h"

#include <stddef.h>

static bool store_busy(const measurement_cal_store_t *store)
{
    if (store == NULL)
    {
        return false;
    }
    const measurement_cal_store_state_t state = measurement_cal_store_state(store);
    return (state != MEASUREMENT_CAL_STORE_IDLE) &&
           (state != MEASUREMENT_CAL_STORE_DONE) &&
           (state != MEASUREMENT_CAL_STORE_ERROR);
}

static bool store_terminal_ok(const measurement_cal_store_t *store)
{
    return (store != NULL) && (measurement_cal_store_state(store) == MEASUREMENT_CAL_STORE_DONE);
}

static bool candidate_dirty(const app_calibration_service_t *service)
{
    return (service != NULL) &&
           ((service->candidate_state == APP_CAL_CANDIDATE_BUILDING) ||
            (service->candidate_state == APP_CAL_CANDIDATE_PARTIAL) ||
            (service->candidate_state == APP_CAL_CANDIDATE_COMPLETE) ||
            (service->candidate_state == APP_CAL_CANDIDATE_COMMITTING));
}

static void update_candidate_completeness(app_calibration_service_t *service)
{
    if (service == NULL)
    {
        return;
    }
    const measurement_cal_validity_t validity =
        app_calibration_service_candidate_validity(service);
    service->candidate_state = (validity.status == MEASUREMENT_CAL_VALIDITY_VALID) ?
                                   APP_CAL_CANDIDATE_COMPLETE :
                                   APP_CAL_CANDIDATE_PARTIAL;
    service->status = APP_CAL_SERVICE_CANDIDATE_DIRTY;
}

static bsp_status_t acquire_store_workspace(app_calibration_service_t *service)
{
    if ((service == NULL) || (service->workspace == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (service->store_workspace_held)
    {
        return BSP_STATUS_OK;
    }
    const bsp_status_t status =
        app_io_workspace_acquire(service->workspace, APP_IO_WORKSPACE_OWNER_CALIBRATION_STORE);
    if (status == BSP_STATUS_OK)
    {
        service->store_workspace_held = true;
    }
    return status;
}

static bsp_status_t release_store_workspace(app_calibration_service_t *service)
{
    if ((service == NULL) || !service->store_workspace_held)
    {
        return BSP_STATUS_OK;
    }
    const bsp_status_t status =
        app_io_workspace_release(service->workspace, APP_IO_WORKSPACE_OWNER_CALIBRATION_STORE);
    if (status == BSP_STATUS_OK)
    {
        service->store_workspace_held = false;
    }
    return status;
}

void app_calibration_service_init(app_calibration_service_t *service)
{
    if (service == NULL)
    {
        return;
    }
    *service = (app_calibration_service_t){0};
    app_calibration_runtime_init(&service->runtime);
    app_calibration_workflow_init(&service->workflow);
    app_calibration_campaign_init(&service->campaign);
    service->status = APP_CAL_SERVICE_READY;
    service->candidate_state = APP_CAL_CANDIDATE_NONE;
    service->last_store_status = BSP_STATUS_OK;
    service->initialized = true;
}

void app_calibration_service_attach_workspace(app_calibration_service_t *service,
                                              app_io_workspace_t *workspace)
{
    if (service != NULL)
    {
        service->workspace = workspace;
    }
}

bsp_status_t app_calibration_service_load(app_calibration_service_t *service,
                                          const measurement_cal_store_io_t *io,
                                          uint32_t capacity_bytes)
{
    if (service == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!service->initialized)
    {
        app_calibration_service_init(service);
    }
    if (app_calibration_service_busy(service))
    {
        service->status = APP_CAL_SERVICE_STORE_BUSY;
        service->last_store_status = BSP_STATUS_BUSY;
        return BSP_STATUS_BUSY;
    }
    if (candidate_dirty(service))
    {
        service->status = APP_CAL_SERVICE_CANDIDATE_DIRTY;
        service->last_store_status = BSP_STATUS_BUSY;
        return BSP_STATUS_BUSY;
    }
    if (io == NULL)
    {
        app_calibration_service_mark_storage_unavailable(service);
        service->last_store_status = BSP_STATUS_INVALID_ARG;
        return BSP_STATUS_INVALID_ARG;
    }

    bsp_status_t workspace_status = acquire_store_workspace(service);
    if (workspace_status != BSP_STATUS_OK)
    {
        service->status = APP_CAL_SERVICE_STORE_BUSY;
        service->last_store_status = workspace_status;
        return workspace_status;
    }

    service->capacity_bytes = capacity_bytes;
    const bsp_status_t status =
        app_calibration_runtime_refresh(&service->runtime,
                                        &service->store,
                                        io,
                                        capacity_bytes,
                                        app_io_workspace_calibration_frame(service->workspace),
                                        app_io_workspace_calibration_frame_bytes());
    workspace_status = release_store_workspace(service);
    service->last_store_status = status;
    service->storage_available = app_calibration_runtime_store_ready(&service->runtime);
    if ((status == BSP_STATUS_OK) && (workspace_status != BSP_STATUS_OK))
    {
        service->status = APP_CAL_SERVICE_ERROR;
        service->last_store_status = workspace_status;
        return workspace_status;
    }
    if (!service->storage_available)
    {
        service->status = APP_CAL_SERVICE_STORAGE_UNAVAILABLE;
        return status;
    }
    service->status = app_calibration_runtime_active_valid(&service->runtime) ?
                          APP_CAL_SERVICE_ACTIVE_VALID :
                          APP_CAL_SERVICE_NO_VALID_CALIBRATION;
    return status;
}

void app_calibration_service_mark_storage_unavailable(app_calibration_service_t *service)
{
    if (service == NULL)
    {
        return;
    }
    if (!service->initialized)
    {
        app_calibration_service_init(service);
    }
    (void)release_store_workspace(service);
    app_calibration_runtime_init(&service->runtime);
    service->storage_available = false;
    service->status = APP_CAL_SERVICE_STORAGE_UNAVAILABLE;
    service->last_store_status = BSP_STATUS_ERROR;
}

app_cal_service_status_t app_calibration_service_status(const app_calibration_service_t *service)
{
    if ((service == NULL) || !service->initialized)
    {
        return APP_CAL_SERVICE_UNINITIALIZED;
    }
    if (app_calibration_workflow_active(&service->workflow))
    {
        return APP_CAL_SERVICE_WORKFLOW_ACTIVE;
    }
    if (store_busy(&service->store))
    {
        return APP_CAL_SERVICE_STORE_BUSY;
    }
    if (candidate_dirty(service))
    {
        return APP_CAL_SERVICE_CANDIDATE_DIRTY;
    }
    if (service->status == APP_CAL_SERVICE_WORKFLOW_ACTIVE)
    {
        if (!service->storage_available)
        {
            return APP_CAL_SERVICE_STORAGE_UNAVAILABLE;
        }
        return app_calibration_runtime_active_valid(&service->runtime) ?
                   APP_CAL_SERVICE_ACTIVE_VALID :
                   APP_CAL_SERVICE_NO_VALID_CALIBRATION;
    }
    return service->status;
}

bool app_calibration_service_busy(const app_calibration_service_t *service)
{
    return (service != NULL) &&
           (app_calibration_workflow_active(&service->workflow) || store_busy(&service->store));
}

app_calibration_runtime_t *app_calibration_service_runtime(app_calibration_service_t *service)
{
    return (service == NULL) ? NULL : &service->runtime;
}

const app_calibration_runtime_t *app_calibration_service_runtime_const(
    const app_calibration_service_t *service)
{
    return (service == NULL) ? NULL : &service->runtime;
}

bool app_calibration_service_active_valid(const app_calibration_service_t *service)
{
    return (service != NULL) && app_calibration_runtime_active_valid(&service->runtime);
}

uint32_t app_calibration_service_active_sequence(const app_calibration_service_t *service)
{
    const measurement_cal_set_t *active = app_calibration_service_active_set(service);
    return (active == NULL) ? 0u : active->sequence;
}

const measurement_cal_set_t *app_calibration_service_active_set(
    const app_calibration_service_t *service)
{
    return (service == NULL) ? NULL : app_calibration_runtime_active_set(&service->runtime);
}

app_calibration_workflow_t *app_calibration_service_workflow(app_calibration_service_t *service)
{
    return (service == NULL) ? NULL : &service->workflow;
}

const app_calibration_workflow_t *app_calibration_service_workflow_const(
    const app_calibration_service_t *service)
{
    return (service == NULL) ? NULL : &service->workflow;
}

app_calibration_campaign_t *app_calibration_service_campaign(app_calibration_service_t *service)
{
    return (service == NULL) ? NULL : &service->campaign;
}

const app_calibration_campaign_t *app_calibration_service_campaign_const(
    const app_calibration_service_t *service)
{
    return (service == NULL) ? NULL : &service->campaign;
}

bsp_status_t app_calibration_service_start_workflow(app_calibration_service_t *service,
                                                    const app_cal_workflow_request_t *request)
{
    if ((service == NULL) || (request == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!service->initialized)
    {
        app_calibration_service_init(service);
    }
    if (app_calibration_service_busy(service))
    {
        return BSP_STATUS_BUSY;
    }
    service->workflow_sequence++;
    const bsp_status_t status =
        app_calibration_workflow_start(&service->workflow, request, service->workflow_sequence);
    if ((status == BSP_STATUS_BUSY) || (status == BSP_STATUS_OK))
    {
        service->status = APP_CAL_SERVICE_WORKFLOW_ACTIVE;
    }
    return status;
}

bsp_status_t app_calibration_service_candidate_begin(app_calibration_service_t *service)
{
    if (service == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (app_calibration_service_busy(service))
    {
        return BSP_STATUS_BUSY;
    }
    if (candidate_dirty(service))
    {
        service->status = APP_CAL_SERVICE_CANDIDATE_DIRTY;
        return BSP_STATUS_BUSY;
    }
    measurement_cal_set_init(&service->store.scan_set,
                             MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                             0u);
    app_calibration_campaign_init(&service->campaign);
    service->candidate_state = APP_CAL_CANDIDATE_BUILDING;
    service->status = APP_CAL_SERVICE_CANDIDATE_DIRTY;
    return BSP_STATUS_OK;
}

bsp_status_t app_calibration_service_candidate_discard(app_calibration_service_t *service)
{
    if (service == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (store_busy(&service->store) || app_calibration_workflow_active(&service->workflow))
    {
        return BSP_STATUS_BUSY;
    }
    if ((measurement_cal_store_state(&service->store) == MEASUREMENT_CAL_STORE_DONE) ||
        (measurement_cal_store_state(&service->store) == MEASUREMENT_CAL_STORE_ERROR))
    {
        const bsp_status_t status = measurement_cal_store_acknowledge(&service->store);
        if (status != BSP_STATUS_OK)
        {
            service->last_store_status = status;
            service->status = APP_CAL_SERVICE_ERROR;
            return status;
        }
        const bsp_status_t release = release_store_workspace(service);
        if (release != BSP_STATUS_OK)
        {
            service->last_store_status = release;
            service->status = APP_CAL_SERVICE_ERROR;
            return release;
        }
    }
    app_calibration_campaign_init(&service->campaign);
    measurement_cal_set_init(&service->store.scan_set,
                             MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                             0u);
    service->candidate_state = APP_CAL_CANDIDATE_NONE;
    service->status = app_calibration_runtime_active_valid(&service->runtime) ?
                          APP_CAL_SERVICE_ACTIVE_VALID :
                          APP_CAL_SERVICE_NO_VALID_CALIBRATION;
    service->last_store_status = BSP_STATUS_OK;
    return BSP_STATUS_OK;
}

measurement_cal_set_t *app_calibration_service_candidate_set(app_calibration_service_t *service)
{
    if ((service == NULL) || (service->candidate_state == APP_CAL_CANDIDATE_NONE) ||
        (service->candidate_state == APP_CAL_CANDIDATE_ACTIVATED))
    {
        return NULL;
    }
    return &service->store.scan_set;
}

const measurement_cal_set_t *app_calibration_service_candidate_set_const(
    const app_calibration_service_t *service)
{
    if ((service == NULL) || (service->candidate_state == APP_CAL_CANDIDATE_NONE) ||
        (service->candidate_state == APP_CAL_CANDIDATE_ACTIVATED))
    {
        return NULL;
    }
    return &service->store.scan_set;
}

measurement_cal_validity_t app_calibration_service_candidate_validity(
    const app_calibration_service_t *service)
{
    const measurement_cal_requirements_t requirements = measurement_cal_requirements_rev1_full();
    return measurement_cal_validate_set((service == NULL) ? NULL : &service->store.scan_set,
                                        &requirements,
                                        MEASUREMENT_CAL_HARDWARE_REV1,
                                        MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
}

bsp_status_t app_calibration_service_campaign_begin_condition(app_calibration_service_t *service,
                                                              const measurement_cal_key_t *key)
{
    if ((service == NULL) || (key == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!candidate_dirty(service))
    {
        return BSP_STATUS_BUSY;
    }
    if ((service->campaign.state == APP_CAL_CAMPAIGN_COLLECTING) &&
        (app_calibration_campaign_missing_mask(&service->campaign) != 0u) &&
        !measurement_cal_key_equal(&service->campaign.key, key))
    {
        return BSP_STATUS_BUSY;
    }
    return app_calibration_campaign_begin_condition(&service->campaign, key);
}

bsp_status_t app_calibration_service_campaign_submit_evidence(app_calibration_service_t *service,
                                                              const app_cal_evidence_t *evidence)
{
    if ((service == NULL) || (evidence == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!candidate_dirty(service))
    {
        return BSP_STATUS_BUSY;
    }
    const bsp_status_t status = app_calibration_campaign_submit_evidence(&service->campaign, evidence);
    if (status == BSP_STATUS_OK)
    {
        update_candidate_completeness(service);
    }
    return status;
}

measurement_cal_solver_status_t app_calibration_service_campaign_solve_condition(
    app_calibration_service_t *service,
    measurement_cal_record_t *record)
{
    if ((service == NULL) || (record == NULL))
    {
        return MEASUREMENT_CAL_SOLVER_INVALID_ARG;
    }
    if (!candidate_dirty(service))
    {
        return MEASUREMENT_CAL_SOLVER_MISSING_STANDARDS;
    }
    return app_calibration_campaign_solve_condition(&service->campaign, record);
}

bsp_status_t app_calibration_service_candidate_insert_record(app_calibration_service_t *service,
                                                             const measurement_cal_record_t *record)
{
    if ((service == NULL) || (record == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    measurement_cal_set_t *candidate = app_calibration_service_candidate_set(service);
    if (candidate == NULL)
    {
        return BSP_STATUS_BUSY;
    }
    const bsp_status_t status = app_calibration_campaign_insert_record(record, candidate);
    if (status == BSP_STATUS_OK)
    {
        update_candidate_completeness(service);
    }
    return status;
}

app_cal_candidate_state_t app_calibration_service_candidate_state(const app_calibration_service_t *service)
{
    return (service == NULL) ? APP_CAL_CANDIDATE_NONE : service->candidate_state;
}

bsp_status_t app_calibration_service_candidate_commit_start(app_calibration_service_t *service)
{
    if (service == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!service->storage_available)
    {
        service->status = APP_CAL_SERVICE_STORAGE_UNAVAILABLE;
        return BSP_STATUS_ERROR;
    }
    if (app_calibration_service_busy(service))
    {
        service->status = APP_CAL_SERVICE_STORE_BUSY;
        return BSP_STATUS_BUSY;
    }
    if ((service->candidate_state != APP_CAL_CANDIDATE_COMPLETE) &&
        (service->candidate_state != APP_CAL_CANDIDATE_PARTIAL) &&
        (service->candidate_state != APP_CAL_CANDIDATE_BUILDING) &&
        (service->candidate_state != APP_CAL_CANDIDATE_FAILED))
    {
        service->last_store_status = BSP_STATUS_INVALID_ARG;
        return BSP_STATUS_INVALID_ARG;
    }
    const measurement_cal_requirements_t requirements = measurement_cal_requirements_rev1_full();
    const measurement_cal_validity_t validity =
        measurement_cal_validate_set(&service->store.scan_set,
                                     &requirements,
                                     MEASUREMENT_CAL_HARDWARE_REV1,
                                     MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
    if (validity.status != MEASUREMENT_CAL_VALIDITY_VALID)
    {
        service->status = APP_CAL_SERVICE_ERROR;
        service->last_store_status = BSP_STATUS_INVALID_ARG;
        return BSP_STATUS_INVALID_ARG;
    }
    const bsp_status_t workspace_status = acquire_store_workspace(service);
    if (workspace_status != BSP_STATUS_OK)
    {
        service->status = APP_CAL_SERVICE_STORE_BUSY;
        service->last_store_status = workspace_status;
        return workspace_status;
    }
    service->store.image = app_io_workspace_calibration_frame(service->workspace);
    service->store.image_capacity = app_io_workspace_calibration_frame_bytes();
    const bsp_status_t status =
        measurement_cal_store_write_start(&service->store, &service->store.scan_set, &requirements);
    service->last_store_status = status;
    if (status == BSP_STATUS_BUSY)
    {
        service->candidate_state = APP_CAL_CANDIDATE_COMMITTING;
        service->status = APP_CAL_SERVICE_STORE_BUSY;
    }
    else
    {
        (void)release_store_workspace(service);
        service->candidate_state = APP_CAL_CANDIDATE_FAILED;
        service->status = APP_CAL_SERVICE_ERROR;
    }
    return status;
}

static bsp_status_t activate_verified_commit(app_calibration_service_t *service)
{
    service->runtime.active_set = service->store.scan_set;
    service->runtime.active_valid = true;
    service->runtime.store_ready = true;
    service->runtime.last_status = BSP_STATUS_OK;
    service->runtime.active_slot = measurement_cal_store_target_slot(&service->store);
    service->storage_available = true;
    service->status = APP_CAL_SERVICE_ACTIVE_VALID;
    service->last_store_status = BSP_STATUS_OK;
    const measurement_cal_requirements_t requirements = measurement_cal_requirements_rev1_full();
    (void)measurement_cal_store_refresh_diagnostics(&service->store,
                                                    &requirements,
                                                    MEASUREMENT_CAL_HARDWARE_REV1,
                                                    MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                                    service->runtime.slots);
    const bsp_status_t ack = measurement_cal_store_acknowledge(&service->store);
    const bsp_status_t release = release_store_workspace(service);
    service->candidate_state = (ack == BSP_STATUS_OK) ? APP_CAL_CANDIDATE_ACTIVATED :
                                                        APP_CAL_CANDIDATE_FAILED;
    return (ack == BSP_STATUS_OK) ? release : ack;
}

bsp_status_t app_calibration_service_step(app_calibration_service_t *service, uint32_t now_ms)
{
    if (service == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!store_busy(&service->store))
    {
        if (store_terminal_ok(&service->store) &&
            (service->candidate_state == APP_CAL_CANDIDATE_COMMITTING))
        {
            return activate_verified_commit(service);
        }
        return BSP_STATUS_OK;
    }
    const bsp_status_t status = measurement_cal_store_step(&service->store, now_ms);
    service->last_store_status = status;
    if (status == BSP_STATUS_BUSY)
    {
        service->status = APP_CAL_SERVICE_STORE_BUSY;
        return status;
    }
    if (status == BSP_STATUS_OK)
    {
        return activate_verified_commit(service);
    }
    service->status = APP_CAL_SERVICE_ERROR;
    service->candidate_state = APP_CAL_CANDIDATE_FAILED;
    return status;
}

uint32_t app_calibration_service_context_size_bytes(void)
{
    return (uint32_t)sizeof(app_calibration_service_t);
}

const char *app_calibration_service_status_string(app_cal_service_status_t status)
{
    switch (status)
    {
    case APP_CAL_SERVICE_UNINITIALIZED:
        return "UNINITIALIZED";
    case APP_CAL_SERVICE_READY:
        return "READY";
    case APP_CAL_SERVICE_STORAGE_UNAVAILABLE:
        return "STORAGE_UNAVAILABLE";
    case APP_CAL_SERVICE_NO_VALID_CALIBRATION:
        return "NO_VALID_CALIBRATION";
    case APP_CAL_SERVICE_ACTIVE_VALID:
        return "ACTIVE_VALID";
    case APP_CAL_SERVICE_WORKFLOW_ACTIVE:
        return "WORKFLOW_ACTIVE";
    case APP_CAL_SERVICE_STORE_BUSY:
        return "STORE_BUSY";
    case APP_CAL_SERVICE_CANDIDATE_DIRTY:
        return "CANDIDATE_DIRTY";
    case APP_CAL_SERVICE_ERROR:
    default:
        return "ERROR";
    }
}

const char *app_calibration_candidate_state_string(app_cal_candidate_state_t state)
{
    switch (state)
    {
    case APP_CAL_CANDIDATE_NONE:
        return "NONE";
    case APP_CAL_CANDIDATE_BUILDING:
        return "BUILDING";
    case APP_CAL_CANDIDATE_PARTIAL:
        return "PARTIAL";
    case APP_CAL_CANDIDATE_COMPLETE:
        return "COMPLETE";
    case APP_CAL_CANDIDATE_COMMITTING:
        return "COMMITTING";
    case APP_CAL_CANDIDATE_ACTIVATED:
        return "ACTIVATED";
    case APP_CAL_CANDIDATE_FAILED:
    default:
        return "FAILED";
    }
}
