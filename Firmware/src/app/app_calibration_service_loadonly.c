#include "app/app_calibration_service.h"

void app_calibration_service_init(app_calibration_service_t *service)
{
    if (service == NULL) return;
    *service = (app_calibration_service_t){.initialized = true, .status = APP_CAL_SERVICE_READY};
    app_calibration_runtime_init(&service->runtime);
}
void app_calibration_service_attach_workspace(app_calibration_service_t *service, app_io_workspace_t *workspace)
{
    if (service != NULL) service->workspace = workspace;
}
bsp_status_t app_calibration_service_load(app_calibration_service_t *service,
    const measurement_cal_store_io_t *io, uint32_t capacity)
{
    if (service == NULL || service->workspace == NULL || !service->initialized) return BSP_STATUS_INVALID_ARG;
    if (app_io_workspace_acquire(service->workspace, APP_IO_WORKSPACE_OWNER_CALIBRATION_STORE) != BSP_STATUS_OK)
        return BSP_STATUS_BUSY;
    service->store_workspace_held = true;
    const bsp_status_t status = app_calibration_runtime_refresh(&service->runtime, &service->store,
        io, capacity, app_io_workspace_calibration_frame(service->workspace),
        app_io_workspace_calibration_frame_bytes());
    const bsp_status_t released = app_io_workspace_release(service->workspace, APP_IO_WORKSPACE_OWNER_CALIBRATION_STORE);
    service->store_workspace_held = released != BSP_STATUS_OK;
    service->capacity_bytes = capacity;
    service->storage_available = service->runtime.store_ready;
    service->last_store_status = status == BSP_STATUS_OK ? released : status;
    if (released != BSP_STATUS_OK)
    {
        service->runtime.active_valid = false;
        service->status = APP_CAL_SERVICE_ERROR;
    }
    else service->status = service->runtime.active_valid ? APP_CAL_SERVICE_ACTIVE_VALID :
        service->storage_available ? APP_CAL_SERVICE_NO_VALID_CALIBRATION : APP_CAL_SERVICE_STORAGE_UNAVAILABLE;
    return service->last_store_status;
}
void app_calibration_service_mark_storage_unavailable(app_calibration_service_t *service)
{
    if (service == NULL) return;
    service->runtime.active_valid = false;
    service->runtime.store_ready = false;
    service->storage_available = false;
    service->status = APP_CAL_SERVICE_STORAGE_UNAVAILABLE;
}
app_cal_service_status_t app_calibration_service_status(const app_calibration_service_t *service)
{
    return service == NULL ? APP_CAL_SERVICE_UNINITIALIZED : service->status;
}
bool app_calibration_service_busy(const app_calibration_service_t *service)
{
    return service != NULL && service->store_workspace_held;
}
app_calibration_runtime_t *app_calibration_service_runtime(app_calibration_service_t *service)
{
    return service == NULL ? NULL : &service->runtime;
}
const app_calibration_runtime_t *app_calibration_service_runtime_const(const app_calibration_service_t *service)
{
    return service == NULL ? NULL : &service->runtime;
}
bool app_calibration_service_active_valid(const app_calibration_service_t *service)
{
    return service != NULL && service->runtime.active_valid;
}
const measurement_cal_set_t *app_calibration_service_active_set(const app_calibration_service_t *service)
{
    return service == NULL ? NULL : app_calibration_runtime_active_set(&service->runtime);
}
uint32_t app_calibration_service_active_sequence(const app_calibration_service_t *service)
{
    const measurement_cal_set_t *set = app_calibration_service_active_set(service);
    return set == NULL ? 0u : set->sequence;
}
uint32_t app_calibration_service_context_size_bytes(void)
{
    return sizeof(app_calibration_service_t);
}
const char *app_calibration_service_status_string(app_cal_service_status_t status)
{
    switch (status)
    {
    case APP_CAL_SERVICE_ACTIVE_VALID: return "ACTIVE_VALID";
    case APP_CAL_SERVICE_READY: return "READY";
    case APP_CAL_SERVICE_NO_VALID_CALIBRATION: return "NO_VALID_CALIBRATION";
    case APP_CAL_SERVICE_STORAGE_UNAVAILABLE: return "STORAGE_UNAVAILABLE";
    case APP_CAL_SERVICE_ERROR: return "ERROR";
    default: return "UNINITIALIZED";
    }
}
