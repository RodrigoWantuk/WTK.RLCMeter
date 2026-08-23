#include "app/app_measurement_session.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    measurement_status_t dsp_status;
    measurement_complex_t z;
    measurement_interpretation_t interpretation;
    float source_peak_v;
    float ret_1x_peak_v;
    float ret_hg_peak_v;
    measurement_return_channel_t selected;
    bool ret_1x_usable;
    bool ret_hg_usable;
    bool clipped;
    bool open_like;
    bool short_like;
    bool phase05_fail;
    hw_metrology_measure_error_t phase05_error;
} fake_outcome_t;

typedef struct
{
    fake_outcome_t outcomes[MEASUREMENT_AUTO_MAX_ATTEMPTS];
    hw_metrology_measure_request_t requests[MEASUREMENT_AUTO_MAX_ATTEMPTS];
    hw_metrology_block_t block;
    uint8_t outcome_count;
    uint8_t start_count;
    uint8_t process_count;
    bool active;
    bool done;
    bool dumpable;
    bool abort_called;
    hw_metrology_measure_error_t error;
} fake_io_t;

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

static fake_outcome_t good_outcome(measurement_complex_t z, measurement_interpretation_t interpretation)
{
    return (fake_outcome_t){
        .dsp_status = MEASUREMENT_STATUS_OK,
        .z = z,
        .interpretation = interpretation,
        .source_peak_v = 0.100f,
        .ret_1x_peak_v = 0.030f,
        .ret_hg_peak_v = 0.030f,
        .selected = MEASUREMENT_RETURN_1X,
        .ret_1x_usable = true,
        .ret_hg_usable = true,
        .clipped = false,
        .open_like = false,
        .short_like = false,
        .phase05_fail = false,
        .phase05_error = HW_METROLOGY_MEASURE_OK,
    };
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

static bsp_status_t fake_start_attempt(const hw_metrology_measure_request_t *request,
                                       uint32_t now_ms,
                                       void *user)
{
    (void)now_ms;
    fake_io_t *fake = (fake_io_t *)user;
    if ((fake == NULL) || (request == NULL) || (fake->start_count >= fake->outcome_count))
    {
        return BSP_STATUS_ERROR;
    }
    fake->requests[fake->start_count] = *request;
    const fake_outcome_t *outcome = &fake->outcomes[fake->start_count];
    fake->start_count++;
    fake->active = true;
    fake->done = false;
    fake->dumpable = !outcome->phase05_fail;
    fake->error = outcome->phase05_error;
    if (outcome->phase05_fail)
    {
        fake->done = true;
        fake->active = false;
    }
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_step_attempt(uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_io_t *fake = (fake_io_t *)user;
    if (fake == NULL)
    {
        return BSP_STATUS_ERROR;
    }
    if (fake->abort_called)
    {
        fake->active = false;
        fake->done = true;
        fake->dumpable = false;
        fake->error = HW_METROLOGY_MEASURE_ERR_ABORT;
        return BSP_STATUS_OK;
    }
    fake->active = false;
    fake->done = true;
    return BSP_STATUS_OK;
}

static bool fake_attempt_active(void *user)
{
    const fake_io_t *fake = (const fake_io_t *)user;
    return (fake != NULL) && fake->active;
}

static bool fake_attempt_done(void *user)
{
    const fake_io_t *fake = (const fake_io_t *)user;
    return (fake != NULL) && fake->done;
}

static bool fake_attempt_dumpable(void *user)
{
    const fake_io_t *fake = (const fake_io_t *)user;
    return (fake != NULL) && fake->dumpable;
}

static const hw_metrology_block_t *fake_attempt_block(void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    return (fake == NULL) ? NULL : &fake->block;
}

static hw_metrology_measure_error_t fake_attempt_error(void *user)
{
    const fake_io_t *fake = (const fake_io_t *)user;
    return (fake == NULL) ? HW_METROLOGY_MEASURE_ERR_INVALID : fake->error;
}

static void fake_attempt_acknowledge(void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    if (fake != NULL)
    {
        fake->active = false;
        fake->done = false;
        fake->dumpable = false;
    }
}

static bsp_status_t fake_attempt_abort(void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    if (fake == NULL)
    {
        return BSP_STATUS_ERROR;
    }
    fake->abort_called = true;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_process_block(const hw_metrology_block_t *block,
                                       const measurement_attempt_config_t *attempt,
                                       measurement_calibrated_result_t *processed,
                                       void *user)
{
    (void)block;
    fake_io_t *fake = (fake_io_t *)user;
    if ((fake == NULL) || (attempt == NULL) || (processed == NULL) ||
        (fake->process_count >= fake->outcome_count))
    {
        return BSP_STATUS_ERROR;
    }
    const fake_outcome_t *outcome = &fake->outcomes[fake->process_count];
    fake->process_count++;
    *processed = (measurement_calibrated_result_t){0};
    measurement_result_t *result = &processed->result;
    processed->provenance = (measurement_calibration_provenance_t){
        .source = MEASUREMENT_CAL_SOURCE_IDEAL,
        .status = MEASUREMENT_CAL_RESOLVE_MISSING,
        .set_sequence = 0u,
        .model_version = MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
        .condition_id = 0u,
        .uncalibrated = true,
    };
    result->status = outcome->dsp_status;
    result->phasors.vexc_1_peak_v = outcome->source_peak_v;
    result->phasors.vexc_2_peak_v = outcome->source_peak_v;
    result->phasors.clipped = outcome->clipped;
    result->ret_1x_quality = (measurement_channel_quality_t){
        .usable = outcome->ret_1x_usable,
        .clipped = outcome->clipped,
        .calibration_valid = true,
        .signal_too_small = outcome->ret_1x_peak_v < 0.002f,
        .signal_peak_v = outcome->ret_1x_peak_v,
    };
    result->ret_hg_quality = (measurement_channel_quality_t){
        .usable = outcome->ret_hg_usable,
        .clipped = outcome->clipped,
        .calibration_valid = true,
        .signal_too_small = outcome->ret_hg_peak_v < 0.002f,
        .signal_peak_v = outcome->ret_hg_peak_v,
    };
    result->selected_channel = outcome->selected;
    result->impedance = (measurement_impedance_result_t){
        .status = outcome->dsp_status,
        .channel = outcome->selected,
        .vs_v = {outcome->source_peak_v, 0.0f},
        .vx_v = {outcome->ret_1x_peak_v, 0.0f},
        .z_ohms = outcome->z,
        .open_like = outcome->open_like,
        .short_like = outcome->short_like,
    };
    result->derived = measurement_derive_quantities(outcome->z,
                                                    frequency_hz(attempt->frequency),
                                                    &(measurement_dsp_config_t){
                                                        .ret_hg_transfer = {15.468085f, 0.0f},
                                                        .zref_ohms =
                                                            measurement_dsp_config_ideal(attempt->range_id).zref_ohms,
                                                        .source_min_v_peak = 0.001f,
                                                        .return_min_v_peak = 0.00005f,
                                                        .denominator_min_v_peak = 0.00005f,
                                                        .reactance_min_ohms = 0.001f,
                                                        .interpretation_ratio_resistive_max = 0.10f,
                                                        .interpretation_ratio_reactive_min = 0.25f,
                                                    },
                                                    outcome->dsp_status);
    result->derived.interpretation = outcome->interpretation;
    return BSP_STATUS_OK;
}

static app_measurement_session_io_t make_io(fake_io_t *fake)
{
    return (app_measurement_session_io_t){
        .start_attempt = fake_start_attempt,
        .step_attempt = fake_step_attempt,
        .attempt_active = fake_attempt_active,
        .attempt_done = fake_attempt_done,
        .attempt_dumpable = fake_attempt_dumpable,
        .attempt_block = fake_attempt_block,
        .attempt_error = fake_attempt_error,
        .attempt_acknowledge = fake_attempt_acknowledge,
        .attempt_abort = fake_attempt_abort,
        .process_block = fake_process_block,
        .user = fake,
    };
}

static int start_session(app_measurement_session_t *session, fake_io_t *fake, measurement_auto_mode_t mode,
                         uint32_t sequence, const measurement_auto_hint_t *hint)
{
    const bsp_clock_summary_t clock = {
        .source = BSP_CLOCK_SOURCE_HSE_PLL,
        .hse_ready = true,
        .sysclk_hz = 72000000u,
    };
    app_measurement_session_io_t io = make_io(fake);
    int failures = 0;
    failures += expect_true(app_measurement_session_init(session, &io) == BSP_STATUS_OK, "app session init");
    failures += expect_true(app_measurement_session_start(session,
                                                         mode,
                                                         sequence,
                                                         MEASUREMENT_QUALIFICATION_UNQUALIFIED,
                                                         hint,
                                                         &clock,
                                                         BSP_STATUS_OK,
                                                         0u) == BSP_STATUS_BUSY,
                            "app session start");
    failures += expect_true(app_measurement_session_step(session, 0u) == APP_MEASUREMENT_EVENT_AUTO_BEGIN,
                            "auto begin event");
    return failures;
}

static app_measurement_event_t run_until_event(app_measurement_session_t *session,
                                               app_measurement_event_t wanted,
                                               uint32_t *now_ms)
{
    for (uint32_t i = 0u; i < 32u; i++)
    {
        const app_measurement_event_t event = app_measurement_session_step(session, (*now_ms)++);
        if ((event != APP_MEASUREMENT_EVENT_NONE) && ((wanted == APP_MEASUREMENT_EVENT_NONE) || (event == wanted)))
        {
            return event;
        }
    }
    return APP_MEASUREMENT_EVENT_NONE;
}

static int test_simple_resistor_final(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    fake.outcome_count = 1u;
    fake.outcomes[0] = good_outcome(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    app_measurement_session_t session;
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 1u, NULL);
    uint32_t now = 1u;
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN, &now) ==
                                APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN,
                            "simple attempt begins");
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                                APP_MEASUREMENT_EVENT_FINAL_RESULT,
                            "simple final");
    const measurement_session_result_t *final = app_measurement_session_final(&session);
    failures += expect_true(final != NULL && final->final, "simple final readable");
    failures += expect_true(final->status == MEASUREMENT_AUTO_STATUS_FINAL_OK, "simple final ok");
    failures += expect_u32(final->primary_attempt_index, 0u, "simple primary index");
    failures += expect_u32(fake.start_count, 1u, "simple one transaction");
    failures += expect_range(fake.requests[0].range_id, HW_RANGE_ID_1K, "simple initial range");
    return failures;
}

