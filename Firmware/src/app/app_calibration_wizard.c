#include "app/app_calibration_wizard.h"

#include <stddef.h>

#include "measurement/measurement_condition.h"

static const hw_excitation_freq_t k_frequencies[] = {
    HW_EXCITATION_FREQ_100HZ,
    HW_EXCITATION_FREQ_1KHZ,
    HW_EXCITATION_FREQ_10KHZ,
};

static const hw_excitation_amp_t k_amplitudes[] = {
    HW_EXCITATION_AMP_100MVRMS,
    HW_EXCITATION_AMP_500MVRMS,
};

static hw_range_id_t range_from_index(uint8_t index)
{
    switch (index)
    {
    case 0u:
        return HW_RANGE_ID_10R;
    case 1u:
        return HW_RANGE_ID_100R;
    case 2u:
        return HW_RANGE_ID_1K;
    case 3u:
        return HW_RANGE_ID_10K;
    case 4u:
        return HW_RANGE_ID_100K;
    case 5u:
        return HW_RANGE_ID_1M;
    default:
        return HW_RANGE_ID_INVALID;
    }
}

static bool state_is_capture(app_cal_wizard_state_t state)
{
    return (state == APP_CAL_WIZARD_CAPTURE_OPEN) ||
           (state == APP_CAL_WIZARD_CAPTURE_SHORT) ||
           (state == APP_CAL_WIZARD_CAPTURE_LOAD);
}

static app_cal_wizard_state_t capture_state_for_standard(app_cal_standard_type_t standard)
{
    switch (standard)
    {
    case APP_CAL_STANDARD_OPEN:
        return APP_CAL_WIZARD_CAPTURE_OPEN;
    case APP_CAL_STANDARD_SHORT:
        return APP_CAL_WIZARD_CAPTURE_SHORT;
    case APP_CAL_STANDARD_LOAD:
    default:
        return APP_CAL_WIZARD_CAPTURE_LOAD;
    }
}

static measurement_cal_standard_type_t solver_standard_type(app_cal_standard_type_t standard)
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

static void clear_range_cache(app_calibration_wizard_t *wizard)
{
    for (uint8_t i = 0u; i < APP_CAL_WIZARD_MAX_CONDITIONS_PER_RANGE; i++)
    {
        wizard->open_cache[i] = (app_cal_wizard_cached_standard_t){0};
        wizard->short_cache[i] = (app_cal_wizard_cached_standard_t){0};
    }
}

static void enter_failure(app_calibration_wizard_t *wizard, app_cal_wizard_error_t error)
{
    if (wizard != NULL)
    {
        wizard->error = error;
        wizard->state = APP_CAL_WIZARD_FAILED;
    }
}

static bool current_key(const app_calibration_wizard_t *wizard, measurement_cal_key_t *key)
{
    if ((wizard == NULL) || (key == NULL))
    {
        return false;
    }
    return app_calibration_wizard_condition_key(range_from_index(wizard->range_index),
                                                wizard->condition_index,
                                                key) == BSP_STATUS_OK;
}

static app_cal_workflow_request_t make_request(app_calibration_wizard_t *wizard)
{
    measurement_cal_key_t key = {0};
    (void)current_key(wizard, &key);
    app_cal_workflow_request_t request = {
        .key = key,
        .standard = {
            .type = wizard->standard,
            .z_ohms = measurement_complex(0.0f, 0.0f),
            .z_valid = false,
        },
        .temperature_mC = wizard->temperature_mC,
        .temperature_valid = wizard->temperature_valid,
    };
    if (wizard->standard == APP_CAL_STANDARD_SHORT)
    {
        request.standard.z_valid = true;
    }
    else if (wizard->standard == APP_CAL_STANDARD_LOAD)
    {
        measurement_complex_t load_z = measurement_complex(0.0f, 0.0f);
        const bsp_status_t status = wizard->fixture.load_z(key.range_id,
                                                           key.frequency,
                                                           &load_z,
                                                           wizard->fixture.user);
        if (status == BSP_STATUS_OK)
        {
            request.standard.z_ohms = load_z;
            request.standard.z_valid = true;
        }
    }
    return request;
}

