#include "app/app_measurement_session.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

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
    uint8_t provenance_variant;
    bool processing_missing;
    hw_metrology_measure_error_t phase05_error;
} fake_outcome_t;

typedef struct
{
    app_measurement_session_io_t io;
    fake_outcome_t outcomes[MEASUREMENT_AUTO_HISTORY_CAPACITY + MEASUREMENT_TRIMMED_MAX_EXTRA_CAPTURES];
    hw_metrology_measure_request_t requests[MEASUREMENT_AUTO_HISTORY_CAPACITY + MEASUREMENT_TRIMMED_MAX_EXTRA_CAPTURES];
    hw_metrology_block_t block;
    uint8_t outcome_count;
    uint8_t start_count;
    uint8_t process_count;
    bool active;
    bool done;
    bool dumpable;
    bool abort_called;
    hw_metrology_measure_error_t error;
#if WTK_ENABLE_MEASURE_SESSION_HOLD
    bool hold_mode;
    bool qualified_calibration;
#if WTK_ENABLE_FREQUENCY_SURVEY
    bool synthetic_survey;
    float survey_r;
    float survey_c;
    float survey_l;
#endif
    bool hold_waiting;
    bool closing;
    bool cleanup_fail;
    bool supervision_fail;
    uint8_t cleanup_steps;
    uint32_t finish_calls;
#endif
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
    case HW_EXCITATION_FREQ_10HZ:
        return 10u;
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
#if WTK_ENABLE_MEASURE_SESSION_HOLD
    fake->hold_waiting = false;
    fake->closing = false;
    fake->cleanup_steps = 0u;
#endif
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
#if WTK_ENABLE_MEASURE_SESSION_HOLD
    if (fake->hold_mode)
    {
#if WTK_ENABLE_FREQUENCY_SURVEY
        if (fake->synthetic_survey && fake->start_count != 0u &&
            fake->requests[fake->start_count - 1u].frequency == HW_EXCITATION_FREQ_10HZ)
        {
            fake->active = false;
            fake->done = true;
            return BSP_STATUS_OK;
        }
#endif
        if (fake->closing)
        {
            fake->cleanup_steps++;
            if (fake->cleanup_steps < 2u) return BSP_STATUS_BUSY;
            fake->active = false;
            fake->done = false;
            fake->error = fake->cleanup_fail ? HW_METROLOGY_MEASURE_ERR_AUX_RESTORE : HW_METROLOGY_MEASURE_OK;
            return fake->cleanup_fail ? BSP_STATUS_ERROR : BSP_STATUS_OK;
        }
        if (fake->hold_waiting && fake->supervision_fail)
        {
            fake->abort_called = true;
            fake->error = HW_METROLOGY_MEASURE_ERR_ABORT;
            return BSP_STATUS_ERROR;
        }
        if (!fake->hold_waiting)
        {
            fake->hold_waiting = true;
            fake->done = true;
        }
        return BSP_STATUS_BUSY;
    }
#endif
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
#if WTK_ENABLE_MEASURE_SESSION_HOLD
        if (fake->hold_mode && fake->active)
        {
            fake->done = false;
            fake->dumpable = false;
            return;
        }
#endif
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
    if (outcome->processing_missing)
    {
        fake->process_count++;
        return BSP_STATUS_ERROR;
    }
#if WTK_ENABLE_FREQUENCY_SURVEY
    fake_outcome_t synthetic;
    if (fake->synthetic_survey)
    {
        const float omega = 6.28318530718f * (float)frequency_hz(attempt->frequency);
        const measurement_complex_t z = measurement_complex(fake->survey_r,
            fake->survey_c > 0.0f ? -1.0f / (omega * fake->survey_c) : omega * fake->survey_l);
        const measurement_dsp_config_t config = measurement_dsp_config_ideal(attempt->range_id);
        const measurement_derived_result_t derived = measurement_derive_quantities(z,
            frequency_hz(attempt->frequency), &config, MEASUREMENT_STATUS_OK);
        synthetic = good_outcome(z, derived.interpretation);
        synthetic.ret_1x_peak_v = 0.1f * measurement_complex_mag(z) /
            measurement_complex_mag(measurement_complex_add(config.zref_ohms, z));
        synthetic.ret_hg_peak_v = synthetic.ret_1x_peak_v;
        synthetic.short_like = synthetic.ret_1x_peak_v < 0.00005f;
        if (synthetic.short_like)
        {
            synthetic.dsp_status = MEASUREMENT_STATUS_CHANNEL_UNUSABLE;
            synthetic.ret_1x_usable = false;
            synthetic.ret_hg_usable = false;
            synthetic.short_like = false;
        }
        outcome = &synthetic;
    }
#endif
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
#if WTK_ENABLE_MEASUREMENT_TRIMMED
    if (fake->io.trimmed_enabled)
    {
        const measurement_cal_key_t key = measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
            MEASUREMENT_CAL_MODEL_VERSION_CURRENT, attempt->range_id, attempt->frequency, attempt->amplitude);
        processed->provenance.source = MEASUREMENT_CAL_SOURCE_PERSISTED;
        processed->provenance.status = fake->qualified_calibration ?
            MEASUREMENT_CAL_RESOLVE_FOUND : MEASUREMENT_CAL_RESOLVE_UNQUALIFIED;
        processed->provenance.set_sequence = 1u + outcome->provenance_variant;
        processed->provenance.condition_id = measurement_cal_condition_id(&key);
        processed->provenance.uncalibrated = !fake->qualified_calibration;
    }
