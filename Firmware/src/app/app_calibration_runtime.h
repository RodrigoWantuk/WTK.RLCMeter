#ifndef WTK_APP_CALIBRATION_RUNTIME_H
#define WTK_APP_CALIBRATION_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "measurement/measurement_calibration.h"
#include "measurement/measurement_calibration_store.h"

typedef struct
{
    measurement_cal_set_t active_set;
    measurement_cal_store_slot_info_t slots[2];
    measurement_cal_store_slot_t active_slot;
    bsp_status_t last_status;
    bool store_ready;
    bool active_valid;
} app_calibration_runtime_t;

void app_calibration_runtime_init(app_calibration_runtime_t *runtime);
bsp_status_t app_calibration_runtime_refresh(app_calibration_runtime_t *runtime,
                                             measurement_cal_store_t *store_scratch,
                                             const measurement_cal_store_io_t *io,
                                             uint32_t capacity_bytes);
const measurement_cal_set_t *app_calibration_runtime_active_set(const app_calibration_runtime_t *runtime);
const measurement_cal_store_slot_info_t *app_calibration_runtime_slots(
    const app_calibration_runtime_t *runtime);
measurement_cal_store_slot_t app_calibration_runtime_active_slot(const app_calibration_runtime_t *runtime);
bsp_status_t app_calibration_runtime_last_status(const app_calibration_runtime_t *runtime);
bool app_calibration_runtime_store_ready(const app_calibration_runtime_t *runtime);
bool app_calibration_runtime_active_valid(const app_calibration_runtime_t *runtime);
uint32_t app_calibration_runtime_context_size_bytes(void);

#endif
