#include "app/app_measurement_session.h"

#include <stddef.h>

static bool io_ready(const app_measurement_session_io_t *io)
{
    return (io != NULL) &&
           (io->start_attempt != NULL) &&
           (io->step_attempt != NULL) &&
           (io->attempt_active != NULL) &&
           (io->attempt_done != NULL) &&
           (io->attempt_dumpable != NULL) &&
           (io->attempt_block != NULL) &&
           (io->attempt_error != NULL) &&
           (io->attempt_acknowledge != NULL) &&
           (io->attempt_abort != NULL) &&
           (io->process_block != NULL);
}

static float max_float(float a, float b)
{
    return (a > b) ? a : b;
}

static float selected_return_peak(const measurement_result_t *dsp)
{
    if (dsp == NULL)
    {
        return 0.0f;
    }
    return (dsp->selected_channel == MEASUREMENT_RETURN_HG) ? dsp->ret_hg_quality.signal_peak_v :
                                                              dsp->ret_1x_quality.signal_peak_v;
}

static float range_zref_mag(hw_range_id_t range_id)
{
    const measurement_dsp_config_t config = measurement_dsp_config_ideal(range_id);
    return measurement_complex_mag(config.zref_ohms);
}

static bool is_safety_abort_error(hw_metrology_measure_error_t error)
{
    return (error == HW_METROLOGY_MEASURE_ERR_PERMIT) ||
           (error == HW_METROLOGY_MEASURE_ERR_ABORT);
}

static void store_policy_event(app_measurement_session_t *session,
                               measurement_auto_event_t event)
{
    if (session == NULL)
    {
        return;
    }
    session->pending_policy_event = event;
    if (event == MEASUREMENT_AUTO_EVENT_PARTIAL_RESULT)
    {
        const measurement_session_result_t *partial = measurement_auto_last_result(&session->policy);
        if (partial != NULL)
        {
            session->have_partial = true;
        }
        session->pending_event = APP_MEASUREMENT_EVENT_PARTIAL_RESULT;
    }
    else if (event == MEASUREMENT_AUTO_EVENT_FINAL_RESULT)
    {
        const measurement_session_result_t *final = measurement_auto_last_result(&session->policy);
        if (final != NULL)
        {
            session->have_final = true;
        }
        session->state = APP_MEASUREMENT_SESSION_DONE;
        session->pending_event = APP_MEASUREMENT_EVENT_FINAL_RESULT;
    }
}

static void submit_phase05_failure(app_measurement_session_t *session,
                                   hw_metrology_measure_error_t error,
                                   bool canceled)
{
    measurement_attempt_result_t result =
        app_measurement_attempt_from_dsp(&session->current_attempt, NULL, true, is_safety_abort_error(error), canceled);
    if (canceled)
    {
        result.safety_abort = false;
    }
    store_policy_event(session, measurement_auto_submit_result(&session->policy, &result));
}

static void make_request(const app_measurement_session_t *session,
                         hw_metrology_measure_request_t *request)
{
    if ((session == NULL) || (request == NULL))
    {
        return;
    }
    *request = (hw_metrology_measure_request_t){
        .clock_summary = session->clock_summary,
        .clock_init_status = session->clock_status,
        .frequency = session->current_attempt.frequency,
        .amplitude = session->current_attempt.amplitude,
        .range_id = session->current_attempt.range_id,
    };
}

bsp_status_t app_measurement_session_init(app_measurement_session_t *session,
                                          const app_measurement_session_io_t *io)
{
    if ((session == NULL) || !io_ready(io))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *session = (app_measurement_session_t){0};
    session->io = *io;
    session->state = APP_MEASUREMENT_SESSION_IDLE;
    measurement_auto_session_init(&session->policy);
    return BSP_STATUS_OK;
}

