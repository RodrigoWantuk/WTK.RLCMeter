#include "measurement/measurement_engine.h"

#include <stdio.h>
#include <string.h>

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int expect_u32(uint32_t actual, uint32_t expected, const char *message)
{
    if (actual != expected)
    {
        (void)fprintf(stderr,
                      "FAIL: %s (got %lu expected %lu)\n",
                      message,
                      (unsigned long)actual,
                      (unsigned long)expected);
        return 1;
    }
    return 0;
}

static int expect_range(hw_range_id_t actual, hw_range_id_t expected, const char *message)
{
    return expect_u32((uint32_t)actual, (uint32_t)expected, message);
}

static int expect_freq(hw_excitation_freq_t actual, hw_excitation_freq_t expected, const char *message)
{
    return expect_u32((uint32_t)actual, (uint32_t)expected, message);
}

static int expect_amp(hw_excitation_amp_t actual, hw_excitation_amp_t expected, const char *message)
{
    return expect_u32((uint32_t)actual, (uint32_t)expected, message);
}

static float abs_f(float value)
{
    return (value < 0.0f) ? -value : value;
}

static int expect_near(float actual, float expected, float tolerance, const char *message)
{
    if (abs_f(actual - expected) > tolerance)
    {
        (void)fprintf(stderr,
                      "FAIL: %s (got %.7g expected %.7g tol %.7g)\n",
                      message,
                      (double)actual,
                      (double)expected,
                      (double)tolerance);
        return 1;
    }
    return 0;
}

static uint32_t frequency_hz(hw_excitation_freq_t frequency)
{
    switch (frequency)
    {
    case HW_EXCITATION_FREQ_100HZ:
        return 100u;
    case HW_EXCITATION_FREQ_1KHZ:
        return 1000u;
    case HW_EXCITATION_FREQ_10KHZ:
        return 10000u;
    case HW_EXCITATION_FREQ_INVALID:
    default:
        return 0u;
    }
}

static measurement_attempt_result_t make_result(measurement_complex_t z,
                                                measurement_interpretation_t interpretation)
{
    measurement_attempt_result_t result;
    memset(&result, 0, sizeof(result));
    result.dsp_status = MEASUREMENT_STATUS_OK;
    result.z_ohms = z;
    result.derived = measurement_derive_quantities(z,
                                                   1000u,
                                                   &(measurement_dsp_config_t){
                                                       .ret_hg_transfer = {15.468085f, 0.0f},
                                                       .zref_ohms = {1000.0f, 0.0f},
                                                       .source_min_v_peak = 0.001f,
                                                       .return_min_v_peak = 0.00005f,
                                                       .denominator_min_v_peak = 0.00005f,
                                                       .reactance_min_ohms = 0.001f,
                                                       .interpretation_ratio_resistive_max = 0.10f,
                                                       .interpretation_ratio_reactive_min = 0.25f,
                                                   },
                                                   MEASUREMENT_STATUS_OK);
    result.derived.interpretation = interpretation;
    result.ret_1x_quality = (measurement_channel_quality_t){
        .usable = true,
        .clipped = false,
        .calibration_valid = true,
        .signal_too_small = false,
        .signal_peak_v = 0.020f,
    };
    result.ret_hg_quality = (measurement_channel_quality_t){
        .usable = true,
        .clipped = false,
        .calibration_valid = true,
        .signal_too_small = false,
        .signal_peak_v = 0.020f,
    };
    result.selected_channel = MEASUREMENT_RETURN_1X;
    return result;
}

static measurement_attempt_result_t make_result_for_attempt(const measurement_attempt_config_t *attempt,
                                                           measurement_complex_t z,
                                                           measurement_interpretation_t interpretation)
{
    measurement_attempt_result_t result = make_result(z, interpretation);
    if (attempt != NULL)
    {
        result.derived = measurement_derive_quantities(z,
                                                       frequency_hz(attempt->frequency),
                                                       &(measurement_dsp_config_t){
                                                           .ret_hg_transfer = {15.468085f, 0.0f},
                                                           .zref_ohms = measurement_dsp_config_ideal(attempt->range_id).zref_ohms,
                                                           .source_min_v_peak = 0.001f,
                                                           .return_min_v_peak = 0.00005f,
                                                           .denominator_min_v_peak = 0.00005f,
                                                           .reactance_min_ohms = 0.001f,
                                                           .interpretation_ratio_resistive_max = 0.10f,
                                                           .interpretation_ratio_reactive_min = 0.25f,
                                                       },
                                                       MEASUREMENT_STATUS_OK);
        result.derived.interpretation = interpretation;
    }
    return result;
}

