#ifndef WTK_APP_CALIBRATION_CAMPAIGN_H
#define WTK_APP_CALIBRATION_CAMPAIGN_H

#include <stdbool.h>
#include <stdint.h>

#include "app/app_calibration_workflow.h"
#include "bsp/bsp_status.h"
#include "measurement/measurement_calibration_solver.h"

typedef enum
{
    APP_CAL_CAMPAIGN_EMPTY = 0,
    APP_CAL_CAMPAIGN_COLLECTING,
    APP_CAL_CAMPAIGN_CONDITION_SOLVED,
} app_cal_campaign_state_t;

typedef struct
{
    measurement_cal_key_t key;
    measurement_cal_solver_standard_t open;
    measurement_cal_solver_standard_t shorted;
    measurement_cal_solver_standard_t load;
    measurement_cal_solver_solution_t last_solution;
    app_cal_campaign_state_t state;
    uint32_t solved_count;
    bool have_open;
    bool have_short;
    bool have_load;
} app_calibration_campaign_t;

void app_calibration_campaign_init(app_calibration_campaign_t *campaign);
bsp_status_t app_calibration_campaign_begin_condition(app_calibration_campaign_t *campaign,
                                                      const measurement_cal_key_t *key);
bsp_status_t app_calibration_campaign_submit_evidence(app_calibration_campaign_t *campaign,
                                                      const app_cal_evidence_t *evidence);
bsp_status_t app_calibration_standard_from_evidence(
    const app_cal_evidence_t *evidence,
    measurement_cal_solver_standard_t *standard);
bsp_status_t app_calibration_campaign_submit_standard(
    app_calibration_campaign_t *campaign,
    const measurement_cal_solver_standard_t *standard);
measurement_cal_solver_status_t app_calibration_campaign_solve_condition(
    app_calibration_campaign_t *campaign,
    measurement_cal_record_t *record);
bsp_status_t app_calibration_campaign_insert_record(const measurement_cal_record_t *record,
                                                    measurement_cal_set_t *candidate);
bool app_calibration_campaign_condition_ready(const app_calibration_campaign_t *campaign);
uint32_t app_calibration_campaign_missing_mask(const app_calibration_campaign_t *campaign);
uint32_t app_calibration_campaign_context_size_bytes(void);
const char *app_cal_campaign_state_string(app_cal_campaign_state_t state);

#endif