#endif
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
#if WTK_ENABLE_FREQUENCY_SURVEY
    if (fake->synthetic_survey && outcome->dsp_status != MEASUREMENT_STATUS_OK) return BSP_STATUS_ERROR;
#endif
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
    fake->io = make_io(fake);
    int failures = 0;
    failures += expect_true(app_measurement_session_init(session, &fake->io) == BSP_STATUS_OK, "app session init");
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
    for (uint32_t i = 0u; i < 128u; i++)
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

#if WTK_ENABLE_MEASURE_SESSION_HOLD
static bsp_status_t fake_finish_session(void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    fake->finish_calls++;
    fake->closing = true;
    return BSP_STATUS_BUSY;
}

static int test_final_result_waits_for_session_cleanup(void)
{
    int failures = 0;
    for (uint8_t failure = 0u; failure < 6u; failure++)
    {
        app_measurement_session_t session;
        fake_io_t fake = {0};
        fake.hold_mode = true;
        fake.outcome_count = 1u;
        fake.outcomes[0] = good_outcome(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
        failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 23u, NULL);
        fake.io.finish_session = fake_finish_session;
        uint32_t now = 1u;
        for (uint32_t i = 0u; i < 30u && session.state != APP_MEASUREMENT_SESSION_FINISHING; i++)
        {
            (void)app_measurement_session_step(&session, now++);
            if (failure >= 3u && fake.hold_waiting)
            {
                if (failure == 3u) fake.supervision_fail = true;
                if (failure == 4u) session.state = (app_measurement_session_state_t)255;
                if (failure == 5u)
                {
                    session.state = APP_MEASUREMENT_SESSION_START_ATTEMPT;
                    session.policy.has_next_attempt = false;
                }
                break;
            }
        }
        failures += expect_true(fake.requests[0].session_hold, "automatic request explicitly opts into session ownership");
        failures += expect_true(fake.active && !app_measurement_session_final_ready(&session),
                                "no final result escapes while K1 cleanup is pending");
        if (session.state == APP_MEASUREMENT_SESSION_FINISHING)
            failures += expect_true(strcmp(app_measurement_session_state_string(session.state), "FINISHING") == 0,
                                    "cleanup ownership has an explicit diagnostic state");
        if (failure == 1u) fake.cleanup_fail = true;
        if (failure == 2u) (void)app_measurement_session_cancel(&session);
        failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                                    APP_MEASUREMENT_EVENT_FINAL_RESULT,
                                "held session eventually produces a final event after cleanup");
        failures += expect_true(!fake.active && app_measurement_session_final_ready(&session),
                                "final event implies released hardware ownership");
        const measurement_session_result_t *result = app_measurement_session_final(&session);
        const measurement_auto_status_t expected = (failure == 0u) ? MEASUREMENT_AUTO_STATUS_FINAL_OK :
            (failure == 2u) ? MEASUREMENT_AUTO_STATUS_CANCELED : MEASUREMENT_AUTO_STATUS_SAFETY_ABORT;
        failures += expect_true(result != NULL && result->status == expected,
                                "late cleanup fault or cancellation revokes provisional success");
        if (failure != 0u)
            failures += expect_true(result->confidence.publication_confidence == MEASUREMENT_CONFIDENCE_REJECTED,
                                    "revoked result cannot be published as a valid measurement");
    }
    return failures;
}

