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

void app_calibration_service_init(app_calibration_service_t *service)
{
    if (service == NULL)
    {
        return;
    }
    *service = (app_calibration_service_t){0};
    app_calibration_runtime_init(&service->runtime);
    app_calibration_workflow_init(&service->workflow);
    service->status = APP_CAL_SERVICE_READY;
    service->last_store_status = BSP_STATUS_OK;
    service->initialized = true;
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
    if (io == NULL)
    {
        app_calibration_service_mark_storage_unavailable(service);
        service->last_store_status = BSP_STATUS_INVALID_ARG;
        return BSP_STATUS_INVALID_ARG;
    }

    service->capacity_bytes = capacity_bytes;
    const bsp_status_t status =
        app_calibration_runtime_refresh(&service->runtime, &service->store, io, capacity_bytes);
    service->last_store_status = status;
    service->storage_available = app_calibration_runtime_store_ready(&service->runtime);
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
    case APP_CAL_SERVICE_ERROR:
    default:
        return "ERROR";
    }
}