static bool safety_allows_capture(const hw_safety_result_t *safety)
{
    return (safety != NULL) && safety->measure_allowed;
}

static bsp_status_t start_current_condition(app_calibration_wizard_t *wizard,
                                            const hw_safety_result_t *safety,
                                            const bsp_clock_summary_t *clock_summary,
                                            bsp_status_t clock_status,
                                            uint32_t now_ms)
{
    measurement_cal_key_t key = {0};
    if ((wizard == NULL) || !current_key(wizard, &key))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!safety_allows_capture(safety))
    {
        wizard->state_before_safety = capture_state_for_standard(wizard->standard);
        wizard->state = APP_CAL_WIZARD_SAFETY_BLOCKED;
        return BSP_STATUS_BUSY;
    }
    app_cal_workflow_request_t request = make_request(wizard);
    if ((wizard->standard == APP_CAL_STANDARD_LOAD) && !request.standard.z_valid)
    {
        enter_failure(wizard, APP_CAL_WIZARD_ERROR_CONDITION);
        return BSP_STATUS_ERROR;
    }
    const bsp_status_t status =
        app_calibration_session_start(&wizard->session, &request, clock_summary, clock_status, now_ms);
    if (status == BSP_STATUS_BUSY)
    {
        wizard->state = capture_state_for_standard(wizard->standard);
    }
    else
    {
        enter_failure(wizard, APP_CAL_WIZARD_ERROR_PHASE05);
    }
    return status;
}

static bsp_status_t cache_standard(app_calibration_wizard_t *wizard,
                                   const measurement_cal_solver_standard_t *standard)
{
    if ((wizard == NULL) || (standard == NULL) ||
        (wizard->condition_index >= APP_CAL_WIZARD_MAX_CONDITIONS_PER_RANGE))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    app_cal_wizard_cached_standard_t *slot =
        (standard->standard == MEASUREMENT_CAL_STANDARD_OPEN) ?
            &wizard->open_cache[wizard->condition_index] :
            &wizard->short_cache[wizard->condition_index];
    slot->key = standard->key;
    slot->standard = *standard;
    slot->valid = true;
    return BSP_STATUS_OK;
}

static bsp_status_t solve_load_condition(app_calibration_wizard_t *wizard,
                                         const measurement_cal_solver_standard_t *load)
{
    if ((wizard == NULL) || (load == NULL) ||
        (wizard->condition_index >= APP_CAL_WIZARD_MAX_CONDITIONS_PER_RANGE))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    const app_cal_wizard_cached_standard_t *open = &wizard->open_cache[wizard->condition_index];
    const app_cal_wizard_cached_standard_t *shorted = &wizard->short_cache[wizard->condition_index];
    if (!open->valid || !shorted->valid ||
        !measurement_cal_key_equal(&open->key, &load->key) ||
        !measurement_cal_key_equal(&shorted->key, &load->key))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    app_calibration_campaign_t *campaign = app_calibration_service_campaign(wizard->service);
    measurement_cal_record_t record;
    bsp_status_t status = app_calibration_campaign_begin_condition(campaign, &load->key);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    status = app_calibration_campaign_submit_standard(campaign, &open->standard);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    status = app_calibration_campaign_submit_standard(campaign, &shorted->standard);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    status = app_calibration_campaign_submit_standard(campaign, load);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    wizard->solver_status = app_calibration_campaign_solve_condition(campaign, &record);
    if (wizard->solver_status != MEASUREMENT_CAL_SOLVER_OK)
    {
        return BSP_STATUS_ERROR;
    }
    status = app_calibration_service_candidate_insert_record(wizard->service, &record);
    if (status == BSP_STATUS_OK)
    {
        wizard->solved_count++;
    }
    return status;
}

