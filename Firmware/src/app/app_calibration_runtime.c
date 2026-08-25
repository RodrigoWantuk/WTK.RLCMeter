#include "app/app_calibration_runtime.h"

void app_calibration_runtime_init(app_calibration_runtime_t *runtime)
{
    if (runtime == NULL)
    {
        return;
    }
    *runtime = (app_calibration_runtime_t){0};
    runtime->active_slot = MEASUREMENT_CAL_STORE_SLOT_A;
    runtime->last_status = BSP_STATUS_ERROR;
}

bsp_status_t app_calibration_runtime_refresh(app_calibration_runtime_t *runtime,
                                             measurement_cal_store_t *store_scratch,
                                             const measurement_cal_store_io_t *io,
                                             uint32_t capacity_bytes)
{
    if ((runtime == NULL) || (store_scratch == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    runtime->store_ready = false;
    runtime->active_valid = false;
    runtime->last_status = BSP_STATUS_ERROR;
    if (io == NULL)
    {
        runtime->last_status = BSP_STATUS_INVALID_ARG;
        return BSP_STATUS_INVALID_ARG;
    }

    bsp_status_t status = measurement_cal_store_init(store_scratch, io, capacity_bytes);
    runtime->store_ready = status == BSP_STATUS_OK;
    runtime->last_status = status;
    if (status != BSP_STATUS_OK)
    {
        return status;
    }

    const measurement_cal_requirements_t requirements = measurement_cal_requirements_rev1_full();
    status = measurement_cal_store_load_newest_usable(store_scratch,
                                                      &requirements,
                                                      MEASUREMENT_CAL_HARDWARE_REV1,
                                                      MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                                      &runtime->active_set,
                                                      &runtime->active_slot,
                                                      runtime->slots);
    runtime->active_valid = status == BSP_STATUS_OK;
    runtime->last_status = status;
    return status;
}

const measurement_cal_set_t *app_calibration_runtime_active_set(const app_calibration_runtime_t *runtime)
{
    return ((runtime != NULL) && runtime->active_valid) ? &runtime->active_set : NULL;
}

const measurement_cal_store_slot_info_t *app_calibration_runtime_slots(
    const app_calibration_runtime_t *runtime)
{
    return (runtime != NULL) ? runtime->slots : NULL;
}

measurement_cal_store_slot_t app_calibration_runtime_active_slot(const app_calibration_runtime_t *runtime)
{
    return (runtime != NULL) ? runtime->active_slot : MEASUREMENT_CAL_STORE_SLOT_A;
}

bsp_status_t app_calibration_runtime_last_status(const app_calibration_runtime_t *runtime)
{
    return (runtime != NULL) ? runtime->last_status : BSP_STATUS_INVALID_ARG;
}

bool app_calibration_runtime_store_ready(const app_calibration_runtime_t *runtime)
{
    return (runtime != NULL) && runtime->store_ready;
}

bool app_calibration_runtime_active_valid(const app_calibration_runtime_t *runtime)
{
    return (runtime != NULL) && runtime->active_valid;
}

uint32_t app_calibration_runtime_context_size_bytes(void)
{
    return (uint32_t)sizeof(app_calibration_runtime_t);
}
