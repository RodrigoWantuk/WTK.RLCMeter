#ifndef WTK_APP_CAL_INTEGRATION_H
#define WTK_APP_CAL_INTEGRATION_H

#include "app/app_calibration_workflow.h"

typedef struct
{
    measurement_complex_t mean[6];
    uint32_t clip_mask;
    uint16_t count;
} app_cal_integration_t;

/* BUSY means a partial group; the caller must finish SAFE teardown before
   acquiring the next block through a new, independently permitted transaction. */
bsp_status_t app_cal_integration_push(app_cal_integration_t *integration,
    uint16_t required, const hw_metrology_block_t *block,
    const app_cal_workflow_request_t *request, app_cal_capture_sample_t *sample);

#endif
