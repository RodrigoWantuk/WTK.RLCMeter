#include "measurement/measurement_engine.h"

#include <stddef.h>

#define AUTO_RATIO_TOO_SMALL (0.20f)
#define AUTO_RATIO_TOO_LARGE (5.00f)
#define AUTO_RATIO_OPEN_LIKE (100.0f)
#define AUTO_RET_WEAK_V_PEAK (0.0020f)
#define AUTO_RET_GOOD_V_PEAK (0.0100f)
#define AUTO_REACTIVE_RATIO_MIN (0.25f)
#define AUTO_RESISTIVE_RATIO_MAX (0.10f)

typedef enum
{
    NEXT_DECISION_FINAL = 0,
    NEXT_DECISION_ATTEMPT,
} next_decision_t;

typedef struct
{
    next_decision_t decision;
    measurement_auto_status_t final_status;
    measurement_attempt_config_t attempt;
    measurement_attempt_reason_t continuation_reason;
    bool publish_partial;
} policy_decision_t;

static bool range_index(hw_range_id_t range_id, uint8_t *index)
{
    if (index == NULL)
    {
        return false;
    }
    switch (range_id)
    {
    case HW_RANGE_ID_10R:
        *index = 0u;
        return true;
    case HW_RANGE_ID_100R:
        *index = 1u;
        return true;
    case HW_RANGE_ID_1K:
        *index = 2u;
        return true;
    case HW_RANGE_ID_10K:
        *index = 3u;
        return true;
    case HW_RANGE_ID_100K:
        *index = 4u;
        return true;
    case HW_RANGE_ID_1M:
        *index = 5u;
        return true;
    case HW_RANGE_ID_INVALID:
    default:
        return false;
    }
}

static hw_range_id_t range_from_index(uint8_t index)
{
    static const hw_range_id_t ranges[] = {
        HW_RANGE_ID_10R,
        HW_RANGE_ID_100R,
        HW_RANGE_ID_1K,
        HW_RANGE_ID_10K,
        HW_RANGE_ID_100K,
        HW_RANGE_ID_1M,
    };
    return (index < (uint8_t)(sizeof(ranges) / sizeof(ranges[0]))) ? ranges[index] :
                                                                     HW_RANGE_ID_INVALID;
}

static bool frequency_index(hw_excitation_freq_t frequency, uint8_t *index)
{
    if (index == NULL)
    {
        return false;
    }
    switch (frequency)
    {
    case HW_EXCITATION_FREQ_100HZ:
        *index = 0u;
        return true;
    case HW_EXCITATION_FREQ_1KHZ:
        *index = 1u;
        return true;
    case HW_EXCITATION_FREQ_10KHZ:
        *index = 2u;
        return true;
    case HW_EXCITATION_FREQ_INVALID:
    default:
        return false;
    }
}

static bool amplitude_index(hw_excitation_amp_t amplitude, uint8_t *index)
{
    if (index == NULL)
    {
        return false;
    }
    switch (amplitude)
    {
    case HW_EXCITATION_AMP_100MVRMS:
        *index = 0u;
        return true;
    case HW_EXCITATION_AMP_500MVRMS:
        *index = 1u;
        return true;
    case HW_EXCITATION_AMP_INVALID:
    default:
        return false;
    }
}

