#include "app/app_measurement_session.h"

#include <stddef.h>
#include <math.h>
#include "wtk_build_config.h"

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
           (io->process_block != NULL)
#if WTK_ENABLE_FREQUENCY_SURVEY
           && (!io->frequency_survey_enabled || io->finish_session != NULL)
#if WTK_ENABLE_CONTINUOUS_SURVEY
           && (!io->continuous_survey_enabled || io->frequency_survey_enabled)
#endif
#endif
           ;
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

#if WTK_ENABLE_MEASUREMENT_TRIMMED
static bool trim_sample_usable(const measurement_attempt_result_t *point,
                               const measurement_attempt_result_t *primary)
{
    const measurement_channel_quality_t *quality = point->selected_channel == MEASUREMENT_RETURN_HG ?
        &point->ret_hg_quality : &point->ret_1x_quality;
    const measurement_cal_key_t key = measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
        MEASUREMENT_CAL_MODEL_VERSION_CURRENT, point->config.range_id,
        point->config.frequency, point->config.amplitude);
    return point->dsp_status == MEASUREMENT_STATUS_OK && point->derived.valid &&
        !point->phase05_failed && !point->safety_abort && !point->canceled &&
        !point->clipped && !point->open_like && !point->short_like &&
        measurement_complex_is_finite(point->z_ohms) &&
        quality->usable && quality->calibration_valid && !quality->clipped &&
        isfinite(quality->signal_peak_v) && quality->signal_peak_v >= 0.002f &&
        isfinite(point->source_peak_v) && isfinite(point->denominator_peak_v) &&
        point->source_peak_v > 0.0f && point->denominator_peak_v > 0.0f &&
        (point->selected_channel == MEASUREMENT_RETURN_1X || point->selected_channel == MEASUREMENT_RETURN_HG) &&
        point->selected_channel == primary->selected_channel &&
        point->config.range_id == primary->config.range_id &&
        point->config.frequency == primary->config.frequency &&
        point->config.amplitude == primary->config.amplitude &&
        point->calibration.source == MEASUREMENT_CAL_SOURCE_PERSISTED &&
        (point->calibration.status == MEASUREMENT_CAL_RESOLVE_FOUND ||
         point->calibration.status == MEASUREMENT_CAL_RESOLVE_UNQUALIFIED) &&
        point->calibration.model_version == MEASUREMENT_CAL_MODEL_VERSION_CURRENT &&
        point->calibration.condition_id == measurement_cal_condition_id(&key) &&
        /* The engineering set has applied persisted coefficients but remains
         * UNQUALIFIED/uncalibrated until physical accuracy qualification. */
        point->calibration.uncalibrated == primary->calibration.uncalibrated &&
        point->calibration.source == primary->calibration.source &&
        point->calibration.status == primary->calibration.status &&
        point->calibration.set_sequence == primary->calibration.set_sequence &&
        point->calibration.lead_compensated == primary->calibration.lead_compensated &&
        point->calibration.lead_sequence == primary->calibration.lead_sequence &&
        point->calibration.model_version == primary->calibration.model_version &&
        point->calibration.condition_id == primary->calibration.condition_id &&
        point->supplementary_applied == primary->supplementary_applied;
}

static void trim_record_confidence(app_measurement_session_t *session,
                                   const measurement_attempt_result_t *point)
{
    measurement_confidence_result_t *confidence = &session->policy.last_result.confidence;
    const measurement_confidence_result_t sample =
        measurement_auto_evaluate_confidence(point, session->policy.qualification);
    if (sample.measurement_quality > confidence->measurement_quality)
        confidence->measurement_quality = sample.measurement_quality;
    if (sample.publication_confidence > confidence->publication_confidence)
        confidence->publication_confidence = sample.publication_confidence;
    confidence->reason_flags |= sample.reason_flags;
    confidence->mathematically_valid = confidence->mathematically_valid && sample.mathematically_valid;
}