static bool validate_complete_candidate(app_calibration_wizard_t *wizard)
{
    if (wizard == NULL)
    {
        return false;
    }
    const measurement_cal_set_t *candidate =
        app_calibration_service_candidate_set_const(wizard->service);
    const measurement_cal_validity_t validity =
        app_calibration_service_candidate_validity(wizard->service);
    return (candidate != NULL) &&
           (candidate->record_count == MEASUREMENT_CAL_MAX_RECORDS) &&
           (wizard->solved_count == MEASUREMENT_CAL_MAX_RECORDS) &&
           (validity.status == MEASUREMENT_CAL_VALIDITY_VALID);
}

static void begin_range(app_calibration_wizard_t *wizard)
{
    wizard->condition_index = 0u;
    wizard->condition_count =
        app_calibration_wizard_condition_count(range_from_index(wizard->range_index));
    wizard->standard = APP_CAL_STANDARD_OPEN;
    clear_range_cache(wizard);
    wizard->state = APP_CAL_WIZARD_WAIT_OPEN_FIXTURE;
}

static void advance_after_condition(app_calibration_wizard_t *wizard)
{
    if (wizard == NULL)
    {
        return;
    }
    wizard->condition_index++;
    if (wizard->condition_index < wizard->condition_count)
    {
        wizard->state = capture_state_for_standard(wizard->standard);
        return;
    }
    wizard->condition_index = 0u;
    if (wizard->standard == APP_CAL_STANDARD_OPEN)
    {
        wizard->standard = APP_CAL_STANDARD_SHORT;
        wizard->state = APP_CAL_WIZARD_WAIT_SHORT_FIXTURE;
        return;
    }
    if (wizard->standard == APP_CAL_STANDARD_SHORT)
    {
        wizard->standard = APP_CAL_STANDARD_LOAD;
        wizard->state = APP_CAL_WIZARD_WAIT_LOAD_FIXTURE;
        return;
    }
    wizard->state = APP_CAL_WIZARD_RANGE_COMPLETE;
}