bsp_status_t app_measurement_session_start(app_measurement_session_t *session,
                                           measurement_auto_mode_t mode,
                                           uint32_t sequence,
                                           measurement_qualification_t qualification,
                                           const measurement_auto_hint_t *hint,
                                           const bsp_clock_summary_t *clock_summary,
                                           bsp_status_t clock_status,
                                           uint32_t now_ms)
{
    (void)now_ms;
    if ((session == NULL) || app_measurement_session_active(session))
    {
        return BSP_STATUS_BUSY;
    }
    measurement_auto_start_t start = {
        .mode = mode,
        .qualification = qualification,
        .session_sequence = sequence,
        .hint = (hint != NULL) ? *hint : (measurement_auto_hint_t){0},
    };
    if (!measurement_auto_start_session(&session->policy, &start))
    {
        return BSP_STATUS_ERROR;
    }
    session->clock_summary = clock_summary;
    session->clock_status = clock_status;
    session->mode = mode;
    session->session_sequence = sequence;
    session->have_partial = false;
    session->have_final = false;
    session->cancel_requested = false;
    session->attempt_started = false;
    session->state = APP_MEASUREMENT_SESSION_START_ATTEMPT;
    session->pending_event = APP_MEASUREMENT_EVENT_AUTO_BEGIN;
    return BSP_STATUS_BUSY;
}

static app_measurement_event_t take_pending_event(app_measurement_session_t *session)
{
    const app_measurement_event_t event = session->pending_event;
    session->pending_event = APP_MEASUREMENT_EVENT_NONE;
    return event;
}

app_measurement_event_t app_measurement_session_step(app_measurement_session_t *session,
                                                     uint32_t now_ms)
{
    if (session == NULL)
    {
        return APP_MEASUREMENT_EVENT_ERROR;
    }
    if (session->pending_event != APP_MEASUREMENT_EVENT_NONE)
    {
        return take_pending_event(session);
    }

    switch (session->state)
    {
    case APP_MEASUREMENT_SESSION_IDLE:
    case APP_MEASUREMENT_SESSION_DONE:
        return APP_MEASUREMENT_EVENT_NONE;

    case APP_MEASUREMENT_SESSION_START_ATTEMPT:
    {
        if (session->cancel_requested)
        {
            (void)measurement_auto_cancel(&session->policy);
            store_policy_event(session, MEASUREMENT_AUTO_EVENT_FINAL_RESULT);
            return take_pending_event(session);
        }
        if (!measurement_auto_next_attempt(&session->policy, &session->current_attempt))
        {
            session->state = APP_MEASUREMENT_SESSION_DONE;
            session->pending_event = APP_MEASUREMENT_EVENT_ERROR;
            return take_pending_event(session);
        }
        hw_metrology_measure_request_t request = {0};
        make_request(session, &request);
        const bsp_status_t started = session->io.start_attempt(&request, now_ms, session->io.user);
        if ((started != BSP_STATUS_OK) && (started != BSP_STATUS_BUSY))
        {
            submit_phase05_failure(session, session->io.attempt_error(session->io.user), false);
            return take_pending_event(session);
        }
        session->attempt_started = true;
        session->state = APP_MEASUREMENT_SESSION_WAIT_ATTEMPT;
        session->pending_event = APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN;
        return take_pending_event(session);
    }

    case APP_MEASUREMENT_SESSION_WAIT_ATTEMPT:
    {
        if (session->cancel_requested)
        {
            (void)session->io.attempt_abort(session->io.user);
            session->state = APP_MEASUREMENT_SESSION_CANCELING;
            return APP_MEASUREMENT_EVENT_NONE;
        }
        const bsp_status_t step_status = session->io.step_attempt(now_ms, session->io.user);
        if (session->io.attempt_done(session->io.user))
        {
            session->state = APP_MEASUREMENT_SESSION_PROCESS_DSP;
            return APP_MEASUREMENT_EVENT_NONE;
        }
        if ((step_status != BSP_STATUS_BUSY) &&
            (step_status != BSP_STATUS_OK) &&
            !session->io.attempt_active(session->io.user))
        {
            submit_phase05_failure(session, session->io.attempt_error(session->io.user), false);
            session->io.attempt_acknowledge(session->io.user);
            return take_pending_event(session);
        }
        return APP_MEASUREMENT_EVENT_NONE;
    }

    case APP_MEASUREMENT_SESSION_PROCESS_DSP:
    {
        measurement_attempt_result_t attempt_result;
        if (!session->io.attempt_dumpable(session->io.user))
        {
            submit_phase05_failure(session, session->io.attempt_error(session->io.user), false);
            session->io.attempt_acknowledge(session->io.user);
            return take_pending_event(session);
        }
        const hw_metrology_block_t *block = session->io.attempt_block(session->io.user);
        const bsp_status_t dsp_status =
            session->io.process_block(block, &session->current_attempt, &session->dsp_result, session->io.user);
        attempt_result = app_measurement_attempt_from_dsp(&session->current_attempt,
                                                          &session->dsp_result,
                                                          dsp_status != BSP_STATUS_OK,
                                                          false,
                                                          false);
        session->io.attempt_acknowledge(session->io.user);
        const measurement_auto_event_t policy_event =
            measurement_auto_submit_result(&session->policy, &attempt_result);
        store_policy_event(session, policy_event);
        if (policy_event == MEASUREMENT_AUTO_EVENT_ATTEMPT_READY)
        {
            session->state = APP_MEASUREMENT_SESSION_START_ATTEMPT;
        }
        else if (policy_event == MEASUREMENT_AUTO_EVENT_PARTIAL_RESULT)
        {
            session->state = APP_MEASUREMENT_SESSION_START_ATTEMPT;
        }
        return (session->pending_event != APP_MEASUREMENT_EVENT_NONE) ? take_pending_event(session) :
                                                                       APP_MEASUREMENT_EVENT_NONE;
    }

    case APP_MEASUREMENT_SESSION_CANCELING:
    {
        const bsp_status_t step_status = session->io.step_attempt(now_ms, session->io.user);
        if (session->io.attempt_done(session->io.user) || !session->io.attempt_active(session->io.user) ||
            (step_status == BSP_STATUS_OK))
        {
            session->io.attempt_acknowledge(session->io.user);
            measurement_attempt_result_t canceled =
                app_measurement_attempt_from_dsp(&session->current_attempt, NULL, false, false, true);
            store_policy_event(session, measurement_auto_submit_result(&session->policy, &canceled));
            return take_pending_event(session);
        }
        return APP_MEASUREMENT_EVENT_NONE;
    }

    default:
        session->state = APP_MEASUREMENT_SESSION_DONE;
        session->pending_event = APP_MEASUREMENT_EVENT_ERROR;
        return take_pending_event(session);
    }
}