static float abs_float(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float max_float(float a, float b)
{
    return (a > b) ? a : b;
}

static float ideal_zref_mag(hw_range_id_t range_id)
{
    const measurement_dsp_config_t config = measurement_dsp_config_ideal(range_id);
    return measurement_complex_mag(config.zref_ohms);
}

static uint64_t condition_mask(measurement_attempt_config_t attempt)
{
    uint8_t range = 0u;
    uint8_t frequency = 0u;
    uint8_t amplitude = 0u;
    if (!range_index(attempt.range_id, &range) ||
        !frequency_index(attempt.frequency, &frequency) ||
        !amplitude_index(attempt.amplitude, &amplitude))
    {
        return 0u;
    }
    const uint8_t bit = (uint8_t)(((uint8_t)range * 6u) + ((uint8_t)frequency * 2u) + amplitude);
    return UINT64_C(1) << bit;
}

static bool condition_was_attempted(const measurement_auto_session_t *session,
                                    measurement_attempt_config_t attempt)
{
    const uint64_t mask = condition_mask(attempt);
    return (mask == 0u) || ((session->attempted_conditions & mask) != 0u);
}

static bool valid_hint(const measurement_auto_hint_t *hint)
{
    return (hint != NULL) && hint->valid &&
           measurement_auto_condition_allowed(hint->range_id, hint->frequency, hint->amplitude);
}

static measurement_attempt_config_t make_attempt(const measurement_auto_session_t *session,
                                                 hw_range_id_t range_id,
                                                 hw_excitation_freq_t frequency,
                                                 hw_excitation_amp_t amplitude,
                                                 measurement_attempt_reason_t reason)
{
    measurement_attempt_config_t attempt = {
        .range_id = range_id,
        .frequency = frequency,
        .amplitude = amplitude,
        .ret_strategy = MEASUREMENT_RET_STRATEGY_AUTO,
        .attempt_number = (uint8_t)(session->attempt_count + 1u),
        .reason = reason,
    };
    return attempt;
}

static measurement_attempt_config_t initial_attempt(const measurement_auto_session_t *session,
                                                    const measurement_auto_hint_t *hint)
{
    if (valid_hint(hint))
    {
        return make_attempt(session,
                            hint->range_id,
                            hint->frequency,
                            hint->amplitude,
                            MEASUREMENT_ATTEMPT_PREVIOUS_HINT);
    }
    return make_attempt(session,
                        HW_RANGE_ID_1K,
                        HW_EXCITATION_FREQ_1KHZ,
                        HW_EXCITATION_AMP_100MVRMS,
                        MEASUREMENT_ATTEMPT_INITIAL_PROBE);
}

static bool result_math_ok(const measurement_attempt_result_t *result)
{
    return (result != NULL) &&
           !result->phase05_failed &&
           !result->safety_abort &&
           !result->canceled &&
           (result->dsp_status == MEASUREMENT_STATUS_OK) &&
           result->derived.valid &&
           measurement_complex_is_finite(result->z_ohms);
}

static bool any_ret_usable(const measurement_attempt_result_t *result)
{
    return (result != NULL) && (result->ret_1x_quality.usable || result->ret_hg_quality.usable);
}

static float selected_return_peak(const measurement_attempt_result_t *result)
{
    if (result == NULL)
    {
        return 0.0f;
    }
    return (result->selected_channel == MEASUREMENT_RETURN_HG) ? result->ret_hg_quality.signal_peak_v :
                                                                 result->ret_1x_quality.signal_peak_v;
}

static bool can_append_attempt(const measurement_auto_session_t *session,
                               measurement_attempt_config_t attempt)
{
    return (session != NULL) &&
           (session->attempt_count < MEASUREMENT_AUTO_MAX_ATTEMPTS) &&
           measurement_auto_condition_allowed(attempt.range_id, attempt.frequency, attempt.amplitude) &&
           !condition_was_attempted(session, attempt);
}

static bool choose_next_range(const measurement_auto_session_t *session,
                              const measurement_attempt_config_t *base,
                              int8_t direction,
                              measurement_attempt_reason_t reason,
                              measurement_attempt_config_t *out)
{
    uint8_t current = 0u;
    if ((session == NULL) || (base == NULL) || (out == NULL) ||
        !range_index(base->range_id, &current) || (direction == 0) ||
        (session->range_transition_count >= MEASUREMENT_AUTO_MAX_RANGE_TRANSITIONS))
    {
        return false;
    }

    const int candidate = (int)current + (int)direction;
    if ((candidate < 0) || (candidate > 5))
    {
        return false;
    }

    measurement_attempt_config_t attempt =
        make_attempt(session, range_from_index((uint8_t)candidate), base->frequency, base->amplitude, reason);
    if (!can_append_attempt(session, attempt))
    {
        return false;
    }

    *out = attempt;
    return true;
}

static bool choose_500mv_retry(const measurement_auto_session_t *session,
                               const measurement_attempt_config_t *base,
                               measurement_attempt_config_t *out)
{
    if ((session == NULL) || (base == NULL) || (out == NULL) ||
        (base->amplitude != HW_EXCITATION_AMP_100MVRMS))
    {
        return false;
    }
    measurement_attempt_config_t attempt = make_attempt(session,
                                                        base->range_id,
                                                        base->frequency,
                                                        HW_EXCITATION_AMP_500MVRMS,
                                                        MEASUREMENT_ATTEMPT_IMPROVE_SNR);
    if (can_append_attempt(session, attempt))
    {
        *out = attempt;
        return true;
    }
    return false;
}

static bool choose_frequency_refinement(const measurement_auto_session_t *session,
                                        const measurement_attempt_result_t *result,
                                        measurement_attempt_config_t *out)
{
    if ((session == NULL) || (result == NULL) || (out == NULL) ||
        (session->frequency_refinement_count >= MEASUREMENT_AUTO_MAX_FREQUENCY_REFINEMENTS))
    {
        return false;
    }
    if ((result->derived.interpretation != MEASUREMENT_INTERPRET_CAPACITIVE) &&
        (result->derived.interpretation != MEASUREMENT_INTERPRET_INDUCTIVE) &&
        (result->derived.interpretation != MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN))
    {
        return false;
    }

    hw_excitation_freq_t next = HW_EXCITATION_FREQ_10KHZ;
    if (result->config.frequency == HW_EXCITATION_FREQ_10KHZ)
    {
        next = HW_EXCITATION_FREQ_1KHZ;
    }
    else if (result->config.frequency == HW_EXCITATION_FREQ_100HZ)
    {
        next = HW_EXCITATION_FREQ_1KHZ;
    }

    measurement_attempt_config_t attempt = make_attempt(session,
                                                        result->config.range_id,
                                                        next,
                                                        result->config.amplitude,
                                                        MEASUREMENT_ATTEMPT_CHECK_SECOND_FREQUENCY);
    if (can_append_attempt(session, attempt))
    {
        *out = attempt;
        return true;
    }
    return false;
}

static measurement_attempt_result_t best_attempt(const measurement_auto_session_t *session)
{
    measurement_attempt_result_t best = {0};
    bool have_best = false;
    float best_mag = 0.0f;
    if (session == NULL)
    {
        return best;
    }

    for (uint8_t i = 0u; i < session->attempt_count; i++)
    {
        const measurement_attempt_result_t *candidate = &session->history[i];
        if (!result_math_ok(candidate))
        {
            continue;
        }
        const float mag = measurement_complex_mag(candidate->z_ohms);
        if (!have_best || (mag > best_mag))
        {
            best = *candidate;
            best_mag = mag;
            have_best = true;
        }
    }
    return best;
}

static measurement_auto_status_t terminal_status_from_result(const measurement_attempt_result_t *result)
{
    if (result == NULL)
    {
        return MEASUREMENT_AUTO_STATUS_FAILED;
    }
    if (result->canceled)
    {
        return MEASUREMENT_AUTO_STATUS_CANCELED;
    }
    if (result->safety_abort)
    {
        return MEASUREMENT_AUTO_STATUS_SAFETY_ABORT;
    }
    if (result->open_like || (result->dsp_status == MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL))
    {
        return MEASUREMENT_AUTO_STATUS_OPEN_LIKE;
    }
    if (result->short_like)
    {
        return MEASUREMENT_AUTO_STATUS_SHORT_LIKE;
    }
    if (result->phase05_failed)
    {
        return MEASUREMENT_AUTO_STATUS_FAILED;
    }
    return MEASUREMENT_AUTO_STATUS_NO_VALID_CONDITION;
}

static policy_decision_t decide_next(const measurement_auto_session_t *session,
                                     const measurement_attempt_result_t *result)
{
    policy_decision_t decision = {
        .decision = NEXT_DECISION_FINAL,
        .final_status = terminal_status_from_result(result),
        .attempt = {0},
        .continuation_reason = MEASUREMENT_ATTEMPT_VERIFY_CLASSIFICATION,
        .publish_partial = false,
    };

    if ((session == NULL) || (result == NULL))
    {
        decision.final_status = MEASUREMENT_AUTO_STATUS_FAILED;
        return decision;
    }
    if ((session->attempt_count >= MEASUREMENT_AUTO_MAX_ATTEMPTS) ||
        result->canceled ||
        result->safety_abort ||
        result->phase05_failed)
    {
        if (session->attempt_count >= MEASUREMENT_AUTO_MAX_ATTEMPTS)
        {
            decision.final_status = result_math_ok(result) ? MEASUREMENT_AUTO_STATUS_FINAL_OK :
                                                             terminal_status_from_result(result);
        }
        return decision;
    }

    if (result->clipped || (result->dsp_status == MEASUREMENT_STATUS_CLIPPED))
    {
        measurement_attempt_config_t attempt;
        if (choose_next_range(session, &result->config, 1, MEASUREMENT_ATTEMPT_AVOID_CLIPPING, &attempt))
        {
            decision.decision = NEXT_DECISION_ATTEMPT;
            decision.attempt = attempt;
            decision.continuation_reason = MEASUREMENT_ATTEMPT_AVOID_CLIPPING;
            return decision;
        }
        decision.final_status = MEASUREMENT_AUTO_STATUS_NO_VALID_CONDITION;
        return decision;
    }

    if (!result_math_ok(result))
    {
        measurement_attempt_config_t attempt;
        if (!any_ret_usable(result) && choose_500mv_retry(session, &result->config, &attempt))
        {
            decision.decision = NEXT_DECISION_ATTEMPT;
            decision.attempt = attempt;
            decision.continuation_reason = MEASUREMENT_ATTEMPT_IMPROVE_SNR;
            return decision;
        }
        decision.final_status = terminal_status_from_result(result);
        return decision;
    }

    decision.publish_partial = true;
    const float zref = ideal_zref_mag(result->config.range_id);
    const float zmag = measurement_complex_mag(result->z_ohms);
    const float ratio = zmag / max_float(zref, 1.0e-6f);
    measurement_attempt_config_t attempt;

    if ((ratio <= AUTO_RATIO_TOO_SMALL) &&
        choose_next_range(session, &result->config, -1, MEASUREMENT_ATTEMPT_RERANGE, &attempt))
    {
        decision.decision = NEXT_DECISION_ATTEMPT;
        decision.attempt = attempt;
        decision.continuation_reason = MEASUREMENT_ATTEMPT_RERANGE;
        return decision;
    }
    if ((ratio >= AUTO_RATIO_TOO_LARGE) &&
        choose_next_range(session, &result->config, 1, MEASUREMENT_ATTEMPT_RERANGE, &attempt))
    {
        decision.decision = NEXT_DECISION_ATTEMPT;
        decision.attempt = attempt;
        decision.continuation_reason = MEASUREMENT_ATTEMPT_RERANGE;
        return decision;
    }
    if ((ratio >= AUTO_RATIO_OPEN_LIKE) && (result->config.range_id == HW_RANGE_ID_1M))
    {
        decision.final_status = MEASUREMENT_AUTO_STATUS_OPEN_LIKE;
        return decision;
    }
    if ((ratio <= (AUTO_RATIO_TOO_SMALL * 0.1f)) && (result->config.range_id == HW_RANGE_ID_10R))
    {
        decision.final_status = MEASUREMENT_AUTO_STATUS_SHORT_LIKE;
        return decision;
    }

    if ((selected_return_peak(result) < AUTO_RET_WEAK_V_PEAK) &&
        choose_500mv_retry(session, &result->config, &attempt))
    {
        decision.decision = NEXT_DECISION_ATTEMPT;
        decision.attempt = attempt;
        decision.continuation_reason = MEASUREMENT_ATTEMPT_IMPROVE_SNR;
        return decision;
    }

    if ((result->derived.interpretation != MEASUREMENT_INTERPRET_RESISTIVE) &&
        choose_frequency_refinement(session, result, &attempt))
    {
        decision.decision = NEXT_DECISION_ATTEMPT;
        decision.attempt = attempt;
        decision.continuation_reason = MEASUREMENT_ATTEMPT_CHECK_SECOND_FREQUENCY;
        return decision;
    }

    decision.final_status = MEASUREMENT_AUTO_STATUS_FINAL_OK;
    return decision;
}

static void finalize_result(measurement_auto_session_t *session,
                            measurement_auto_status_t status,
                            measurement_attempt_reason_t continuation_reason)
{
    if (session == NULL)
    {
        return;
    }
    measurement_attempt_result_t best = best_attempt(session);
    session->status = status;
    session->has_next_attempt = false;
    session->has_final = true;
    session->last_result.status = status;
    session->last_result.best_attempt = best;
    session->last_result.confidence = measurement_auto_evaluate_confidence(&best, session->qualification);
    session->last_result.classification =
        measurement_auto_classify_session(session->history, session->attempt_count);
    if ((status == MEASUREMENT_AUTO_STATUS_OPEN_LIKE) || (status == MEASUREMENT_AUTO_STATUS_SHORT_LIKE))
    {
        session->last_result.confidence.class_id = MEASUREMENT_CONFIDENCE_LOW_CONFIDENCE;
    }
    if ((status == MEASUREMENT_AUTO_STATUS_NO_VALID_CONDITION) ||
        (status == MEASUREMENT_AUTO_STATUS_SAFETY_ABORT) ||
        (status == MEASUREMENT_AUTO_STATUS_CANCELED) ||
        (status == MEASUREMENT_AUTO_STATUS_FAILED))
    {
        session->last_result.confidence.class_id = MEASUREMENT_CONFIDENCE_REJECTED;
    }
    session->last_result.continuation_reason = continuation_reason;
    session->last_result.attempt_count = session->attempt_count;
    session->last_result.session_sequence = session->session_sequence;
    session->last_result.mode = session->mode;
    session->last_result.partial = false;
    session->last_result.final = true;
}

static void publish_partial(measurement_auto_session_t *session,
                            const measurement_attempt_result_t *result,
                            measurement_attempt_reason_t continuation_reason)
{
    if ((session == NULL) || (result == NULL))
    {
        return;
    }
    session->last_result.status = MEASUREMENT_AUTO_STATUS_RUNNING;
    session->last_result.best_attempt = *result;
    session->last_result.confidence = measurement_auto_evaluate_confidence(result, session->qualification);
    session->last_result.classification =
        measurement_auto_classify_session(session->history, session->attempt_count);
    session->last_result.continuation_reason = continuation_reason;
    session->last_result.attempt_count = session->attempt_count;
    session->last_result.session_sequence = session->session_sequence;
    session->last_result.mode = session->mode;
    session->last_result.partial = true;
    session->last_result.final = false;
    session->has_partial = true;
}

void measurement_auto_session_init(measurement_auto_session_t *session)
{
    if (session != NULL)
    {
        *session = (measurement_auto_session_t){0};
        session->status = MEASUREMENT_AUTO_STATUS_IDLE;
        session->qualification = MEASUREMENT_QUALIFICATION_UNQUALIFIED;
    }
}

bool measurement_auto_start_session(measurement_auto_session_t *session,
                                    const measurement_auto_start_t *start)
{
    if ((session == NULL) || (start == NULL))
    {
        return false;
    }
    measurement_auto_hint_t hint = start->hint;
    measurement_auto_session_init(session);
    session->mode = start->mode;
    session->qualification = start->qualification;
    session->session_sequence = start->session_sequence;
    session->status = MEASUREMENT_AUTO_STATUS_RUNNING;
    session->next_attempt = initial_attempt(session, &hint);
    session->has_next_attempt = true;
    return measurement_auto_condition_allowed(session->next_attempt.range_id,
                                              session->next_attempt.frequency,
                                              session->next_attempt.amplitude);
}

bool measurement_auto_next_attempt(const measurement_auto_session_t *session,
                                   measurement_attempt_config_t *attempt)
{
    if ((session == NULL) || (attempt == NULL) || !session->has_next_attempt)
    {
        return false;
    }
    *attempt = session->next_attempt;
    return true;
}

measurement_auto_event_t measurement_auto_submit_result(measurement_auto_session_t *session,
                                                        const measurement_attempt_result_t *result)
{
    if ((session == NULL) || (result == NULL) ||
        (session->status != MEASUREMENT_AUTO_STATUS_RUNNING) ||
        !session->has_next_attempt ||
        (session->attempt_count >= MEASUREMENT_AUTO_MAX_ATTEMPTS))
    {
        return MEASUREMENT_AUTO_EVENT_NONE;
    }

    measurement_attempt_result_t stored = *result;
    stored.config = session->next_attempt;
    session->history[session->attempt_count] = stored;
    session->attempt_count++;
    session->attempted_conditions |= condition_mask(stored.config);
    session->has_next_attempt = false;

    const policy_decision_t decision = decide_next(session, &stored);
    if ((decision.decision == NEXT_DECISION_ATTEMPT) &&
        (session->attempt_count < MEASUREMENT_AUTO_MAX_ATTEMPTS))
    {
        if (decision.attempt.range_id != stored.config.range_id)
        {
            session->range_transition_count++;
        }
        if (decision.attempt.frequency != stored.config.frequency)
        {
            session->frequency_refinement_count++;
        }
        session->next_attempt = decision.attempt;
        session->has_next_attempt = true;
        if (decision.publish_partial)
        {
            publish_partial(session, &stored, decision.continuation_reason);
            return MEASUREMENT_AUTO_EVENT_PARTIAL_RESULT;
        }
        return MEASUREMENT_AUTO_EVENT_ATTEMPT_READY;
    }

    finalize_result(session, decision.final_status, decision.continuation_reason);
    return MEASUREMENT_AUTO_EVENT_FINAL_RESULT;
}

measurement_auto_event_t measurement_auto_cancel(measurement_auto_session_t *session)
{
    if ((session == NULL) || (session->status != MEASUREMENT_AUTO_STATUS_RUNNING))
    {
        return MEASUREMENT_AUTO_EVENT_NONE;
    }
    finalize_result(session, MEASUREMENT_AUTO_STATUS_CANCELED, MEASUREMENT_ATTEMPT_VERIFY_CLASSIFICATION);
    return MEASUREMENT_AUTO_EVENT_FINAL_RESULT;
}

measurement_auto_hint_t measurement_auto_make_hint(const measurement_session_result_t *result)
{
    measurement_auto_hint_t hint = {0};
    if ((result == NULL) || !result->final ||
        ((result->status != MEASUREMENT_AUTO_STATUS_FINAL_OK) &&
         (result->status != MEASUREMENT_AUTO_STATUS_OPEN_LIKE) &&
         (result->status != MEASUREMENT_AUTO_STATUS_SHORT_LIKE)))
    {
        return hint;
    }
    hint.valid = measurement_auto_condition_allowed(result->best_attempt.config.range_id,
                                                   result->best_attempt.config.frequency,
                                                   result->best_attempt.config.amplitude);
    hint.range_id = result->best_attempt.config.range_id;
    hint.frequency = result->best_attempt.config.frequency;
    hint.amplitude = result->best_attempt.config.amplitude;
    hint.ret_channel = result->best_attempt.selected_channel;
    return hint;
}

bool measurement_auto_condition_allowed(hw_range_id_t range_id,
                                        hw_excitation_freq_t frequency,
                                        hw_excitation_amp_t amplitude)
{
    uint8_t ignored = 0u;
    return range_index(range_id, &ignored) &&
           frequency_index(frequency, &ignored) &&
           amplitude_index(amplitude, &ignored) &&
           (hw_excitation_validate_amplitude(range_id, amplitude) == BSP_STATUS_OK);
}

measurement_ret_policy_result_t measurement_auto_select_ret_channel(
    const measurement_attempt_result_t *result)
{
    measurement_ret_policy_result_t policy = {
        .usable = false,
        .selected = MEASUREMENT_RETURN_1X,
        .reason_flags = MEASUREMENT_AUTO_REASON_NONE,
    };
    if (result == NULL)
    {
        policy.reason_flags = MEASUREMENT_AUTO_REASON_DSP_ERROR;
        return policy;
    }
    if (result->ret_1x_quality.clipped || result->ret_hg_quality.clipped || result->clipped)
    {
        policy.reason_flags |= MEASUREMENT_AUTO_REASON_CLIPPED;
    }
    if (result->ret_1x_quality.usable && (result->ret_1x_quality.signal_peak_v >= AUTO_RET_GOOD_V_PEAK))
    {
        policy.usable = true;
        policy.selected = MEASUREMENT_RETURN_1X;
        return policy;
    }
    if (result->ret_hg_quality.usable && !result->ret_hg_quality.clipped)
    {
        policy.usable = true;
        policy.selected = MEASUREMENT_RETURN_HG;
        policy.reason_flags |= MEASUREMENT_AUTO_REASON_HG_PROVISIONAL;
        return policy;
    }
    if (result->ret_1x_quality.usable)
    {
        policy.usable = true;
        policy.selected = MEASUREMENT_RETURN_1X;
        return policy;
    }
    policy.reason_flags |= MEASUREMENT_AUTO_REASON_RETURN_LOW;
    return policy;
}

measurement_confidence_result_t measurement_auto_evaluate_confidence(
    const measurement_attempt_result_t *result,
    measurement_qualification_t qualification)
{
    measurement_confidence_result_t confidence = {
        .class_id = MEASUREMENT_CONFIDENCE_REJECTED,
        .reason_flags = MEASUREMENT_AUTO_REASON_NONE,
        .mathematically_valid = result_math_ok(result),
    };
    if (result == NULL)
    {
        confidence.reason_flags = MEASUREMENT_AUTO_REASON_DSP_ERROR;
        return confidence;
    }
    if (result->canceled)
    {
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_CANCELED;
    }
    if (result->safety_abort)
    {
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_SAFETY_ABORT;
    }
    if (result->phase05_failed)
    {
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_HARDWARE_ERROR;
    }
    if (result->clipped || (result->dsp_status == MEASUREMENT_STATUS_CLIPPED))
    {
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_CLIPPED;
    }
    if (result->dsp_status == MEASUREMENT_STATUS_SOURCE_TOO_SMALL)
    {
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_SOURCE_LOW;
    }
    if (result->dsp_status == MEASUREMENT_STATUS_CHANNEL_UNUSABLE)
    {
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_RETURN_LOW;
    }
    if (result->dsp_status == MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL)
    {
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_DENOMINATOR_LOW | MEASUREMENT_AUTO_REASON_OPEN_LIKE;
    }
    if (result->open_like)
    {
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_OPEN_LIKE;
    }
    if (result->short_like)
    {
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_SHORT_LIKE;
    }
    if (!confidence.mathematically_valid)
    {
        if (confidence.reason_flags == MEASUREMENT_AUTO_REASON_NONE)
        {
            confidence.reason_flags |= MEASUREMENT_AUTO_REASON_DSP_ERROR;
        }
        return confidence;
    }
    if (selected_return_peak(result) < AUTO_RET_GOOD_V_PEAK)
    {
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_RETURN_LOW;
    }
    if (result->selected_channel == MEASUREMENT_RETURN_HG)
    {
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_HG_PROVISIONAL;
    }

    switch (qualification)
    {
    case MEASUREMENT_QUALIFICATION_NOMINAL:
        confidence.class_id = (confidence.reason_flags == MEASUREMENT_AUTO_REASON_NONE) ?
                                  MEASUREMENT_CONFIDENCE_NOMINAL :
                                  MEASUREMENT_CONFIDENCE_LOW_CONFIDENCE;
        break;
    case MEASUREMENT_QUALIFICATION_EXTENDED:
        confidence.class_id = (confidence.reason_flags == MEASUREMENT_AUTO_REASON_NONE) ?
                                  MEASUREMENT_CONFIDENCE_EXTENDED :
                                  MEASUREMENT_CONFIDENCE_LOW_CONFIDENCE;
        break;
    case MEASUREMENT_QUALIFICATION_DISABLED:
        confidence.class_id = MEASUREMENT_CONFIDENCE_REJECTED;
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_HARDWARE_ERROR;
        break;
    case MEASUREMENT_QUALIFICATION_UNQUALIFIED:
    default:
        confidence.class_id = (confidence.reason_flags & ~(uint32_t)MEASUREMENT_AUTO_REASON_HG_PROVISIONAL) == 0u ?
                                  MEASUREMENT_CONFIDENCE_EXTENDED :
                                  MEASUREMENT_CONFIDENCE_LOW_CONFIDENCE;
        confidence.reason_flags |= MEASUREMENT_AUTO_REASON_UNQUALIFIED;
        break;
    }
    return confidence;
}

measurement_session_classification_t measurement_auto_classify_session(
    const measurement_attempt_result_t *history,
    uint8_t count)
{
    measurement_session_classification_t out = {
        .interpretation = MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN,
        .reason_flags = MEASUREMENT_AUTO_REASON_NONE,
        .supporting_attempts = 0u,
    };
    measurement_interpretation_t first = MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN;
    int8_t sign = 0;

    if (history == NULL)
    {
        out.reason_flags = MEASUREMENT_AUTO_REASON_AMBIGUOUS;
        return out;
    }

    for (uint8_t i = 0u; i < count; i++)
    {
        const measurement_attempt_result_t *result = &history[i];
        if (!result_math_ok(result))
        {
            continue;
        }
        const float r_abs = abs_float(result->derived.resistance_ohms);
        const float x_abs = abs_float(result->derived.reactance_ohms);
        const float ratio = x_abs / max_float(r_abs, 1.0e-6f);
        measurement_interpretation_t interp = MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN;
        if (ratio <= AUTO_RESISTIVE_RATIO_MAX)
        {
            interp = MEASUREMENT_INTERPRET_RESISTIVE;
        }
        else if (ratio >= AUTO_REACTIVE_RATIO_MIN)
        {
            interp = (result->derived.reactance_ohms < 0.0f) ?
                         MEASUREMENT_INTERPRET_CAPACITIVE :
                         MEASUREMENT_INTERPRET_INDUCTIVE;
        }

        if (interp == MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN)
        {
            out.reason_flags |= MEASUREMENT_AUTO_REASON_AMBIGUOUS;
            continue;
        }
        if (first == MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN)
        {
            first = interp;
            sign = (result->derived.reactance_ohms > 0.0f) ? 1 : (result->derived.reactance_ohms < 0.0f) ? -1 : 0;
            out.supporting_attempts = 1u;
            continue;
        }
        if (interp != first)
        {
            out.reason_flags |= MEASUREMENT_AUTO_REASON_INCONSISTENT;
            out.interpretation = MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN;
            return out;
        }
        const int8_t this_sign =
            (result->derived.reactance_ohms > 0.0f) ? 1 : (result->derived.reactance_ohms < 0.0f) ? -1 : 0;
        if ((sign != 0) && (this_sign != 0) && (this_sign != sign))
        {
            out.reason_flags |= MEASUREMENT_AUTO_REASON_INCONSISTENT;
            out.interpretation = MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN;
            return out;
        }
        out.supporting_attempts++;
    }

    out.interpretation = first;
    if (first == MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN)
    {
        out.reason_flags |= MEASUREMENT_AUTO_REASON_AMBIGUOUS;
    }
    return out;
}

const measurement_session_result_t *measurement_auto_last_result(
    const measurement_auto_session_t *session)
{
    return (session == NULL) ? NULL : &session->last_result;
}

const char *measurement_auto_status_string(measurement_auto_status_t status)
{
    switch (status)
    {
    case MEASUREMENT_AUTO_STATUS_IDLE:
        return "IDLE";
    case MEASUREMENT_AUTO_STATUS_RUNNING:
        return "RUNNING";
    case MEASUREMENT_AUTO_STATUS_FINAL_OK:
        return "FINAL_OK";
    case MEASUREMENT_AUTO_STATUS_OPEN_LIKE:
        return "OPEN_LIKE";
    case MEASUREMENT_AUTO_STATUS_SHORT_LIKE:
        return "SHORT_LIKE";
    case MEASUREMENT_AUTO_STATUS_NO_VALID_CONDITION:
        return "NO_VALID_CONDITION";
    case MEASUREMENT_AUTO_STATUS_SAFETY_ABORT:
        return "SAFETY_ABORT";
    case MEASUREMENT_AUTO_STATUS_CANCELED:
        return "CANCELED";
    case MEASUREMENT_AUTO_STATUS_FAILED:
    default:
        return "FAILED";
    }
}

const char *measurement_confidence_string(measurement_confidence_class_t confidence)
{
    switch (confidence)
    {
    case MEASUREMENT_CONFIDENCE_NOMINAL:
        return "NOMINAL";
    case MEASUREMENT_CONFIDENCE_EXTENDED:
        return "EXTENDED";
    case MEASUREMENT_CONFIDENCE_LOW_CONFIDENCE:
        return "LOW_CONFIDENCE";
    case MEASUREMENT_CONFIDENCE_REJECTED:
    default:
        return "REJECTED";
    }
}

const char *measurement_attempt_reason_string(measurement_attempt_reason_t reason)
{
    switch (reason)
    {
    case MEASUREMENT_ATTEMPT_INITIAL_PROBE:
        return "INITIAL_PROBE";
    case MEASUREMENT_ATTEMPT_PREVIOUS_HINT:
        return "PREVIOUS_HINT";
    case MEASUREMENT_ATTEMPT_RERANGE:
        return "RERANGE";
    case MEASUREMENT_ATTEMPT_IMPROVE_SNR:
        return "IMPROVE_SNR";
    case MEASUREMENT_ATTEMPT_AVOID_CLIPPING:
        return "AVOID_CLIPPING";
    case MEASUREMENT_ATTEMPT_CHECK_SECOND_FREQUENCY:
        return "CHECK_SECOND_FREQUENCY";
    case MEASUREMENT_ATTEMPT_VERIFY_CLASSIFICATION:
        return "VERIFY_CLASSIFICATION";
    case MEASUREMENT_ATTEMPT_CHECK_REACTIVE_PARAMETER:
    default:
        return "CHECK_REACTIVE_PARAMETER";
    }
}

const char *measurement_ret_strategy_string(measurement_ret_strategy_t strategy)
{
    switch (strategy)
    {
    case MEASUREMENT_RET_STRATEGY_AUTO:
        return "AUTO";
    case MEASUREMENT_RET_STRATEGY_1X:
        return "RET_1X";
    case MEASUREMENT_RET_STRATEGY_HG:
    default:
        return "RET_HG";
    }
}