static int test_held_partial_continuation_and_cancel(void)
{
    int failures = 0;
    for (uint8_t cancel = 0u; cancel < 2u; cancel++)
    {
        fake_io_t fake = {0};
        app_measurement_session_t session;
        fake.hold_mode = true;
        fake.outcome_count = 2u;
        /* Both frequencies stay inside the 1-kohm reference ratio window. */
        fake.outcomes[0] = good_outcome(measurement_complex(20.0f, -3000.0f), MEASUREMENT_INTERPRET_CAPACITIVE);
        fake.outcomes[1] = good_outcome(measurement_complex(20.0f, -300.0f), MEASUREMENT_INTERPRET_CAPACITIVE);
        failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 24u, NULL);
        fake.io.finish_session = fake_finish_session;
        uint32_t now = 1u;
        failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_PARTIAL_RESULT, &now) ==
                                    APP_MEASUREMENT_EVENT_PARTIAL_RESULT && fake.active &&
                                    !app_measurement_session_final_ready(&session),
                                "partial result keeps ownership for the bounded second-frequency check");
        if (cancel != 0u) (void)app_measurement_session_cancel(&session);
        failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                                    APP_MEASUREMENT_EVENT_FINAL_RESULT && !fake.active,
                                "continuation or between-attempt cancel reaches SAFE before final event");
        failures += expect_u32(fake.start_count, (cancel == 0u) ? 2u : 1u, "cancellation cannot start another capture");
        failures += expect_true(app_measurement_session_final(&session)->status ==
                                    ((cancel == 0u) ? MEASUREMENT_AUTO_STATUS_FINAL_OK : MEASUREMENT_AUTO_STATUS_CANCELED),
                                "held partial result receives the correct final disposition");
    }
    return failures;
}
#endif

#if WTK_ENABLE_MEASUREMENT_TRIMMED
static int test_trimmed_session(void)
{
    int failures = 0;
    for (uint8_t variant = 0u; variant < 6u; variant++)
    {
        app_measurement_session_t session;
        fake_io_t fake = {0};
        fake.hold_mode = true;
        fake.qualified_calibration = variant == 0u;
        fake.outcome_count = 9u;
        for (uint8_t i = 0u; i < fake.outcome_count; i++)
            fake.outcomes[i] = good_outcome(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
        fake.outcomes[1].z.re = 9000.0f;
        fake.outcomes[2].z.re = -9000.0f;
        fake.outcomes[3].z.re = 1002.0f;
        fake.outcomes[4].z.re = 1001.0f;
        fake.outcomes[5].z.re = 9100.0f;
        fake.outcomes[6].z.re = -9100.0f;
        if (variant == 1u)
        {
            fake.outcomes[1].selected = MEASUREMENT_RETURN_HG;
            fake.outcomes[2].provenance_variant = 1u;
        }
        if (variant == 2u)
            for (uint8_t i = 1u; i < fake.outcome_count; i++) fake.outcomes[i].clipped = true;
        failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 31u + variant, NULL);
        fake.io.finish_session = fake_finish_session;
        fake.io.trimmed_enabled = true;
        uint32_t now = 1u;
        for (uint32_t i = 0u; i < 128u && !app_measurement_session_final_ready(&session); i++)
        {
            (void)app_measurement_session_step(&session, now++);
            if (session.trimming && variant == 3u) (void)app_measurement_session_cancel(&session);
            if (session.trimming && variant == 5u) fake.supervision_fail = true;
            if (variant == 4u && session.state == APP_MEASUREMENT_SESSION_FINISHING) fake.cleanup_fail = true;
            if (fake.active)
                failures += expect_true(!app_measurement_session_final_ready(&session), "filtered final waits for SAFE cleanup");
        }
        const measurement_session_result_t *result = app_measurement_session_final(&session);
        failures += expect_true(result != NULL && !fake.active, "trimmed session always terminates and releases owner");
        if (result == NULL) continue;
        failures += expect_true(result->trimmed.requested && result->attempt_count == 1u &&
                                session.policy.history[0].z_ohms.re == 1000.0f,
                                "statistical windows never overwrite raw search history");
        const bool success = variant <= 1u;
        failures += expect_true(result->trimmed.applied == success, "only a complete safe estimate is marked applied");
        if (success)
        {
            failures += expect_u32(result->trimmed.samples, 7u, "exactly seven admitted samples");
            failures += expect_u32(fake.start_count, variant == 0u ? 7u : 9u, "bounded mismatched-path and provenance rejection");
            failures += expect_true(result->status == MEASUREMENT_AUTO_STATUS_FINAL_OK &&
                result->primary_attempt.derived.valid &&
                result->confidence.publication_confidence == MEASUREMENT_CONFIDENCE_LOW_CONFIDENCE,
                "averaging preserves unqualified confidence and derives one result");
            if (variant == 0u)
                failures += expect_true(fabsf(result->primary_attempt.z_ohms.re - 1001.0f) < 0.001f &&
                                        result->trimmed.r_retained_mask == 25u,
                                        "two high and two low coordinates excluded");
        }
        else
        {
            failures += expect_true(result->confidence.publication_confidence == MEASUREMENT_CONFIDENCE_REJECTED,
                                    "incomplete canceled or faulty filter cannot fall back to raw result");
            const measurement_auto_status_t expected = variant == 2u ? MEASUREMENT_AUTO_STATUS_FAILED :
                variant == 3u ? MEASUREMENT_AUTO_STATUS_CANCELED : MEASUREMENT_AUTO_STATUS_SAFETY_ABORT;
            failures += expect_true(result->status == expected, "filtered failure has explicit terminal disposition");
        }
        for (uint8_t i = 1u; i < fake.start_count; i++)
            failures += expect_true(fake.requests[i].statistical_capture && fake.requests[i].session_hold &&
                fake.requests[i].range_id == HW_RANGE_ID_1K && fake.requests[i].frequency == HW_EXCITATION_FREQ_1KHZ,
                "all statistical windows use explicit role and primary condition");
        failures += expect_true(!fake.requests[0].statistical_capture, "initial search is never a statistical authorization");
    }
    return failures;
}