bsp_status_t app_measurement_session_cancel(app_measurement_session_t *session)
{
    if (session == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if ((session->state == APP_MEASUREMENT_SESSION_IDLE) ||
        (session->state == APP_MEASUREMENT_SESSION_DONE))
    {
        return BSP_STATUS_OK;
    }
    session->cancel_requested = true;
    if (!session->attempt_started || !session->io.attempt_active(session->io.user))
    {
        if (session->io.attempt_done(session->io.user))
        {
            session->io.attempt_acknowledge(session->io.user);
        }
        (void)measurement_auto_cancel(&session->policy);
        store_policy_event(session, MEASUREMENT_AUTO_EVENT_FINAL_RESULT);
        return BSP_STATUS_OK;
    }
    return BSP_STATUS_BUSY;
}

bool app_measurement_session_active(const app_measurement_session_t *session)
{
    return (session != NULL) &&
           (session->state != APP_MEASUREMENT_SESSION_IDLE) &&
           (session->state != APP_MEASUREMENT_SESSION_DONE);
}

bool app_measurement_session_final_ready(const app_measurement_session_t *session)
{
    return (session != NULL) && session->have_final;
}

bool app_measurement_session_partial_ready(const app_measurement_session_t *session)
{
    return (session != NULL) && session->have_partial;
}

const measurement_attempt_config_t *app_measurement_session_current_attempt(
    const app_measurement_session_t *session)
{
    return (session == NULL) ? NULL : &session->current_attempt;
}

const measurement_session_result_t *app_measurement_session_partial(
    const app_measurement_session_t *session)
{
    return ((session != NULL) && session->have_partial) ? measurement_auto_last_result(&session->policy) : NULL;
}

const measurement_session_result_t *app_measurement_session_final(
    const app_measurement_session_t *session)
{
    return ((session != NULL) && session->have_final) ? measurement_auto_last_result(&session->policy) : NULL;
}

app_measurement_session_state_t app_measurement_session_state(
    const app_measurement_session_t *session)
{
    return (session == NULL) ? APP_MEASUREMENT_SESSION_IDLE : session->state;
}

measurement_attempt_result_t app_measurement_attempt_from_dsp(
    const measurement_attempt_config_t *attempt,
    const measurement_calibrated_result_t *processed,
    bool phase05_failed,
    bool safety_abort,
    bool canceled)
{
    measurement_attempt_result_t result = {0};
    const measurement_result_t *dsp = (processed != NULL) ? &processed->result : NULL;
    if (attempt != NULL)
    {
        result.config = *attempt;
    }
    result.phase05_failed = phase05_failed;
    result.safety_abort = safety_abort;
    result.canceled = canceled;
    result.dsp_status = (dsp != NULL) ? dsp->status : MEASUREMENT_STATUS_INVALID_ARG;
    result.calibration = (measurement_calibration_provenance_t){
        .source = MEASUREMENT_CAL_SOURCE_IDEAL,
        .status = MEASUREMENT_CAL_RESOLVE_MISSING,
        .set_sequence = 0u,
        .model_version = MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
        .condition_id = 0u,
        .uncalibrated = true,
    };
    if (processed != NULL)
    {
        result.calibration = processed->provenance;
    }
    if (dsp != NULL)
    {
        result.z_ohms = dsp->impedance.z_ohms;
        result.derived = dsp->derived;
        result.ret_1x_quality = dsp->ret_1x_quality;
        result.ret_hg_quality = dsp->ret_hg_quality;
        result.selected_channel = dsp->selected_channel;
        result.open_like = dsp->impedance.open_like;
        result.short_like = dsp->impedance.short_like;
        result.clipped = dsp->phasors.clipped ||
                         dsp->ret_1x_quality.clipped ||
                         dsp->ret_hg_quality.clipped ||
                         (dsp->status == MEASUREMENT_STATUS_CLIPPED);
        result.source_peak_v = max_float(dsp->phasors.vexc_1_peak_v, dsp->phasors.vexc_2_peak_v);
        result.return_peak_v = selected_return_peak(dsp);
        const measurement_complex_t denominator =
            measurement_complex_sub(dsp->impedance.vs_v, dsp->impedance.vx_v);
        result.denominator_peak_v = measurement_complex_mag(denominator);
        result.dut_zref_ratio =
            measurement_complex_mag(result.z_ohms) /
            max_float(range_zref_mag(result.config.range_id), 1.0e-6f);
        if (processed == NULL)
        {
            const measurement_cal_key_t cal_key =
                measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                                    MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                    result.config.range_id,
                                    result.config.frequency,
                                    result.config.amplitude);
            result.calibration.condition_id = measurement_cal_condition_id(&cal_key);
        }
    }
    result.ret_evidence = measurement_auto_evaluate_ret_evidence(&result);
    return result;
}