static int test_rerange_and_weak_signal(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    fake.outcome_count = 2u;
    fake.outcomes[0] = good_outcome(measurement_complex(20000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    fake.outcomes[1] = good_outcome(measurement_complex(10000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    app_measurement_session_t session;
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 2u, NULL);
    uint32_t now = 1u;
    (void)run_until_event(&session, APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN, &now);
    (void)run_until_event(&session, APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN, &now);
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                                APP_MEASUREMENT_EVENT_FINAL_RESULT,
                            "rerange final");
    failures += expect_u32(fake.start_count, 2u, "rerange two transactions");
    failures += expect_range(fake.requests[1].range_id, HW_RANGE_ID_10K, "rerange moves to 10K");
    failures += expect_u32(app_measurement_session_final(&session)->primary_attempt_index, 1u, "rerange primary");

    fake = (fake_io_t){0};
    fake.outcome_count = 2u;
    fake.outcomes[0] = good_outcome(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    fake.outcomes[0].dsp_status = MEASUREMENT_STATUS_CHANNEL_UNUSABLE;
    fake.outcomes[0].ret_1x_usable = false;
    fake.outcomes[0].ret_hg_usable = false;
    fake.outcomes[0].ret_1x_peak_v = 0.0005f;
    fake.outcomes[0].ret_hg_peak_v = 0.0005f;
    fake.outcomes[1] = good_outcome(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 3u, NULL);
    now = 1u;
    (void)run_until_event(&session, APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN, &now);
    (void)run_until_event(&session, APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN, &now);
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                                APP_MEASUREMENT_EVENT_FINAL_RESULT,
                            "weak final");
    failures += expect_amp(fake.requests[1].amplitude, HW_EXCITATION_AMP_500MVRMS, "weak escalates amplitude");
    return failures;
}

static int test_reactive_primary_survives_refinement(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    fake.outcome_count = 2u;
    fake.outcomes[0] = good_outcome(measurement_complex(50.0f, -500.0f), MEASUREMENT_INTERPRET_CAPACITIVE);
    fake.outcomes[1] = good_outcome(measurement_complex(50.0f, -50.0f), MEASUREMENT_INTERPRET_CAPACITIVE);
    app_measurement_session_t session;
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 4u, NULL);
    uint32_t now = 1u;
    (void)run_until_event(&session, APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN, &now);
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_PARTIAL_RESULT, &now) ==
                                APP_MEASUREMENT_EVENT_PARTIAL_RESULT,
                            "capacitive partial");
    failures += expect_u32(app_measurement_session_partial(&session)->primary_attempt_index,
                           0u,
                           "capacitive primary partial");
    (void)run_until_event(&session, APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN, &now);
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                                APP_MEASUREMENT_EVENT_FINAL_RESULT,
                            "capacitive final");
    failures += expect_u32(app_measurement_session_final(&session)->primary_attempt_index,
                           0u,
                           "capacitive primary remains first");

    fake = (fake_io_t){0};
    fake.outcome_count = 2u;
    fake.outcomes[0] = good_outcome(measurement_complex(50.0f, 500.0f), MEASUREMENT_INTERPRET_INDUCTIVE);
    fake.outcomes[1] = good_outcome(measurement_complex(50.0f, 5000.0f), MEASUREMENT_INTERPRET_INDUCTIVE);
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 5u, NULL);
    now = 1u;
    (void)run_until_event(&session, APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN, &now);
    (void)run_until_event(&session, APP_MEASUREMENT_EVENT_PARTIAL_RESULT, &now);
    (void)run_until_event(&session, APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN, &now);
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                                APP_MEASUREMENT_EVENT_FINAL_RESULT,
                            "inductive final");
    failures += expect_u32(app_measurement_session_final(&session)->primary_attempt_index,
                           0u,
                           "inductive primary not largest magnitude");
    return failures;
}