static int test_trimmed_primary_after_second_frequency(void)
{
    int failures = 0;
    app_measurement_session_t session;
    fake_io_t fake = {0};
    fake.hold_mode = true;
    fake.outcome_count = 8u;
    for (uint8_t i = 0u; i < fake.outcome_count; i++)
        fake.outcomes[i] = good_outcome(measurement_complex(20.0f, -3000.0f), MEASUREMENT_INTERPRET_CAPACITIVE);
    fake.outcomes[1].z.im = -300.0f;
    failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 38u, NULL);
    fake.io.finish_session = fake_finish_session;
    fake.io.trimmed_enabled = true;
    uint32_t now = 1u;
    failures += expect_true(run_until_event(&session, APP_MEASUREMENT_EVENT_FINAL_RESULT, &now) ==
                            APP_MEASUREMENT_EVENT_FINAL_RESULT, "reactive aggregate completes after frequency refinement");
    const measurement_session_result_t *result = app_measurement_session_final(&session);
    failures += expect_true(result != NULL && result->trimmed.applied && result->attempt_count == 2u &&
                            result->primary_attempt.config.frequency == HW_EXCITATION_FREQ_1KHZ &&
                            result->classification.interpretation == MEASUREMENT_INTERPRET_CAPACITIVE &&
                            result->primary_attempt.derived.capacitance_valid,
                            "C derives from selected primary while secondary evidence remains raw");
    failures += expect_true(fake.requests[1].frequency == HW_EXCITATION_FREQ_10KHZ &&
                            fake.requests[2].frequency == HW_EXCITATION_FREQ_1KHZ &&
                            fake.requests[2].statistical_capture, "first statistical window returns to primary frequency");
    return failures;
}
#endif