const char *app_measurement_event_string(app_measurement_event_t event)
{
    switch (event)
    {
    case APP_MEASUREMENT_EVENT_AUTO_BEGIN:
        return "AUTO_BEGIN";
    case APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN:
        return "ATTEMPT_BEGIN";
    case APP_MEASUREMENT_EVENT_PARTIAL_RESULT:
        return "PARTIAL_RESULT";
    case APP_MEASUREMENT_EVENT_FINAL_RESULT:
        return "FINAL_RESULT";
    case APP_MEASUREMENT_EVENT_ERROR:
        return "ERROR";
    case APP_MEASUREMENT_EVENT_NONE:
    default:
        return "NONE";
    }
}

const char *app_measurement_session_state_string(app_measurement_session_state_t state)
{
    switch (state)
    {
    case APP_MEASUREMENT_SESSION_IDLE:
        return "IDLE";
    case APP_MEASUREMENT_SESSION_START_ATTEMPT:
        return "START_ATTEMPT";
    case APP_MEASUREMENT_SESSION_WAIT_ATTEMPT:
        return "WAIT_ATTEMPT";
    case APP_MEASUREMENT_SESSION_PROCESS_DSP:
        return "PROCESS_DSP";
    case APP_MEASUREMENT_SESSION_DONE:
        return "DONE";
    case APP_MEASUREMENT_SESSION_CANCELING:
    default:
        return "CANCELING";
    }
}

uint32_t app_measurement_session_size_bytes(void)
{
    return (uint32_t)sizeof(app_measurement_session_t);
}