bsp_status_t app_calibration_fixture_profile_default_load(
    hw_range_id_t range_id,
    hw_excitation_freq_t frequency,
    measurement_complex_t *z_ohms,
    void *user)
{
    (void)frequency;
    (void)user;
    if (z_ohms == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    switch (range_id)
    {
    case HW_RANGE_ID_10R:
        *z_ohms = measurement_complex(10.0f, 0.0f);
        return BSP_STATUS_OK;
    case HW_RANGE_ID_100R:
        *z_ohms = measurement_complex(100.0f, 0.0f);
        return BSP_STATUS_OK;
    case HW_RANGE_ID_1K:
        *z_ohms = measurement_complex(1000.0f, 0.0f);
        return BSP_STATUS_OK;
    case HW_RANGE_ID_10K:
        *z_ohms = measurement_complex(10000.0f, 0.0f);
        return BSP_STATUS_OK;
    case HW_RANGE_ID_100K:
        *z_ohms = measurement_complex(100000.0f, 0.0f);
        return BSP_STATUS_OK;
    case HW_RANGE_ID_1M:
        *z_ohms = measurement_complex(1000000.0f, 0.0f);
        return BSP_STATUS_OK;
    case HW_RANGE_ID_INVALID:
    default:
        *z_ohms = measurement_complex(0.0f, 0.0f);
        return BSP_STATUS_INVALID_ARG;
    }
}

bsp_status_t app_calibration_wizard_condition_key(hw_range_id_t range_id,
                                                  uint8_t index,
                                                  measurement_cal_key_t *key)
{
    if (key == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    uint8_t current = 0u;
    for (uint8_t f = 0u; f < (uint8_t)(sizeof(k_frequencies) / sizeof(k_frequencies[0])); f++)
    {
        for (uint8_t a = 0u; a < (uint8_t)(sizeof(k_amplitudes) / sizeof(k_amplitudes[0])); a++)
        {
            if (measurement_condition_calibratable(range_id, k_frequencies[f], k_amplitudes[a]))
            {
                if (current == index)
                {
                    *key = measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                                               MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                               range_id,
                                               k_frequencies[f],
                                               k_amplitudes[a]);
                    return BSP_STATUS_OK;
                }
                current++;
            }
        }
    }
    return BSP_STATUS_INVALID_ARG;
}

uint8_t app_calibration_wizard_condition_count(hw_range_id_t range_id)
{
    uint8_t count = 0u;
    for (uint8_t f = 0u; f < (uint8_t)(sizeof(k_frequencies) / sizeof(k_frequencies[0])); f++)
    {
        for (uint8_t a = 0u; a < (uint8_t)(sizeof(k_amplitudes) / sizeof(k_amplitudes[0])); a++)
        {
            if (measurement_condition_calibratable(range_id, k_frequencies[f], k_amplitudes[a]))
            {
                count++;
            }
        }
    }
    return count;
}

uint8_t app_calibration_wizard_total_condition_count(void)
{
    uint8_t total = 0u;
    for (uint8_t r = 0u; r < APP_CAL_WIZARD_RANGE_COUNT; r++)
    {
        total = (uint8_t)(total + app_calibration_wizard_condition_count(range_from_index(r)));
    }
    return total;
}

bsp_status_t app_calibration_wizard_init(app_calibration_wizard_t *wizard,
                                         app_calibration_service_t *service,
                                         const app_cal_session_io_t *io,
                                         const app_cal_fixture_profile_t *fixture)
{
    if ((wizard == NULL) || (service == NULL) || (io == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *wizard = (app_calibration_wizard_t){0};
    const bsp_status_t status = app_calibration_session_init(&wizard->session, service, io);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    wizard->service = service;
    wizard->fixture = (fixture != NULL) ? *fixture :
        (app_cal_fixture_profile_t){
            .load_z = app_calibration_fixture_profile_default_load,
            .user = NULL,
        };
    if (wizard->fixture.load_z == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    wizard->state = APP_CAL_WIZARD_IDLE;
    wizard->state_before_safety = APP_CAL_WIZARD_IDLE;
    wizard->total_conditions = app_calibration_wizard_total_condition_count();
    wizard->initialized = true;
    return BSP_STATUS_OK;
}

bsp_status_t app_calibration_wizard_start(app_calibration_wizard_t *wizard,
                                          app_cal_wizard_mode_t mode,
                                          uint32_t sequence,
                                          int32_t temperature_mC,
                                          bool temperature_valid)
{
    if ((wizard == NULL) || !wizard->initialized)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (app_calibration_wizard_active(wizard))
    {
        return BSP_STATUS_BUSY;
    }
    const bsp_status_t status = app_calibration_service_candidate_begin(wizard->service);
    if (status != BSP_STATUS_OK)
    {
        wizard->error = (status == BSP_STATUS_BUSY) ? APP_CAL_WIZARD_ERROR_CANDIDATE_BUSY :
                                                      APP_CAL_WIZARD_ERROR_STORAGE;
        wizard->state = APP_CAL_WIZARD_FAILED;
        return status;
    }
    wizard->mode = mode;
    wizard->sequence = sequence;
    wizard->temperature_mC = temperature_mC;
    wizard->temperature_valid = temperature_valid;
    wizard->range_index = 0u;
    wizard->condition_index = 0u;
    wizard->condition_count = 0u;
    wizard->solved_count = 0u;
    wizard->error = APP_CAL_WIZARD_ERROR_NONE;
    wizard->workflow_result = APP_CAL_WORKFLOW_RESULT_NONE;
    wizard->solver_status = MEASUREMENT_CAL_SOLVER_OK;
    wizard->commit_started = false;
    clear_range_cache(wizard);
    wizard->state = APP_CAL_WIZARD_INTRO;
    return BSP_STATUS_OK;
}

void app_calibration_wizard_confirm(app_calibration_wizard_t *wizard)
{
    if ((wizard == NULL) || !wizard->initialized)
    {
        return;
    }
    switch (wizard->state)
    {
    case APP_CAL_WIZARD_INTRO:
        begin_range(wizard);
        break;
    case APP_CAL_WIZARD_WAIT_OPEN_FIXTURE:
    case APP_CAL_WIZARD_WAIT_SHORT_FIXTURE:
    case APP_CAL_WIZARD_WAIT_LOAD_FIXTURE:
        wizard->state = capture_state_for_standard(wizard->standard);
        break;
    case APP_CAL_WIZARD_CONFIRM_SAVE:
        if (app_calibration_service_candidate_commit_start(wizard->service) == BSP_STATUS_BUSY)
        {
            wizard->commit_started = true;
            wizard->state = APP_CAL_WIZARD_COMMITTING;
        }
        else
        {
            enter_failure(wizard, APP_CAL_WIZARD_ERROR_COMMIT);
        }
        break;
    case APP_CAL_WIZARD_FAILED:
        if (wizard->error == APP_CAL_WIZARD_ERROR_COMMIT)
        {
            if (app_calibration_service_candidate_commit_start(wizard->service) == BSP_STATUS_BUSY)
            {
                wizard->commit_started = true;
                wizard->state = APP_CAL_WIZARD_COMMITTING;
            }
        }
        else
        {
            wizard->state = capture_state_for_standard(wizard->standard);
        }
        break;
    default:
        break;
    }
}

bsp_status_t app_calibration_wizard_cancel(app_calibration_wizard_t *wizard)
{
    if ((wizard == NULL) || !wizard->initialized)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (wizard->state == APP_CAL_WIZARD_COMMITTING)
    {
        return BSP_STATUS_BUSY;
    }
    if (app_calibration_session_active(&wizard->session))
    {
        const bsp_status_t status = app_calibration_session_cancel(&wizard->session);
        wizard->state = APP_CAL_WIZARD_CANCELING;
        return status;
    }
    const bsp_status_t discard = app_calibration_service_candidate_discard(wizard->service);
    wizard->error = (discard == BSP_STATUS_OK) ? APP_CAL_WIZARD_ERROR_CANCELED :
                                                 APP_CAL_WIZARD_ERROR_STORAGE;
    wizard->state = (discard == BSP_STATUS_OK) ? APP_CAL_WIZARD_CANCELED :
                                                 APP_CAL_WIZARD_FAILED;
    return discard;
}

void app_calibration_wizard_step(app_calibration_wizard_t *wizard,
                                 const hw_safety_result_t *safety,
                                 const bsp_clock_summary_t *clock_summary,
                                 bsp_status_t clock_status,
                                 uint32_t now_ms)
{
    if ((wizard == NULL) || !wizard->initialized)
    {
        return;
    }
    if (wizard->state == APP_CAL_WIZARD_SAFETY_BLOCKED)
    {
        if (safety_allows_capture(safety))
        {
            wizard->state = wizard->state_before_safety;
        }
        else
        {
            return;
        }
    }
    if (wizard->state == APP_CAL_WIZARD_RANGE_COMPLETE)
    {
        wizard->range_index++;
        if (wizard->range_index >= APP_CAL_WIZARD_RANGE_COUNT)
        {
            wizard->state = validate_complete_candidate(wizard) ?
                                APP_CAL_WIZARD_CONFIRM_SAVE :
                                APP_CAL_WIZARD_FAILED;
            if (wizard->state == APP_CAL_WIZARD_FAILED)
            {
                wizard->error = APP_CAL_WIZARD_ERROR_CANDIDATE_INCOMPLETE;
            }
        }
        else
        {
            begin_range(wizard);
        }
        return;
    }
    if (wizard->state == APP_CAL_WIZARD_COMMITTING)
    {
        const bsp_status_t status = app_calibration_service_step(wizard->service, now_ms);
        if ((status != BSP_STATUS_OK) && (status != BSP_STATUS_BUSY))
        {
            enter_failure(wizard, APP_CAL_WIZARD_ERROR_COMMIT);
            return;
        }
        if (app_calibration_service_active_valid(wizard->service) &&
            (app_calibration_service_candidate_state(wizard->service) == APP_CAL_CANDIDATE_ACTIVATED))
        {
            wizard->state = APP_CAL_WIZARD_COMPLETE;
        }
        return;
    }
    if (wizard->state == APP_CAL_WIZARD_CANCELING)
    {
        const app_cal_session_event_t event = app_calibration_session_step(&wizard->session, now_ms);
        if ((event == APP_CAL_SESSION_EVENT_CANCELED) ||
            !app_calibration_session_active(&wizard->session))
        {
            (void)app_calibration_service_candidate_discard(wizard->service);
            wizard->state = APP_CAL_WIZARD_CANCELED;
            wizard->error = APP_CAL_WIZARD_ERROR_CANCELED;
        }
        return;
    }
    if (!state_is_capture(wizard->state))
    {
        return;
    }
    if (!app_calibration_session_active(&wizard->session))
    {
        (void)start_current_condition(wizard, safety, clock_summary, clock_status, now_ms);
        return;
    }

    const app_cal_session_event_t event = app_calibration_session_step(&wizard->session, now_ms);
    const app_cal_evidence_t *evidence = app_calibration_session_evidence(&wizard->session);
    if (evidence != NULL)
    {
        wizard->accepted = evidence->accepted;
        wizard->attempts = evidence->attempts;
    }
    if ((event == APP_CAL_SESSION_EVENT_FAILED) || (event == APP_CAL_SESSION_EVENT_ERROR))
    {
        wizard->workflow_result =
            app_calibration_workflow_result(app_calibration_service_workflow_const(wizard->service));
        enter_failure(wizard, APP_CAL_WIZARD_ERROR_PHASE05);
        return;
    }
    if (event != APP_CAL_SESSION_EVENT_COMPLETE)
    {
        return;
    }

    measurement_cal_solver_standard_t standard;
    if (app_calibration_standard_from_evidence(evidence, &standard) != BSP_STATUS_OK)
    {
        enter_failure(wizard, APP_CAL_WIZARD_ERROR_CONDITION);
        return;
    }
    standard.standard = solver_standard_type(wizard->standard);
    if ((wizard->standard == APP_CAL_STANDARD_OPEN) || (wizard->standard == APP_CAL_STANDARD_SHORT))
    {
        if (cache_standard(wizard, &standard) != BSP_STATUS_OK)
        {
            enter_failure(wizard, APP_CAL_WIZARD_ERROR_CONDITION);
            return;
        }
    }
    else if (solve_load_condition(wizard, &standard) != BSP_STATUS_OK)
    {
        enter_failure(wizard, APP_CAL_WIZARD_ERROR_SOLVER);
        return;
    }
    advance_after_condition(wizard);
}

bool app_calibration_wizard_active(const app_calibration_wizard_t *wizard)
{
    return (wizard != NULL) &&
           (wizard->state != APP_CAL_WIZARD_IDLE) &&
           !app_calibration_wizard_terminal(wizard);
}

bool app_calibration_wizard_terminal(const app_calibration_wizard_t *wizard)
{
    return (wizard != NULL) &&
           ((wizard->state == APP_CAL_WIZARD_COMPLETE) ||
            (wizard->state == APP_CAL_WIZARD_FAILED) ||
            (wizard->state == APP_CAL_WIZARD_CANCELED));
}

void app_calibration_wizard_snapshot(const app_calibration_wizard_t *wizard,
                                     app_cal_wizard_snapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }
    *snapshot = (app_cal_wizard_snapshot_t){0};
    if (wizard == NULL)
    {
        return;
    }
    measurement_cal_key_t key = {0};
    (void)current_key(wizard, &key);
    snapshot->state = wizard->state;
    snapshot->mode = wizard->mode;
    snapshot->standard = wizard->standard;
    snapshot->error = wizard->error;
    snapshot->workflow_result = wizard->workflow_result;
    snapshot->solver_status = wizard->solver_status;
    snapshot->range_id = key.range_id;
    snapshot->range_index = wizard->range_index;
    snapshot->range_count = APP_CAL_WIZARD_RANGE_COUNT;
    snapshot->condition_index = wizard->condition_index;
    snapshot->condition_count = wizard->condition_count;
    snapshot->solved_count = wizard->solved_count;
    snapshot->total_conditions = wizard->total_conditions;
    snapshot->accepted = wizard->accepted;
    snapshot->attempts = wizard->attempts;
    snapshot->reject_flags = app_calibration_session_last_reject_flags(&wizard->session);
    snapshot->frequency = key.frequency;
    snapshot->amplitude = key.amplitude;
    snapshot->mandatory = wizard->mode == APP_CAL_WIZARD_MODE_MANDATORY;
}

uint32_t app_calibration_wizard_context_size_bytes(void)
{
    return (uint32_t)sizeof(app_calibration_wizard_t);
}

const char *app_cal_wizard_state_string(app_cal_wizard_state_t state)
{
    switch (state)
    {
    case APP_CAL_WIZARD_IDLE:
        return "IDLE";
    case APP_CAL_WIZARD_INTRO:
        return "INTRO";
    case APP_CAL_WIZARD_WAIT_OPEN_FIXTURE:
        return "WAIT_OPEN_FIXTURE";
    case APP_CAL_WIZARD_CAPTURE_OPEN:
        return "CAPTURE_OPEN";
    case APP_CAL_WIZARD_WAIT_SHORT_FIXTURE:
        return "WAIT_SHORT_FIXTURE";
    case APP_CAL_WIZARD_CAPTURE_SHORT:
        return "CAPTURE_SHORT";
    case APP_CAL_WIZARD_WAIT_LOAD_FIXTURE:
        return "WAIT_LOAD_FIXTURE";
    case APP_CAL_WIZARD_CAPTURE_LOAD:
        return "CAPTURE_LOAD";
    case APP_CAL_WIZARD_RANGE_COMPLETE:
        return "RANGE_COMPLETE";
    case APP_CAL_WIZARD_CONFIRM_SAVE:
        return "CONFIRM_SAVE";
    case APP_CAL_WIZARD_COMMITTING:
        return "COMMITTING";
    case APP_CAL_WIZARD_COMPLETE:
        return "COMPLETE";
    case APP_CAL_WIZARD_FAILED:
        return "FAILED";
    case APP_CAL_WIZARD_SAFETY_BLOCKED:
        return "SAFETY_BLOCKED";
    case APP_CAL_WIZARD_CANCELING:
        return "CANCELING";
    case APP_CAL_WIZARD_CANCELED:
    default:
        return "CANCELED";
    }
}

const char *app_cal_wizard_error_string(app_cal_wizard_error_t error)
{
    switch (error)
    {
    case APP_CAL_WIZARD_ERROR_NONE:
        return "NONE";
    case APP_CAL_WIZARD_ERROR_INVALID_ARG:
        return "INVALID_ARG";
    case APP_CAL_WIZARD_ERROR_STORAGE:
        return "STORAGE";
    case APP_CAL_WIZARD_ERROR_CANDIDATE_BUSY:
        return "CANDIDATE_BUSY";
    case APP_CAL_WIZARD_ERROR_CONDITION:
        return "CONDITION";
    case APP_CAL_WIZARD_ERROR_PHASE05:
        return "PHASE05";
    case APP_CAL_WIZARD_ERROR_SOLVER:
        return "SOLVER";
    case APP_CAL_WIZARD_ERROR_CANDIDATE_INCOMPLETE:
        return "CANDIDATE_INCOMPLETE";
    case APP_CAL_WIZARD_ERROR_COMMIT:
        return "COMMIT";
    case APP_CAL_WIZARD_ERROR_CANCELED:
    default:
        return "CANCELED";
    }
}