static int test_open_short_fault_and_cancel(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    fake.outcome_count = 5u;
    for (uint8_t i = 0u; i < fake.outcome_count; i++)
    {
        fake.outcomes[i] = good_outcome(measurement_complex(0.0f, 0.0f), MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN);
        fake.outcomes[i].dsp_status = MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL;
        fake.outcomes[i].open_like = true;
    }
    app_measurement_session_t session;
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 6u, NULL);
    uint32_t now = 1u;
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                                APP_MEASUREMENT_EVENT_FINAL_RESULT,
                            "open final");
    failures += expect_true(app_measurement_session_final(&session)->status == MEASUREMENT_AUTO_STATUS_OPEN_LIKE,
                            "open terminal status");

    fake = (fake_io_t){0};
    fake.outcome_count = 1u;
    fake.outcomes[0] = good_outcome(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    fake.outcomes[0].phase05_fail = true;
    fake.outcomes[0].phase05_error = HW_METROLOGY_MEASURE_ERR_PERMIT;
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 7u, NULL);
    now = 1u;
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                                APP_MEASUREMENT_EVENT_FINAL_RESULT,
                            "safety final");
    failures += expect_true(app_measurement_session_final(&session)->status == MEASUREMENT_AUTO_STATUS_SAFETY_ABORT,
                            "safety abort mapped");

    fake = (fake_io_t){0};
    fake.outcome_count = 1u;
    fake.outcomes[0] = good_outcome(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 8u, NULL);
    now = 1u;
    (void)run_until_event(&session, APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN, &now);
    failures += expect_true(app_measurement_session_cancel(&session) == BSP_STATUS_BUSY, "cancel while active");
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                                APP_MEASUREMENT_EVENT_FINAL_RESULT,
                            "cancel final");
    failures += expect_true(fake.abort_called, "phase05 abort requested");
    failures += expect_true(app_measurement_session_final(&session)->status == MEASUREMENT_AUTO_STATUS_CANCELED,
                            "cancel status");

    fake = (fake_io_t){0};
    fake.outcome_count = 1u;
    fake.outcomes[0] = good_outcome(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 81u, NULL);
    failures += expect_true(app_measurement_session_cancel(&session) == BSP_STATUS_OK, "cancel before first attempt");
    failures += expect_true(app_measurement_session_step(&session, 1u) == APP_MEASUREMENT_EVENT_FINAL_RESULT,
                            "pre-attempt cancel final event");
    failures += expect_true(fake.start_count == 0u, "pre-attempt cancel starts no hardware");
    failures += expect_true(app_measurement_session_final(&session)->status == MEASUREMENT_AUTO_STATUS_CANCELED,
                            "pre-attempt cancel status");

    fake = (fake_io_t){0};
    fake.outcome_count = 2u;
    fake.outcomes[0] = good_outcome(measurement_complex(50.0f, -500.0f), MEASUREMENT_INTERPRET_CAPACITIVE);
    fake.outcomes[1] = good_outcome(measurement_complex(50.0f, -50.0f), MEASUREMENT_INTERPRET_CAPACITIVE);
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 82u, NULL);
    now = 1u;
    (void)run_until_event(&session, APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN, &now);
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_PARTIAL_RESULT, &now) ==
                                APP_MEASUREMENT_EVENT_PARTIAL_RESULT,
                            "partial before cancel");
    failures += expect_true(app_measurement_session_cancel(&session) == BSP_STATUS_OK, "cancel after partial");
    failures += expect_true(app_measurement_session_step(&session, now) == APP_MEASUREMENT_EVENT_FINAL_RESULT,
                            "post-partial cancel final event");
    failures += expect_true(fake.start_count == 1u, "post-partial cancel starts no second hardware attempt");
    failures += expect_true(app_measurement_session_final(&session)->status == MEASUREMENT_AUTO_STATUS_CANCELED,
                            "post-partial cancel status");
    return failures;
}