static measurement_auto_start_t default_start(void)
{
    measurement_auto_start_t start = {
        .mode = MEASUREMENT_AUTO_MODE_CLICK,
        .qualification = MEASUREMENT_QUALIFICATION_UNQUALIFIED,
        .session_sequence = 42u,
        .hint = {0},
    };
    return start;
}

static int start_and_get(measurement_auto_session_t *session, measurement_attempt_config_t *attempt)
{
    measurement_auto_start_t start = default_start();
    int failures = 0;
    failures += expect_true(measurement_auto_start_session(session, &start), "start session");
    failures += expect_true(measurement_auto_next_attempt(session, attempt), "get attempt");
    return failures;
}

static int test_initial_probe_and_policy_matrix(void)
{
    int failures = 0;
    measurement_auto_session_t session;
    measurement_attempt_config_t attempt;
    failures += start_and_get(&session, &attempt);
    failures += expect_range(attempt.range_id, HW_RANGE_ID_1K, "initial range");
    failures += expect_freq(attempt.frequency, HW_EXCITATION_FREQ_1KHZ, "initial frequency");
    failures += expect_amp(attempt.amplitude, HW_EXCITATION_AMP_100MVRMS, "initial amplitude");
    failures += expect_true(attempt.reason == MEASUREMENT_ATTEMPT_INITIAL_PROBE, "initial reason");
    failures += expect_true(measurement_auto_condition_allowed(HW_RANGE_ID_10R,
                                                              HW_EXCITATION_FREQ_1KHZ,
                                                              HW_EXCITATION_AMP_100MVRMS),
                            "10R 100m allowed");
    failures += expect_true(!measurement_auto_condition_allowed(HW_RANGE_ID_10R,
                                                               HW_EXCITATION_FREQ_1KHZ,
                                                               HW_EXCITATION_AMP_500MVRMS),
                            "10R 500m impossible");
    failures += expect_true(!measurement_auto_condition_allowed(HW_RANGE_ID_INVALID,
                                                               HW_EXCITATION_FREQ_1KHZ,
                                                               HW_EXCITATION_AMP_100MVRMS),
                            "invalid range rejected");
    return failures;
}

static int test_autorange_movements(void)
{
    int failures = 0;
    measurement_auto_session_t session;
    measurement_attempt_config_t attempt;
    failures += start_and_get(&session, &attempt);
    measurement_attempt_result_t small = make_result_for_attempt(&attempt,
                                                                 measurement_complex(50.0f, 0.0f),
                                                                 MEASUREMENT_INTERPRET_RESISTIVE);
    failures += expect_true(measurement_auto_submit_result(&session, &small) == MEASUREMENT_AUTO_EVENT_PARTIAL_RESULT,
                            "small DUT partial before rerange");
    failures += expect_true(measurement_auto_next_attempt(&session, &attempt), "next after low ratio");
    failures += expect_range(attempt.range_id, HW_RANGE_ID_100R, "move down one range");

    measurement_auto_start_t start = default_start();
    measurement_auto_session_init(&session);
    failures += expect_true(measurement_auto_start_session(&session, &start), "restart high ratio");
    failures += expect_true(measurement_auto_next_attempt(&session, &attempt), "get high ratio attempt");
    measurement_attempt_result_t large = make_result_for_attempt(&attempt,
                                                                 measurement_complex(20000.0f, 0.0f),
                                                                 MEASUREMENT_INTERPRET_RESISTIVE);
    failures += expect_true(measurement_auto_submit_result(&session, &large) == MEASUREMENT_AUTO_EVENT_PARTIAL_RESULT,
                            "large DUT partial before rerange");
    failures += expect_true(measurement_auto_next_attempt(&session, &attempt), "next after high ratio");
    failures += expect_range(attempt.range_id, HW_RANGE_ID_10K, "move up one range");
    return failures;
}