static bool begin_trimming(app_measurement_session_t *session)
{
    measurement_session_result_t *final = &session->policy.last_result;
    if (!session->io->trimmed_enabled || session->io->finish_session == NULL ||
        session->trimmed.summary.requested || final->status != MEASUREMENT_AUTO_STATUS_FINAL_OK)
        return false;
    session->trimmed.summary.requested = true;
    if ((!session->io->attempt_active(session->io->user)
#if WTK_ENABLE_FREQUENCY_SURVEY
         && !session->policy.frequency_survey
#endif
        ) ||
        !trim_sample_usable(&final->primary_attempt, &final->primary_attempt))
    {
        (void)measurement_auto_stop_session(&session->policy, MEASUREMENT_AUTO_STATUS_FAILED);
        return false;
    }
    (void)measurement_trimmed_add(&session->trimmed, final->primary_attempt.z_ohms);
    session->current_attempt = final->primary_attempt.config;
    session->trimming = true;
    session->have_partial = false;
    session->have_final = false;
    session->pending_event = APP_MEASUREMENT_EVENT_NONE;
    session->state = APP_MEASUREMENT_SESSION_START_ATTEMPT;
    return true;
}

static bool trim_submit(app_measurement_session_t *session, const measurement_attempt_result_t *point)
{
    measurement_session_result_t *final = &session->policy.last_result;
    session->trimmed.summary.extra_captures++;
    if (trim_sample_usable(point, &final->primary_attempt))
    {
        (void)measurement_trimmed_add(&session->trimmed, point->z_ohms);
        trim_record_confidence(session, point);
        measurement_attempt_result_t *primary = &final->primary_attempt;
        if (point->source_peak_v < primary->source_peak_v) primary->source_peak_v = point->source_peak_v;
        if (point->return_peak_v < primary->return_peak_v) primary->return_peak_v = point->return_peak_v;
        if (point->denominator_peak_v < primary->denominator_peak_v)
            primary->denominator_peak_v = point->denominator_peak_v;
        measurement_channel_quality_t *quality = primary->selected_channel == MEASUREMENT_RETURN_HG ?
            &primary->ret_hg_quality : &primary->ret_1x_quality;
        if (point->return_peak_v < quality->signal_peak_v) quality->signal_peak_v = point->return_peak_v;
        primary->ret_evidence = measurement_auto_evaluate_ret_evidence(primary);
    }
    if (session->trimmed.summary.samples < MEASUREMENT_TRIMMED_SAMPLES &&
        session->trimmed.summary.extra_captures < MEASUREMENT_TRIMMED_MAX_EXTRA_CAPTURES)
    {
        session->state = APP_MEASUREMENT_SESSION_START_ATTEMPT;
        return false;
    }
    session->trimming = false;
    if (!measurement_trimmed_finish(&session->trimmed))
    {
        (void)measurement_auto_stop_session(&session->policy, MEASUREMENT_AUTO_STATUS_FAILED);
        return true;
    }
    measurement_attempt_result_t *primary = &final->primary_attempt;
    const measurement_interpretation_t original_local = primary->derived.interpretation;
    primary->z_ohms = session->trimmed.estimate;
    const measurement_dsp_config_t config = measurement_dsp_config_ideal(primary->config.range_id);
    const uint32_t hz = hw_excitation_frequency_hz(primary->config.frequency);
    primary->derived = measurement_derive_quantities(primary->z_ohms, hz, &config, primary->dsp_status);
    primary->dut_zref_ratio = measurement_complex_mag(primary->z_ohms) / measurement_complex_mag(config.zref_ohms);
    trim_record_confidence(session, primary);
    /* Secondary-frequency evidence is preserved, including an unresolved
     * classification. An aggregate with contrary sign cannot assert C or L. */
    if ((final->classification.interpretation == MEASUREMENT_INTERPRET_CAPACITIVE && primary->z_ohms.im >= 0.0f) ||
        (final->classification.interpretation == MEASUREMENT_INTERPRET_INDUCTIVE && primary->z_ohms.im <= 0.0f) ||
        (primary->derived.interpretation != original_local &&
         primary->derived.interpretation != MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN))
    {
        final->classification.interpretation = MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN;
        final->classification.reason_flags |= MEASUREMENT_AUTO_REASON_INCONSISTENT;
        final->confidence.reason_flags |= MEASUREMENT_AUTO_REASON_INCONSISTENT;
    }
    return true;
}
#endif

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
#if WTK_ENABLE_MEASUREMENT_TRIMMED
        if (begin_trimming(session)) return;
        session->policy.last_result.trimmed = session->trimmed.summary;
        if (session->policy.last_result.status != MEASUREMENT_AUTO_STATUS_FINAL_OK)
            session->policy.last_result.trimmed.applied = false;