static int test_live_refresh_hint_boundary(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    fake.outcome_count = 1u;
    fake.outcomes[0] = good_outcome(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    app_measurement_session_t session;
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 9u, NULL);
    uint32_t now = 1u;
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                                APP_MEASUREMENT_EVENT_FINAL_RESULT,
                            "first live seed final");
    measurement_auto_hint_t hint = measurement_auto_make_hint(app_measurement_session_final(&session));
    failures += expect_true(hint.valid, "live hint valid");

    fake = (fake_io_t){0};
    fake.outcome_count = 1u;
    fake.outcomes[0] = good_outcome(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_LIVE, 10u, &hint);
    now = 1u;
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN, &now) ==
                                APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN,
                            "live second attempt begins");
    const measurement_attempt_config_t *attempt = app_measurement_session_current_attempt(&session);
    failures += expect_true((attempt != NULL) && (attempt->reason == MEASUREMENT_ATTEMPT_PREVIOUS_HINT),
                            "live hint is performance hint");
    failures += expect_range(fake.requests[0].range_id, hint.range_id, "live hint range reused");
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                                APP_MEASUREMENT_EVENT_FINAL_RESULT,
                            "live second final");
    failures += expect_u32(app_measurement_session_final(&session)->session_sequence, 10u, "live new sequence");
    return failures;
}

int main(int argc, char **argv)
{
    if ((argc == 2) && (strcmp(argv[1], "--sizes") == 0))
    {
        (void)printf("measurement_auto_session_t=%lu\n",
                     (unsigned long)measurement_auto_session_size_bytes());
        (void)printf("app_measurement_session_t=%lu\n",
                     (unsigned long)app_measurement_session_size_bytes());
        return 0;
    }

    int failures = 0;
    failures += test_simple_resistor_final();
    failures += test_rerange_and_weak_signal();
    failures += test_reactive_primary_survives_refinement();
    failures += test_open_short_fault_and_cancel();
    failures += test_live_refresh_hint_boundary();
    return (failures == 0) ? 0 : 1;
}
