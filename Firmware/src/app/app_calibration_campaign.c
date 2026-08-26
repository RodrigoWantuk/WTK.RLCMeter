#include "app/app_calibration_campaign.h"

#include <stddef.h>

enum
{
    APP_CAL_CAMPAIGN_MISSING_OPEN = 1u << 0,
    APP_CAL_CAMPAIGN_MISSING_SHORT = 1u << 1,
    APP_CAL_CAMPAIGN_MISSING_LOAD = 1u << 2,
};

static measurement_cal_standard_type_t convert_standard(app_cal_standard_type_t standard)
{
    switch (standard)
    {
    case APP_CAL_STANDARD_OPEN:
        return MEASUREMENT_CAL_STANDARD_OPEN;
    case APP_CAL_STANDARD_SHORT:
        return MEASUREMENT_CAL_STANDARD_SHORT;
    case APP_CAL_STANDARD_LOAD:
    default:
        return MEASUREMENT_CAL_STANDARD_LOAD;
    }
}

static bool key_matches(const app_calibration_campaign_t *campaign, const measurement_cal_key_t *key)
{
    return (campaign != NULL) && (key != NULL) &&
           measurement_cal_key_equal(&campaign->key, key);
}

static measurement_complex_t transfer_mean_or_zero(const app_cal_evidence_t *evidence)
{
    return ((evidence != NULL) && evidence->hg_overlap_valid) ?
               evidence->hg_observed_transfer.mean :
               measurement_complex(0.0f, 0.0f);
}

static bool open_y_to_t(measurement_complex_t open_y, measurement_complex_t *t)
{
    return (t != NULL) &&
           (measurement_complex_div(measurement_complex(1.0f, 0.0f),
                                    measurement_complex_add(open_y,
                                                            measurement_complex(1.0f, 0.0f)),
                                    t) == MEASUREMENT_STATUS_OK);
}

static measurement_cal_solver_standard_t make_solver_standard(const app_cal_evidence_t *evidence)
{
    const bool open = evidence->standard.type == APP_CAL_STANDARD_OPEN;
    measurement_cal_solver_standard_t standard = {
        .key = evidence->key,
        .standard = convert_standard(evidence->standard.type),
        .standard_z_ohms = evidence->standard.z_ohms,
        .standard_z_valid = evidence->standard.z_valid,
        .t_1x = measurement_complex(0.0f, 0.0f),
        .t_hg = measurement_complex(0.0f, 0.0f),
        .hg_observed_transfer = transfer_mean_or_zero(evidence),
        .temperature_mC = evidence->temperature.valid ? evidence->temperature.mean_mC : 0,
        .ret_1x_valid = evidence->ret_1x_evidence_valid,
        .ret_hg_valid = evidence->ret_hg_evidence_valid,
        .hg_observed_valid = evidence->hg_overlap_valid,
        .stable = evidence->stable,
        .temperature_valid = evidence->temperature.valid,
        .present = true,
    };
    if (open)
    {
        if (!open_y_to_t(evidence->open_y_1x.mean, &standard.t_1x))
        {
            standard.ret_1x_valid = false;
        }
        if (!open_y_to_t(evidence->open_y_hg.mean, &standard.t_hg))
        {
            standard.ret_hg_valid = false;
        }
    }
    else
    {
        measurement_complex_t t_1x = measurement_complex(0.0f, 0.0f);
        measurement_complex_t t_hg = measurement_complex(0.0f, 0.0f);
        if (measurement_complex_div(evidence->ret_1x.mean,
                                    evidence->source_1.mean,
                                    &t_1x) == MEASUREMENT_STATUS_OK)
        {
            standard.t_1x = t_1x;
        }
        else
        {
            standard.ret_1x_valid = false;
        }
        if (measurement_complex_div(evidence->ret_hg_reconstructed.mean,
                                    evidence->source_2.mean,
                                    &t_hg) == MEASUREMENT_STATUS_OK)
        {
            standard.t_hg = t_hg;
        }
        else
        {
            standard.ret_hg_valid = false;
        }
    }
    if (standard.standard == MEASUREMENT_CAL_STANDARD_OPEN)
    {
        standard.standard_z_ohms = measurement_complex(0.0f, 0.0f);
        standard.standard_z_valid = false;
    }
    else if (standard.standard == MEASUREMENT_CAL_STANDARD_SHORT)
    {
        standard.standard_z_ohms = measurement_complex(0.0f, 0.0f);
        standard.standard_z_valid = true;
    }
    return standard;
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

    const measurement_cal_solver_standard_t standard = make_solver_standard(evidence);
    switch (evidence->standard.type)
    {
    case APP_CAL_STANDARD_OPEN:
        campaign->open = standard;
        campaign->have_open = true;
        break;
    case APP_CAL_STANDARD_SHORT:
        campaign->shorted = standard;
        campaign->have_short = true;
        break;
    case APP_CAL_STANDARD_LOAD:
        campaign->load = standard;
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