#endif
#if WTK_ENABLE_MEASURE_SESSION_HOLD
        if ((session->io->finish_session != NULL) && session->io->attempt_active(session->io->user))
        {
            session->have_partial = false;
            session->have_final = false;
            session->pending_event = APP_MEASUREMENT_EVENT_NONE;
            session->state = APP_MEASUREMENT_SESSION_FINISHING;
            return;
        }
#endif
        const measurement_session_result_t *final = measurement_auto_last_result(&session->policy);
        if (final != NULL)
        {
            session->have_final = true;
        }
        app_waveform_publish(session->io->waveform_store, final);
        session->state = APP_MEASUREMENT_SESSION_DONE;
        session->pending_event = APP_MEASUREMENT_EVENT_FINAL_RESULT;
    }
}

static void submit_phase05_failure(app_measurement_session_t *session,
                                   hw_metrology_measure_error_t error,
                                   bool canceled)
{
#if WTK_ENABLE_MEASUREMENT_TRIMMED
    if (session->trimming)
    {
        session->trimming = false;
        (void)session->io->attempt_abort(session->io->user);
        store_policy_event(session, measurement_auto_stop_session(&session->policy,
            is_safety_abort_error(error) ? MEASUREMENT_AUTO_STATUS_SAFETY_ABORT : MEASUREMENT_AUTO_STATUS_FAILED));
        return;
    }
#endif
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
#if WTK_ENABLE_MEASURE_SESSION_HOLD
        .session_hold = session->io->finish_session != NULL,
#if WTK_ENABLE_CONTINUOUS_SURVEY
        .continuous_survey = session->policy.frequency_survey && session->io->continuous_survey_enabled,
#endif
#if WTK_ENABLE_MEASUREMENT_TRIMMED
        /* A fresh segment always enters through ordinary fresh authorization;
         * only already-held extra windows use the statistical continuation. */
        .statistical_capture = session->trimming && session->io->attempt_active(session->io->user),
#endif
#endif
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
    session->io = io;
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
#if WTK_ENABLE_FREQUENCY_SURVEY
        .frequency_survey = session->io->frequency_survey_enabled,
#endif
    };
    if (!measurement_auto_start_session(&session->policy, &start))
    {
        return BSP_STATUS_ERROR;
    }
    app_waveform_invalidate(session->io->waveform_store);
    session->clock_summary = clock_summary;
    session->clock_status = clock_status;
    session->mode = mode;
    session->session_sequence = sequence;
    session->have_partial = false;
    session->have_final = false;
    session->cancel_requested = false;
    session->attempt_started = false;
#if WTK_ENABLE_MEASUREMENT_TRIMMED
    measurement_trimmed_init(&session->trimmed);
    session->trimming = false;
#if WTK_ENABLE_FREQUENCY_SURVEY
    session->survey_deadline_ms = now_ms + 60000u;
    session->segment_captures = 0u;
#endif
#endif
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
#if WTK_ENABLE_FREQUENCY_SURVEY
    if (session->policy.frequency_survey && app_measurement_session_active(session) &&
        (int32_t)(now_ms - session->survey_deadline_ms) >= 0 &&
        session->state != APP_MEASUREMENT_SESSION_CANCELING && session->state != APP_MEASUREMENT_SESSION_FINISHING)
    {
        (void)session->io->attempt_abort(session->io->user);
        session->pending_event = APP_MEASUREMENT_EVENT_NONE;
        session->have_partial = false;
        session->state = APP_MEASUREMENT_SESSION_CANCELING;
    }
#endif
#if WTK_ENABLE_MEASURE_SESSION_HOLD
    /* Held results remain under the hardware owner's supervision while DSP,
     * policy events and the next-condition handoff run in the foreground. */
    if ((session->io != NULL) && (session->io->finish_session != NULL) &&
        ((session->state == APP_MEASUREMENT_SESSION_PROCESS_DSP) ||
         (session->state == APP_MEASUREMENT_SESSION_START_ATTEMPT)) &&
        session->attempt_started && session->io->attempt_active(session->io->user))
    {
        const bsp_status_t status = session->io->step_attempt(now_ms, session->io->user);
        if (status != BSP_STATUS_OK && status != BSP_STATUS_BUSY)
        {
            session->pending_event = APP_MEASUREMENT_EVENT_NONE;
            session->have_partial = false;
            session->state = APP_MEASUREMENT_SESSION_CANCELING;
        }
    }
