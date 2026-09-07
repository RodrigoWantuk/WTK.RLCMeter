#include "app/app_calibration_campaign.h"

#include <stddef.h>

enum
{
    APP_CAL_CAMPAIGN_MISSING_OPEN = 1u << 0,
    APP_CAL_CAMPAIGN_MISSING_SHORT = 1u << 1,
    APP_CAL_CAMPAIGN_MISSING_LOAD = 1u << 2,
};

static bool key_matches(const app_calibration_campaign_t *campaign, const measurement_cal_key_t *key)
{
    return (campaign != NULL) && (key != NULL) &&
           measurement_cal_key_equal(&campaign->key, key);
}

void app_calibration_campaign_init(app_calibration_campaign_t *campaign)
{
    if (campaign != NULL)
    {
        *campaign = (app_calibration_campaign_t){0};
        campaign->state = APP_CAL_CAMPAIGN_EMPTY;
    }
}

bsp_status_t app_calibration_campaign_begin_condition(app_calibration_campaign_t *campaign,
                                                      const measurement_cal_key_t *key)
{
    if ((campaign == NULL) || (key == NULL) ||
        (key->model_version != MEASUREMENT_CAL_MODEL_VERSION_CURRENT) ||
        !measurement_cal_condition_allowed(key->range_id, key->frequency, key->amplitude))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    app_calibration_campaign_init(campaign);
    campaign->key = *key;
    campaign->state = APP_CAL_CAMPAIGN_COLLECTING;
    return BSP_STATUS_OK;
}

bsp_status_t app_calibration_campaign_submit_evidence(app_calibration_campaign_t *campaign,
                                                      const app_cal_evidence_t *evidence)
{
    if ((campaign == NULL) || (evidence == NULL) ||
        (campaign->state == APP_CAL_CAMPAIGN_EMPTY) ||
        !key_matches(campaign, &evidence->key) ||
        !evidence->stable)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    measurement_cal_solver_standard_t standard;
    const bsp_status_t status = app_calibration_workflow_standard_from_evidence(evidence, &standard);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    return app_calibration_campaign_submit_standard(campaign, &standard);
}

bsp_status_t app_calibration_campaign_submit_standard(
    app_calibration_campaign_t *campaign,
    const measurement_cal_solver_standard_t *standard)
{
    if ((campaign == NULL) || (standard == NULL) ||
        (campaign->state == APP_CAL_CAMPAIGN_EMPTY) ||
        !key_matches(campaign, &standard->key) ||
        !standard->stable ||
        !standard->present)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    switch (standard->standard)
    {
    case MEASUREMENT_CAL_STANDARD_OPEN:
        campaign->open = *standard;
        campaign->have_open = true;
        break;
    case MEASUREMENT_CAL_STANDARD_SHORT:
        campaign->shorted = *standard;
        campaign->have_short = true;
        break;
    case MEASUREMENT_CAL_STANDARD_LOAD:
        campaign->load = *standard;
        campaign->have_load = true;
        break;
    default:
        return BSP_STATUS_INVALID_ARG;
    }
    campaign->state = app_calibration_campaign_condition_ready(campaign) ?
                          APP_CAL_CAMPAIGN_CONDITION_SOLVED :
                          APP_CAL_CAMPAIGN_COLLECTING;
    return BSP_STATUS_OK;
}

measurement_cal_solver_status_t app_calibration_campaign_solve_condition(
    app_calibration_campaign_t *campaign,
    measurement_cal_record_t *record)
{
    if ((campaign == NULL) || (record == NULL))
    {
        return MEASUREMENT_CAL_SOLVER_INVALID_ARG;
    }
    if (!app_calibration_campaign_condition_ready(campaign))
    {
        return MEASUREMENT_CAL_SOLVER_MISSING_STANDARDS;
    }
    const measurement_cal_solver_input_t input = {
        .open = campaign->open,
        .shorted = campaign->shorted,
        .load = campaign->load,
    };
    const measurement_cal_solver_status_t status =
        measurement_cal_solver_solve(&input, &campaign->last_solution);
    if (status != MEASUREMENT_CAL_SOLVER_OK)
    {
        return status;
    }
    *record = measurement_cal_solver_make_record(&campaign->last_solution);
    campaign->solved_count++;
    campaign->state = APP_CAL_CAMPAIGN_CONDITION_SOLVED;
    return MEASUREMENT_CAL_SOLVER_OK;
}

bsp_status_t app_calibration_campaign_insert_record(const measurement_cal_record_t *record,
                                                    measurement_cal_set_t *candidate)
{
    if ((record == NULL) || (candidate == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (measurement_cal_set_replace_record(candidate, record))
    {
        return BSP_STATUS_OK;
    }
    return measurement_cal_set_add_record(candidate, record) ? BSP_STATUS_OK : BSP_STATUS_ERROR;
}

bool app_calibration_campaign_condition_ready(const app_calibration_campaign_t *campaign)
{
    return (campaign != NULL) && campaign->have_open && campaign->have_short && campaign->have_load;
}

uint32_t app_calibration_campaign_missing_mask(const app_calibration_campaign_t *campaign)
{
    if (campaign == NULL)
    {
        return APP_CAL_CAMPAIGN_MISSING_OPEN |
               APP_CAL_CAMPAIGN_MISSING_SHORT |
               APP_CAL_CAMPAIGN_MISSING_LOAD;
    }
    uint32_t mask = 0u;
    if (!campaign->have_open)
    {
        mask |= APP_CAL_CAMPAIGN_MISSING_OPEN;
    }
    if (!campaign->have_short)
    {
        mask |= APP_CAL_CAMPAIGN_MISSING_SHORT;
    }
    if (!campaign->have_load)
    {
        mask |= APP_CAL_CAMPAIGN_MISSING_LOAD;
    }
    return mask;
}

uint32_t app_calibration_campaign_context_size_bytes(void)
{
    return (uint32_t)sizeof(app_calibration_campaign_t);
}

const char *app_cal_campaign_state_string(app_cal_campaign_state_t state)
{
    switch (state)
    {
    case APP_CAL_CAMPAIGN_EMPTY:
        return "EMPTY";
    case APP_CAL_CAMPAIGN_COLLECTING:
        return "COLLECTING";
    case APP_CAL_CAMPAIGN_CONDITION_SOLVED:
    default:
        return "CONDITION_SOLVED";
    }
}