#if WTK_ENABLE_FREQUENCY_SURVEY
static int test_complete_survey_and_statistical_segments(void)
{
    int failures = 0;
    for (uint8_t variant = 0u; variant < 6u; variant++)
    {
        app_measurement_session_t session;
        fake_io_t fake = {0};
        fake.hold_mode = true;
        fake.synthetic_survey = true;
        fake.survey_r = variant == 0u ? 0.07f : 7.5f;
        fake.survey_c = variant == 0u ? 0.0047f : 0.0f;
        fake.survey_l = variant == 0u ? 0.0f : 0.000470f;
        fake.outcome_count = MEASUREMENT_AUTO_HISTORY_CAPACITY + MEASUREMENT_TRIMMED_MAX_EXTRA_CAPTURES;
        for (uint8_t i = 0u; i < fake.outcome_count; i++)
            fake.outcomes[i] = good_outcome(measurement_complex(10.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
        if (variant == 5u) fake.outcomes[1].processing_missing = true;
        failures += start_session(&session, &fake, MEASUREMENT_AUTO_MODE_CLICK, 40u + variant, NULL);
        fake.io.finish_session = fake_finish_session;
        fake.io.trimmed_enabled = true;
        fake.io.frequency_survey_enabled = true;
        /* start_session uses the legacy policy to keep pre-existing tests
         * unchanged; restart this not-yet-captured policy with survey opt-in. */
        const measurement_auto_start_t start = {.frequency_survey = true,
            .qualification = MEASUREMENT_QUALIFICATION_UNQUALIFIED, .session_sequence = 40u + variant};
        (void)measurement_auto_start_session(&session.policy, &start);
        uint32_t now = 1u;
        bool saw_draining = false;
        for (uint32_t i = 0u; i < 512u && !app_measurement_session_final_ready(&session); i++)
        {
            const app_measurement_event_t event = app_measurement_session_step(&session, now++);
            if (app_measurement_session_final_ready(&session))
                failures += expect_true(event == APP_MEASUREMENT_EVENT_FINAL_RESULT,
                    "final event is delivered on terminal step, including already-SAFE 10-Hz capture");
            if (session.state == APP_MEASUREMENT_SESSION_DRAINING)
            {
                saw_draining = true;
                failures += expect_true(fake.active && !app_measurement_session_final_ready(&session),
                                        "frequency handoff drains owner before new permit");
                if (variant == 2u) (void)app_measurement_session_cancel(&session);
                if (variant == 3u) fake.cleanup_fail = true;
                if (variant == 4u) now = session.survey_deadline_ms;
            }
        }
        const measurement_session_result_t *result = app_measurement_session_final(&session);
        failures += expect_true(result != NULL && !fake.active && (saw_draining || variant == 5u),
                                "bounded survey completes or aborts through safe ownership handoff");
        if (result == NULL) continue;
        if (variant >= 2u)
        {
            failures += expect_true(result->status == (variant == 2u ? MEASUREMENT_AUTO_STATUS_CANCELED :
                                    variant == 5u ? MEASUREMENT_AUTO_STATUS_FAILED :
                                                    MEASUREMENT_AUTO_STATUS_SAFETY_ABORT) && !result->trimmed.applied,
                                    "draining cancellation cleanup fault and overall deadline revoke publication");
            continue;
        }
        failures += expect_true(result->status == MEASUREMENT_AUTO_STATUS_FINAL_OK && result->trimmed.applied &&
                                result->trimmed.samples == 7u && result->trimmed.extra_captures == 6u &&
                                result->frequency_attempted_mask == 7u,
                                "all-frequency final receives exactly seven valid primary samples");
        const hw_excitation_freq_t primary = variant == 0u ? HW_EXCITATION_FREQ_10HZ : HW_EXCITATION_FREQ_1KHZ;
        failures += expect_true(result->primary_attempt.config.frequency == primary &&
            result->classification.interpretation == (variant == 0u ? MEASUREMENT_INTERPRET_CAPACITIVE :
                                                                    MEASUREMENT_INTERPRET_INDUCTIVE),
            "large-C and inductive evidence choose suitable primary frequency");
        for (uint8_t i = result->attempt_count; i < fake.start_count; i++)
        {
            failures += expect_true(fake.requests[i].frequency == primary, "statistical windows never mix frequencies");
            if (primary == HW_EXCITATION_FREQ_10HZ || i == result->attempt_count)
                failures += expect_true(!fake.requests[i].statistical_capture,
                                        "fresh segment enters normal real-permit path before statistical continuation");
        }
        if (variant == 0u)
            failures += expect_true(fabsf(result->primary_attempt.derived.capacitance_f - 0.0047f) < 0.000001f,
                                    "10-Hz aggregate derives capacitance at 10 Hz, never 1 kHz");
    }
    return failures;
}
#endif

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
#if WTK_ENABLE_MEASURE_SESSION_HOLD
    failures += test_final_result_waits_for_session_cleanup();
    failures += test_held_partial_continuation_and_cancel();
#if WTK_ENABLE_MEASUREMENT_TRIMMED
    failures += test_trimmed_session();
    failures += test_trimmed_primary_after_second_frequency();
#if WTK_ENABLE_FREQUENCY_SURVEY
    failures += test_complete_survey_and_statistical_segments();
#endif
#endif
#endif
    return (failures == 0) ? 0 : 1;
}