#endif
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
#if WTK_ENABLE_MEASURE_SESSION_HOLD
            if (session->io->finish_session != NULL)
                (void)measurement_auto_stop_session(&session->policy, MEASUREMENT_AUTO_STATUS_CANCELED);
            else
#endif
            (void)measurement_auto_cancel(&session->policy);
            store_policy_event(session, MEASUREMENT_AUTO_EVENT_FINAL_RESULT);
            return take_pending_event(session);
        }
        if (
#if WTK_ENABLE_MEASUREMENT_TRIMMED
            !session->trimming &&
#endif
            !measurement_auto_next_attempt(&session->policy, &session->current_attempt))
        {
#if WTK_ENABLE_MEASURE_SESSION_HOLD
            if (session->io->finish_session != NULL && session->io->attempt_active(session->io->user))
            {
                (void)session->io->attempt_abort(session->io->user);
                session->state = APP_MEASUREMENT_SESSION_CANCELING;
                return APP_MEASUREMENT_EVENT_NONE;
            }
#endif
            session->state = APP_MEASUREMENT_SESSION_DONE;
            session->pending_event = APP_MEASUREMENT_EVENT_ERROR;
            return take_pending_event(session);
        }
        hw_metrology_measure_request_t request = {0};
        make_request(session, &request);
#if WTK_ENABLE_FREQUENCY_SURVEY
        if (session->policy.frequency_survey && session->io->attempt_active(session->io->user) &&
#if WTK_ENABLE_CONTINUOUS_SURVEY
            !session->io->continuous_survey_enabled &&
#endif
            (request.frequency == HW_EXCITATION_FREQ_10HZ ||
             (!session->trimming && session->segment_captures >= HW_METROLOGY_SESSION_MAX_CAPTURES)))
        {
            session->state = APP_MEASUREMENT_SESSION_DRAINING;
            return APP_MEASUREMENT_EVENT_NONE;
        }
        if (!session->io->attempt_active(session->io->user)) session->segment_captures = 0u;
#endif
        const bsp_status_t started = session->io->start_attempt(&request, now_ms, session->io->user);
        if ((started != BSP_STATUS_OK) && (started != BSP_STATUS_BUSY))
        {
            submit_phase05_failure(session, session->io->attempt_error(session->io->user), false);
            return take_pending_event(session);
        }
        session->attempt_started = true;
#if WTK_ENABLE_FREQUENCY_SURVEY
        session->segment_captures++;