static int test_no_range_oscillation(void)
{
    int failures = 0;
    measurement_auto_session_t session;
    measurement_attempt_config_t attempt;
    failures += start_and_get(&session, &attempt);
    measurement_attempt_result_t large = make_result_for_attempt(&attempt,
                                                                 measurement_complex(20000.0f, 0.0f),
                                                                 MEASUREMENT_INTERPRET_RESISTIVE);
    failures += expect_true(measurement_auto_submit_result(&session, &large) == MEASUREMENT_AUTO_EVENT_PARTIAL_RESULT,
                            "oscillation setup reranges up");
    failures += expect_true(measurement_auto_next_attempt(&session, &attempt), "oscillation second attempt");
    failures += expect_range(attempt.range_id, HW_RANGE_ID_10K, "oscillation at 10K");

    measurement_attempt_result_t small = make_result_for_attempt(&attempt,
                                                                 measurement_complex(100.0f, 0.0f),
                                                                 MEASUREMENT_INTERPRET_RESISTIVE);
    failures += expect_true(measurement_auto_submit_result(&session, &small) ==
                                MEASUREMENT_AUTO_EVENT_FINAL_RESULT,
                            "adjacent attempted range terminates");
    const measurement_session_result_t *final = measurement_auto_last_result(&session);
    failures += expect_true(final != NULL && final->final, "oscillation final exists");
    failures += expect_u32(final->attempt_count, 2u, "oscillation attempt bound");
    failures += expect_true(!measurement_auto_next_attempt(&session, &attempt), "no repeated range loop");
    return failures;
}