#endif
        session->state = APP_MEASUREMENT_SESSION_WAIT_ATTEMPT;
        session->pending_event = APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN;
        return take_pending_event(session);
    }

    case APP_MEASUREMENT_SESSION_WAIT_ATTEMPT:
    {
        if (session->cancel_requested)
        {
            (void)session->io->attempt_abort(session->io->user);
            session->state = APP_MEASUREMENT_SESSION_CANCELING;
            return APP_MEASUREMENT_EVENT_NONE;
        }
        const bsp_status_t step_status = session->io->step_attempt(now_ms, session->io->user);
        if (session->io->attempt_done(session->io->user))
        {
            session->state = APP_MEASUREMENT_SESSION_PROCESS_DSP;
            return APP_MEASUREMENT_EVENT_NONE;
        }
        if ((step_status != BSP_STATUS_BUSY) &&
            (step_status != BSP_STATUS_OK) &&
            !session->io->attempt_active(session->io->user))
        {
            submit_phase05_failure(session, session->io->attempt_error(session->io->user), false);
            session->io->attempt_acknowledge(session->io->user);
            return take_pending_event(session);
        }
        return APP_MEASUREMENT_EVENT_NONE;
    }

    case APP_MEASUREMENT_SESSION_PROCESS_DSP:
    {
        if (session->cancel_requested)
        {
            (void)session->io->attempt_abort(session->io->user);
            session->state = APP_MEASUREMENT_SESSION_CANCELING;
            return APP_MEASUREMENT_EVENT_NONE;
        }
        measurement_attempt_result_t attempt_result;
        if (!session->io->attempt_dumpable(session->io->user))
        {
            submit_phase05_failure(session, session->io->attempt_error(session->io->user), false);
            session->io->attempt_acknowledge(session->io->user);
            return take_pending_event(session);
        }
        const hw_metrology_block_t *block = session->io->attempt_block(session->io->user);
#if WTK_ENABLE_FREQUENCY_SURVEY
        if (session->policy.frequency_survey)
            session->dsp_result = (measurement_calibrated_result_t){.result.status = MEASUREMENT_STATUS_INVALID_ARG};
#endif
        const bsp_status_t dsp_status =
            session->io->process_block(block, &session->current_attempt, &session->dsp_result, session->io->user);
#if WTK_ENABLE_FREQUENCY_SURVEY
        if (session->policy.frequency_survey && dsp_status != BSP_STATUS_OK &&
            session->dsp_result.result.status == MEASUREMENT_STATUS_OK)
            session->dsp_result.result.status = MEASUREMENT_STATUS_INVALID_ARG;
#endif
        attempt_result = app_measurement_attempt_from_dsp(&session->current_attempt,
                                                          &session->dsp_result,
                                                          dsp_status != BSP_STATUS_OK,
                                                          false,
                                                          false);
#if WTK_ENABLE_FREQUENCY_SURVEY
        /* The hardware delivered a complete block. DSP rejection (weak return,
         * clipping, near-OPEN or unavailable condition) is evidence for another
         * condition, not an acquisition fault that terminates the survey. */
        if (session->policy.frequency_survey)
            attempt_result.phase05_failed = dsp_status != BSP_STATUS_OK &&
                session->dsp_result.result.status == MEASUREMENT_STATUS_INVALID_ARG;
#endif
#if WTK_ENABLE_MEASUREMENT_TRIMMED
        if (session->trimming)
        {
            /* Copy only an admitted statistical repeat while acquisition still
             * owns the block; acknowledge may overwrite the DMA workspace. */
            if (trim_sample_usable(&attempt_result, &session->policy.last_result.primary_attempt) &&
                session->io->waveform_adc != NULL)
                app_waveform_stage(session->io->waveform_store, block,
                    session->io->waveform_adc(session->io->user), &session->dsp_result,
                    &session->current_attempt, session->session_sequence);
            const bool finished = trim_submit(session, &attempt_result);
            session->io->attempt_acknowledge(session->io->user);
            if (finished)
                store_policy_event(session, MEASUREMENT_AUTO_EVENT_FINAL_RESULT);
            /* A fixed 10-Hz capture is already SAFE: store_policy_event can
             * complete immediately. Deliver its final event before callers
             * stop stepping this now-inactive session. */
            return session->pending_event != APP_MEASUREMENT_EVENT_NONE ? take_pending_event(session) :
                                                                          APP_MEASUREMENT_EVENT_NONE;
        }
#endif
        session->io->attempt_acknowledge(session->io->user);
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
        const bsp_status_t step_status = session->io->step_attempt(now_ms, session->io->user);
        if (session->io->attempt_done(session->io->user) || !session->io->attempt_active(session->io->user) ||
            (step_status == BSP_STATUS_OK))
        {
            session->io->attempt_acknowledge(session->io->user);
#if WTK_ENABLE_MEASURE_SESSION_HOLD
            if (session->io->finish_session != NULL)
            {
                const measurement_auto_status_t terminal = session->cancel_requested ?
                    MEASUREMENT_AUTO_STATUS_CANCELED : MEASUREMENT_AUTO_STATUS_SAFETY_ABORT;
                store_policy_event(session, measurement_auto_stop_session(&session->policy, terminal));
                return take_pending_event(session);
            }
#endif
            measurement_attempt_result_t canceled =
                app_measurement_attempt_from_dsp(&session->current_attempt, NULL, false, false, true);
            store_policy_event(session, measurement_auto_submit_result(&session->policy, &canceled));
            return take_pending_event(session);
        }
        return APP_MEASUREMENT_EVENT_NONE;
    }