static int test_amplitude_and_clipping_policy(void)
{
    int failures = 0;
    measurement_auto_session_t session;
    measurement_attempt_config_t attempt;
    failures += start_and_get(&session, &attempt);
    measurement_attempt_result_t weak = make_result_for_attempt(&attempt,
                                                                measurement_complex(1000.0f, 0.0f),
                                                                MEASUREMENT_INTERPRET_RESISTIVE);
    weak.ret_1x_quality.usable = false;
    weak.ret_1x_quality.signal_too_small = true;
    weak.ret_1x_quality.signal_peak_v = 0.0005f;
    weak.ret_hg_quality.usable = false;
    weak.ret_hg_quality.signal_too_small = true;
    weak.ret_hg_quality.signal_peak_v = 0.0005f;
    weak.dsp_status = MEASUREMENT_STATUS_CHANNEL_UNUSABLE;
    failures += expect_true(measurement_auto_submit_result(&session, &weak) == MEASUREMENT_AUTO_EVENT_ATTEMPT_READY,
                            "weak signal retries with higher amplitude");
    failures += expect_true(measurement_auto_next_attempt(&session, &attempt), "weak next attempt");
    failures += expect_amp(attempt.amplitude, HW_EXCITATION_AMP_500MVRMS, "weak escalates to 500m");

    measurement_auto_start_t start = default_start();
    start.hint = (measurement_auto_hint_t){
        .valid = true,
        .range_id = HW_RANGE_ID_10R,
        .frequency = HW_EXCITATION_FREQ_1KHZ,
        .amplitude = HW_EXCITATION_AMP_100MVRMS,
        .ret_channel = MEASUREMENT_RETURN_1X,
    };
    measurement_auto_session_init(&session);
    failures += expect_true(measurement_auto_start_session(&session, &start), "start 10R hint");
    failures += expect_true(measurement_auto_next_attempt(&session, &attempt), "10R hint attempt");
    weak = make_result_for_attempt(&attempt, measurement_complex(10.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    weak.ret_1x_quality.usable = false;
    weak.ret_hg_quality.usable = false;
    weak.dsp_status = MEASUREMENT_STATUS_CHANNEL_UNUSABLE;
    failures += expect_true(measurement_auto_submit_result(&session, &weak) == MEASUREMENT_AUTO_EVENT_FINAL_RESULT,
                            "10R weak does not request forbidden 500m");

    start = default_start();
    measurement_auto_session_init(&session);
    failures += expect_true(measurement_auto_start_session(&session, &start), "start clip test");
    failures += expect_true(measurement_auto_next_attempt(&session, &attempt), "clip attempt");
    measurement_attempt_result_t clipped = make_result_for_attempt(&attempt,
                                                                   measurement_complex(1000.0f, 0.0f),
                                                                   MEASUREMENT_INTERPRET_RESISTIVE);
    clipped.clipped = true;
    clipped.dsp_status = MEASUREMENT_STATUS_CLIPPED;
    failures += expect_true(measurement_auto_submit_result(&session, &clipped) == MEASUREMENT_AUTO_EVENT_ATTEMPT_READY,
                            "clipping requests new range");
    failures += expect_true(measurement_auto_next_attempt(&session, &attempt), "clip next");
    failures += expect_range(attempt.range_id, HW_RANGE_ID_10K, "clipping moves upward");
    return failures;
}

static int test_ret_policy(void)
{
    int failures = 0;
    measurement_attempt_result_t result =
        make_result(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    measurement_ret_policy_result_t policy = measurement_auto_select_ret_channel(&result);
    failures += expect_true(policy.usable && (policy.selected == MEASUREMENT_RETURN_1X), "1X preferred when strong");

    result.ret_1x_quality.signal_peak_v = 0.001f;
    policy = measurement_auto_select_ret_channel(&result);
    failures += expect_true(policy.usable && (policy.selected == MEASUREMENT_RETURN_HG), "HG preferred for weak 1X");

    result.ret_hg_quality.clipped = true;
    result.ret_hg_quality.usable = false;
    policy = measurement_auto_select_ret_channel(&result);
    failures += expect_true(policy.usable && (policy.selected == MEASUREMENT_RETURN_1X), "HG clipped falls back 1X");

    result.ret_1x_quality.usable = false;
    result.ret_hg_quality.usable = false;
    policy = measurement_auto_select_ret_channel(&result);
    failures += expect_true(!policy.usable, "both invalid rejected");
    return failures;
}

static int test_frequency_refinement_and_final(void)
{
    int failures = 0;
    measurement_auto_session_t session;
    measurement_attempt_config_t attempt;
    failures += start_and_get(&session, &attempt);
    measurement_attempt_result_t cap = make_result_for_attempt(&attempt,
                                                               measurement_complex(50.0f, -500.0f),
                                                               MEASUREMENT_INTERPRET_CAPACITIVE);
    failures += expect_true(measurement_auto_submit_result(&session, &cap) == MEASUREMENT_AUTO_EVENT_PARTIAL_RESULT,
                            "capacitive partial before second frequency");
    failures += expect_true(measurement_auto_next_attempt(&session, &attempt), "capacitive refinement");
    failures += expect_freq(attempt.frequency, HW_EXCITATION_FREQ_10KHZ, "second frequency 10k");
    cap = make_result_for_attempt(&attempt,
                                  measurement_complex(50.0f, -500.0f),
                                  MEASUREMENT_INTERPRET_CAPACITIVE);
    failures += expect_true(measurement_auto_submit_result(&session, &cap) == MEASUREMENT_AUTO_EVENT_FINAL_RESULT,
                            "bounded one frequency refinement");
    const measurement_session_result_t *final = measurement_auto_last_result(&session);
    failures += expect_true(final != NULL && final->final, "final exists");
    failures += expect_true(final->classification.interpretation == MEASUREMENT_INTERPRET_CAPACITIVE,
                            "session cap classification");

    failures += start_and_get(&session, &attempt);
    measurement_attempt_result_t res = make_result_for_attempt(&attempt,
                                                               measurement_complex(1000.0f, 5.0f),
                                                               MEASUREMENT_INTERPRET_RESISTIVE);
    failures += expect_true(measurement_auto_submit_result(&session, &res) == MEASUREMENT_AUTO_EVENT_FINAL_RESULT,
                            "resistive no refinement");
    return failures;
}

static int test_classification_vectors(void)
{
    int failures = 0;
    measurement_attempt_result_t hist[3];
    hist[0] = make_result(measurement_complex(1000.0f, 5.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    measurement_session_classification_t c = measurement_auto_classify_session(hist, 1u);
    failures += expect_true(c.interpretation == MEASUREMENT_INTERPRET_RESISTIVE, "ideal resistor");

    hist[0] = make_result(measurement_complex(15.0f, -1000.0f), MEASUREMENT_INTERPRET_CAPACITIVE);
    hist[1] = make_result(measurement_complex(25.0f, -100.0f), MEASUREMENT_INTERPRET_CAPACITIVE);
    c = measurement_auto_classify_session(hist, 2u);
    failures += expect_true(c.interpretation == MEASUREMENT_INTERPRET_CAPACITIVE, "capacitor ESR");

    hist[0] = make_result(measurement_complex(20.0f, 100.0f), MEASUREMENT_INTERPRET_INDUCTIVE);
    hist[1] = make_result(measurement_complex(25.0f, 1000.0f), MEASUREMENT_INTERPRET_INDUCTIVE);
    c = measurement_auto_classify_session(hist, 2u);
    failures += expect_true(c.interpretation == MEASUREMENT_INTERPRET_INDUCTIVE, "inductor winding resistance");

    hist[0] = make_result(measurement_complex(1000.0f, -1000.0f), MEASUREMENT_INTERPRET_CAPACITIVE);
    hist[1] = make_result(measurement_complex(1000.0f, 1000.0f), MEASUREMENT_INTERPRET_INDUCTIVE);
    c = measurement_auto_classify_session(hist, 2u);
    failures += expect_true(c.interpretation == MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN,
                            "ambiguous sign remains mixed");
    return failures;
}

static int test_terminal_and_cancellation(void)
{
    int failures = 0;
    measurement_auto_session_t session;
    measurement_attempt_config_t attempt;
    failures += start_and_get(&session, &attempt);
    measurement_attempt_result_t open_result =
        make_result_for_attempt(&attempt, measurement_complex(0.0f, 0.0f), MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN);
    open_result.dsp_status = MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL;
    open_result.open_like = true;
    failures += expect_true(measurement_auto_submit_result(&session, &open_result) ==
                                MEASUREMENT_AUTO_EVENT_FINAL_RESULT,
                            "open terminal");
    failures += expect_true(measurement_auto_last_result(&session)->status == MEASUREMENT_AUTO_STATUS_OPEN_LIKE,
                            "open status");

    failures += start_and_get(&session, &attempt);
    measurement_attempt_result_t short_result =
        make_result_for_attempt(&attempt, measurement_complex(0.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    short_result.short_like = true;
    failures += expect_true(measurement_auto_submit_result(&session, &short_result) ==
                                MEASUREMENT_AUTO_EVENT_PARTIAL_RESULT,
                            "short reranges before terminal");
    while (measurement_auto_next_attempt(&session, &attempt))
    {
        short_result = make_result_for_attempt(&attempt,
                                               measurement_complex(0.0f, 0.0f),
                                               MEASUREMENT_INTERPRET_RESISTIVE);
        short_result.short_like = true;
        (void)measurement_auto_submit_result(&session, &short_result);
    }
    failures += expect_true(measurement_auto_last_result(&session)->final, "short final");

    failures += start_and_get(&session, &attempt);
    measurement_attempt_result_t safety = make_result_for_attempt(&attempt,
                                                                  measurement_complex(1000.0f, 0.0f),
                                                                  MEASUREMENT_INTERPRET_RESISTIVE);
    safety.safety_abort = true;
    failures += expect_true(measurement_auto_submit_result(&session, &safety) ==
                                MEASUREMENT_AUTO_EVENT_FINAL_RESULT,
                            "safety abort terminal");
    failures += expect_true(measurement_auto_last_result(&session)->status == MEASUREMENT_AUTO_STATUS_SAFETY_ABORT,
                            "safety abort status");

    failures += start_and_get(&session, &attempt);
    failures += expect_true(measurement_auto_cancel(&session) == MEASUREMENT_AUTO_EVENT_FINAL_RESULT,
                            "cancel terminal");
    failures += expect_true(measurement_auto_last_result(&session)->status == MEASUREMENT_AUTO_STATUS_CANCELED,
                            "cancel status");
    return failures;
}

static int test_confidence_and_live_hint(void)
{
    int failures = 0;
    measurement_attempt_result_t result =
        make_result(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    measurement_confidence_result_t confidence =
        measurement_auto_evaluate_confidence(&result, MEASUREMENT_QUALIFICATION_UNQUALIFIED);
    failures += expect_true(confidence.class_id == MEASUREMENT_CONFIDENCE_EXTENDED, "unqualified not nominal");
    failures += expect_true((confidence.reason_flags & MEASUREMENT_AUTO_REASON_UNQUALIFIED) != 0u,
                            "unqualified reason");
    confidence = measurement_auto_evaluate_confidence(&result, MEASUREMENT_QUALIFICATION_NOMINAL);
    failures += expect_true(confidence.class_id == MEASUREMENT_CONFIDENCE_NOMINAL, "nominal when qualified");
    result.clipped = true;
    result.dsp_status = MEASUREMENT_STATUS_CLIPPED;
    confidence = measurement_auto_evaluate_confidence(&result, MEASUREMENT_QUALIFICATION_NOMINAL);
    failures += expect_true(confidence.class_id == MEASUREMENT_CONFIDENCE_REJECTED, "clip rejected");

    measurement_auto_session_t session;
    measurement_attempt_config_t attempt;
    failures += start_and_get(&session, &attempt);
    result = make_result_for_attempt(&attempt, measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    failures += expect_true(measurement_auto_submit_result(&session, &result) == MEASUREMENT_AUTO_EVENT_FINAL_RESULT,
                            "final for hint");
    measurement_auto_hint_t hint = measurement_auto_make_hint(measurement_auto_last_result(&session));
    failures += expect_true(hint.valid, "hint valid");

    measurement_auto_start_t start = default_start();
    start.mode = MEASUREMENT_AUTO_MODE_LIVE;
    start.session_sequence = 43u;
    start.hint = hint;
    measurement_auto_session_init(&session);
    failures += expect_true(measurement_auto_start_session(&session, &start), "live start with hint");
    failures += expect_true(measurement_auto_next_attempt(&session, &attempt), "live hinted attempt");
    failures += expect_true(attempt.reason == MEASUREMENT_ATTEMPT_PREVIOUS_HINT, "hint reason");
    failures += expect_range(attempt.range_id, hint.range_id, "hint range reused");
    return failures;
}

static int test_synthetic_sequence_harness(void)
{
    int failures = 0;
    measurement_auto_session_t session;
    measurement_attempt_config_t attempt;
    failures += start_and_get(&session, &attempt);
    failures += expect_range(attempt.range_id, HW_RANGE_ID_1K, "sequence attempt 1 range");
    failures += expect_freq(attempt.frequency, HW_EXCITATION_FREQ_1KHZ, "sequence attempt 1 freq");

    measurement_attempt_result_t weak_cap = make_result_for_attempt(&attempt,
                                                                    measurement_complex(20.0f, -10000.0f),
                                                                    MEASUREMENT_INTERPRET_CAPACITIVE);
    failures += expect_true(measurement_auto_submit_result(&session, &weak_cap) ==
                                MEASUREMENT_AUTO_EVENT_PARTIAL_RESULT,
                            "sequence rerange event");
    failures += expect_true(measurement_auto_next_attempt(&session, &attempt), "sequence attempt 2");
    failures += expect_range(attempt.range_id, HW_RANGE_ID_10K, "sequence attempt 2 range");

    measurement_attempt_result_t usable_cap = make_result_for_attempt(&attempt,
                                                                      measurement_complex(20.0f, -10000.0f),
                                                                      MEASUREMENT_INTERPRET_CAPACITIVE);
    failures += expect_true(measurement_auto_submit_result(&session, &usable_cap) ==
                                MEASUREMENT_AUTO_EVENT_PARTIAL_RESULT,
                            "sequence refinement event");
    failures += expect_true(measurement_auto_next_attempt(&session, &attempt), "sequence attempt 3");
    failures += expect_range(attempt.range_id, HW_RANGE_ID_10K, "sequence attempt 3 range");
    failures += expect_freq(attempt.frequency, HW_EXCITATION_FREQ_10KHZ, "sequence attempt 3 freq");

    measurement_attempt_result_t refine = make_result_for_attempt(&attempt,
                                                                  measurement_complex(20.0f, -5000.0f),
                                                                  MEASUREMENT_INTERPRET_CAPACITIVE);
    failures += expect_true(measurement_auto_submit_result(&session, &refine) ==
                                MEASUREMENT_AUTO_EVENT_FINAL_RESULT,
                            "sequence final event");
    const measurement_session_result_t *final = measurement_auto_last_result(&session);
    failures += expect_true(final->classification.interpretation == MEASUREMENT_INTERPRET_CAPACITIVE,
                            "sequence final capacitive");
    failures += expect_u32(final->attempt_count, 3u, "sequence attempt count");
    failures += expect_near(final->best_attempt.derived.reactance_ohms, -10000.0f, 0.01f, "partial data retained");
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_initial_probe_and_policy_matrix();
    failures += test_autorange_movements();
    failures += test_no_range_oscillation();
    failures += test_amplitude_and_clipping_policy();
    failures += test_ret_policy();
    failures += test_frequency_refinement_and_final();
    failures += test_classification_vectors();
    failures += test_terminal_and_cancellation();
    failures += test_confidence_and_live_hint();
    failures += test_synthetic_sequence_harness();
    return (failures == 0) ? 0 : 1;
}