#if WTK_ENABLE_MEASURE_SESSION_HOLD
#if WTK_ENABLE_FREQUENCY_SURVEY
    case APP_MEASUREMENT_SESSION_DRAINING:
        (void)session->io->finish_session(session->io->user);
        (void)session->io->step_attempt(now_ms, session->io->user);
        if (!session->io->attempt_active(session->io->user))
        {
            const hw_metrology_measure_error_t error = session->io->attempt_error(session->io->user);
            session->io->attempt_acknowledge(session->io->user);
            session->attempt_started = false;
            session->segment_captures = 0u;
            if (error != HW_METROLOGY_MEASURE_OK || session->cancel_requested)
                store_policy_event(session, measurement_auto_stop_session(&session->policy,
                    session->cancel_requested ? MEASUREMENT_AUTO_STATUS_CANCELED : MEASUREMENT_AUTO_STATUS_SAFETY_ABORT));
            else session->state = APP_MEASUREMENT_SESSION_START_ATTEMPT;
        }
        return session->pending_event != APP_MEASUREMENT_EVENT_NONE ? take_pending_event(session) :
                                                                      APP_MEASUREMENT_EVENT_NONE;
#endif
    case APP_MEASUREMENT_SESSION_FINISHING:
    {
        (void)session->io->finish_session(session->io->user);
        (void)session->io->step_attempt(now_ms, session->io->user);
        if (!session->io->attempt_active(session->io->user))
        {
            const hw_metrology_measure_error_t error = session->io->attempt_error(session->io->user);
            if (session->cancel_requested || error != HW_METROLOGY_MEASURE_OK)
            {
                (void)measurement_auto_stop_session(&session->policy,
                    session->cancel_requested ? MEASUREMENT_AUTO_STATUS_CANCELED :
                                                MEASUREMENT_AUTO_STATUS_SAFETY_ABORT);
            }
            session->io->attempt_acknowledge(session->io->user);
            store_policy_event(session, MEASUREMENT_AUTO_EVENT_FINAL_RESULT);
            return take_pending_event(session);
        }
        return APP_MEASUREMENT_EVENT_NONE;
    }
#endif

    default:
#if WTK_ENABLE_MEASURE_SESSION_HOLD
        if (session->io != NULL && session->io->finish_session != NULL &&
            session->io->attempt_active(session->io->user))
        {
            (void)session->io->attempt_abort(session->io->user);
            session->state = APP_MEASUREMENT_SESSION_CANCELING;
            return APP_MEASUREMENT_EVENT_NONE;
        }
#endif
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
    if (!session->attempt_started || !session->io->attempt_active(session->io->user))
    {
        if (session->io->attempt_done(session->io->user))
        {
            session->io->attempt_acknowledge(session->io->user);
        }
#if WTK_ENABLE_MEASURE_SESSION_HOLD
        if (session->io->finish_session != NULL)
        {
            (void)measurement_auto_stop_session(&session->policy, MEASUREMENT_AUTO_STATUS_CANCELED);
        }
        else
#endif
        {
            (void)measurement_auto_cancel(&session->policy);
        }
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
        result.supplementary_applied = processed->supplementary_applied;
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
#if WTK_PRODUCT_MODERN
        /* The DSP already selected an independently validated acquisition path.
           Clipping on the unused HG path remains in ret_evidence diagnostics. */
        result.clipped = (dsp->selected_channel == MEASUREMENT_RETURN_HG ?
            dsp->ret_hg_quality.clipped : dsp->ret_1x_quality.clipped) ||
            dsp->status == MEASUREMENT_STATUS_CLIPPED;
#else
        result.clipped = dsp->phasors.clipped ||
                         dsp->ret_1x_quality.clipped ||
                         dsp->ret_hg_quality.clipped ||
                         (dsp->status == MEASUREMENT_STATUS_CLIPPED);
#endif
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
#if WTK_ENABLE_MEASURE_SESSION_HOLD
    case APP_MEASUREMENT_SESSION_FINISHING:
        return "FINISHING";
#if WTK_ENABLE_FREQUENCY_SURVEY
    case APP_MEASUREMENT_SESSION_DRAINING:
        return "DRAINING";
#endif
#endif
    case APP_MEASUREMENT_SESSION_CANCELING:
    default:
        return "CANCELING";
    }
}

uint32_t app_measurement_session_size_bytes(void)
{
    return (uint32_t)sizeof(app_measurement_session_t);
}
