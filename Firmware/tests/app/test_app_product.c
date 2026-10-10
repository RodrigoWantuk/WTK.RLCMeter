#include "app/app_product.h"
#include "app/app_io_workspace.h"
#include "storage/resource_store.h"
#include "ui/ui_text_catalog.h"

#include <stdio.h>
#include "wtk_build_config.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "ui/ui_format.h"

typedef struct
{
    measurement_complex_t z;
    measurement_interpretation_t interpretation;
    bool fail_phase05;
    uint8_t path_faults;
    uint8_t invalid_quantity;
    uint8_t electrical_model; /* 1 series C, 2 series L; otherwise complex Z. */
    float reactive_value;
} fake_outcome_t;

typedef struct
{
    app_measurement_session_io_t measurement_io;
    app_cal_session_io_t calibration_io;
    fake_outcome_t outcomes[MEASUREMENT_AUTO_MAX_ATTEMPTS];
    hw_metrology_measure_request_t requests[MEASUREMENT_AUTO_MAX_ATTEMPTS];
    hw_metrology_block_t block;
    uint8_t outcome_count;
    uint8_t start_count;
    uint8_t process_count;
    uint8_t ack_count;
    uint8_t abort_delay_steps;
    bool active;
    bool done;
    bool dumpable;
    bool abort_called;
} fake_io_t;

static app_calibration_service_t g_service;
static app_settings_service_t g_settings;
static app_io_workspace_t g_workspace;

static app_product_inputs_t inputs_ready(void);
static bsp_status_t init_product(app_product_t *product, fake_io_t *fake);
static void boot_to_ready(app_product_t *product, const app_product_inputs_t *inputs);
static void send_button(app_product_t *product, button_id_t button, button_event_type_t type);
static void send_button_at(app_product_t *product, button_id_t button, button_event_type_t type, uint32_t now_ms);
static void click_ok(app_product_t *product);

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
        (void)fprintf(stderr, "FAIL: %s (got %lu expected %lu)\n", message, (unsigned long)actual, (unsigned long)expected);
        return 1;
    }
    return 0;
}

static fake_outcome_t good_outcome(measurement_complex_t z, measurement_interpretation_t interpretation)
{
    return (fake_outcome_t){
        .z = z,
        .interpretation = interpretation,
        .fail_phase05 = false,
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

static bsp_status_t fake_start_attempt(const hw_metrology_measure_request_t *request, uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_io_t *fake = (fake_io_t *)user;
    if ((fake == NULL) || (request == NULL) || (fake->start_count >= fake->outcome_count))
    {
        return BSP_STATUS_ERROR;
    }
    fake->requests[fake->start_count] = *request;
    fake->dumpable = !fake->outcomes[fake->start_count].fail_phase05;
    fake->start_count++;
    fake->active = !fake->outcomes[fake->start_count - 1u].fail_phase05;
    fake->done = fake->outcomes[fake->start_count - 1u].fail_phase05;
    if (fake->active)
    {
        if (app_io_workspace_acquire(&g_workspace, APP_IO_WORKSPACE_OWNER_METROLOGY) != BSP_STATUS_OK)
        {
            return BSP_STATUS_BUSY;
        }
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
        if (fake->abort_delay_steps > 0u)
        {
            fake->abort_delay_steps--;
            return BSP_STATUS_BUSY;
        }
        fake->active = false;
        fake->done = true;
        fake->dumpable = false;
        return BSP_STATUS_OK;
    }
    fake->active = false;
    fake->done = true;
    return BSP_STATUS_OK;
}

static bool fake_attempt_active(void *user)
{
    return ((const fake_io_t *)user)->active;
}

static bool fake_attempt_done(void *user)
{
    return ((const fake_io_t *)user)->done;
}

static bool fake_attempt_dumpable(void *user)
{
    return ((const fake_io_t *)user)->dumpable;
}

static const hw_metrology_block_t *fake_attempt_block(void *user)
{
    return &((fake_io_t *)user)->block;
}

static hw_metrology_measure_error_t fake_attempt_error(void *user)
{
    (void)user;
    return HW_METROLOGY_MEASURE_ERR_PERMIT;
}

static void fake_attempt_acknowledge(void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    fake->ack_count++;
    fake->active = false;
    fake->done = false;
    fake->dumpable = false;
    if (app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_METROLOGY)
    {
        (void)app_io_workspace_release(&g_workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
    }
}

static bsp_status_t fake_attempt_abort(void *user)
{
    ((fake_io_t *)user)->abort_called = true;
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
    const fake_outcome_t *outcome = &fake->outcomes[fake->process_count++];
    *processed = (measurement_calibrated_result_t){0};
    processed->provenance = (measurement_calibration_provenance_t){
        .source = MEASUREMENT_CAL_SOURCE_PERSISTED,
        .status = MEASUREMENT_CAL_RESOLVE_UNQUALIFIED,
        .model_version = MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
        .uncalibrated = true,
        .set_sequence = 1u,
        .condition_id = measurement_cal_condition_id(&(const measurement_cal_key_t){
            .hardware_revision = MEASUREMENT_CAL_HARDWARE_REV1,
            .model_version = MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
            .range_id = attempt->range_id, .frequency = attempt->frequency, .amplitude = attempt->amplitude}),
    };
    measurement_result_t *result = &processed->result;
    result->status = MEASUREMENT_STATUS_OK;
    result->phasors.vexc_1_peak_v = 0.100f;
    result->phasors.vexc_2_peak_v = 0.100f;
    result->ret_1x_quality = (measurement_channel_quality_t){
        .usable = true,
        .calibration_valid = true,
        .signal_peak_v = 0.030f,
    };
    result->ret_hg_quality = result->ret_1x_quality;
    result->selected_channel = MEASUREMENT_RETURN_1X;
    measurement_complex_t z = outcome->z;
    const float omega = 6.28318530717958647692f * (float)frequency_hz(attempt->frequency);
    if (outcome->electrical_model == 1u) z.im = -1.0f / (omega * outcome->reactive_value);
    if (outcome->electrical_model == 2u) z.im = omega * outcome->reactive_value;
    result->impedance = (measurement_impedance_result_t){
        .status = MEASUREMENT_STATUS_OK,
        .channel = MEASUREMENT_RETURN_1X,
        .vs_v = {0.100f, 0.0f},
        .vx_v = {0.030f, 0.0f},
        .z_ohms = z,
    };
    const measurement_dsp_config_t config = measurement_dsp_config_ideal(attempt->range_id);
    result->derived = measurement_derive_quantities(z,
                                                    frequency_hz(attempt->frequency),
                                                    &config,
                                                    MEASUREMENT_STATUS_OK);
    result->derived.interpretation = outcome->interpretation;
    if (outcome->path_faults & 1u) { result->ret_hg_quality.clipped = true; result->ret_hg_quality.usable = false; }
    if (outcome->path_faults & 2u) { result->ret_1x_quality.clipped = true; result->ret_1x_quality.usable = false; }
    if (outcome->path_faults & 4u) result->selected_channel = MEASUREMENT_RETURN_HG;
    if (outcome->invalid_quantity == 1u) result->derived.q = NAN;
    if (outcome->invalid_quantity == 2u) result->derived.d = INFINITY;
    if (outcome->invalid_quantity == 3u) result->derived.capacitance_f = -1.0f;
    if (outcome->invalid_quantity == 4u) result->derived.inductance_h = NAN;
    if (outcome->invalid_quantity == 5u) result->derived.phase_rad = NAN;
    if (outcome->invalid_quantity == 6u) processed->provenance.condition_id++;
    if (outcome->invalid_quantity == 7u) processed->provenance.model_version++;
    if (outcome->invalid_quantity == 8u) processed->provenance.set_sequence++;
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

static app_cal_session_io_t make_cal_io(fake_io_t *fake)
{
    return (app_cal_session_io_t){
        .start_capture = fake_start_attempt,
        .step_capture = fake_step_attempt,
        .capture_active = fake_attempt_active,
        .capture_done = fake_attempt_done,
        .capture_dumpable = fake_attempt_dumpable,
        .capture_block = fake_attempt_block,
        .capture_error = fake_attempt_error,
        .capture_acknowledge = fake_attempt_acknowledge,
        .capture_abort = fake_attempt_abort,
        .user = fake,
    };
}

static void init_test_cal_service(void)
{
    app_calibration_service_init(&g_service);
    g_settings = (app_settings_service_t){0};
    app_settings_service_use_defaults(&g_settings);
    app_io_workspace_init(&g_workspace);
    app_calibration_service_attach_workspace(&g_service, &g_workspace);
}

static void step_until_attempt_active(app_product_t *product,
                                      app_product_inputs_t *inputs,
                                      const bsp_clock_summary_t *clock,
                                      uint32_t start_ms,
                                      fake_io_t *fake)
{
    click_ok(product);
    for (uint32_t now = start_ms; now < (start_ms + 10u); now++)
    {
        app_product_step(product, inputs, clock, BSP_STATUS_OK, now);
        if (fake->active)
        {
            return;
        }
    }
}

static int test_fault_during_measurement_capture_drains_runtime(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    fake.outcome_count = 1u;
    fake.outcomes[0] = good_outcome(measurement_complex(1000.0f, 0.0f),
                                    MEASUREMENT_INTERPRET_RESISTIVE);
    fake.abort_delay_steps = 2u;
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK,
                            "product init measurement fault drain");
    boot_to_ready(&product, &inputs);
    step_until_attempt_active(&product, &inputs, &clock, 3u, &fake);
    failures += expect_true(fake.active, "measurement capture active before fault");
    failures += expect_true(app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_METROLOGY,
                            "workspace owned by metrology before fault");

    inputs.safety_fault_mask = APP_SAFETY_FAULT_METROLOGY_RUNTIME;
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 20u);
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_FAULT,
                            "fault presentation happens immediately");
    failures += expect_true(fake.abort_called, "measurement abort requested");
    failures += expect_true(app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_METROLOGY,
                            "workspace not released before abort acknowledgement");

    for (uint32_t now = 21u; now < 30u; now++)
    {
        app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, now);
        if (app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_FREE)
        {
            break;
        }
    }
    failures += expect_true(app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_FREE,
                            "workspace released after drained measurement abort");
    failures += expect_true(fake.ack_count != 0u, "measurement abort acknowledged");
    return failures;
}

static int test_calibration_validity_loss_drains_measurement(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    fake.outcome_count = 1u;
    fake.outcomes[0] = good_outcome(measurement_complex(1000.0f, 0.0f),
                                    MEASUREMENT_INTERPRET_RESISTIVE);
    fake.abort_delay_steps = 1u;
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK,
                            "product init calibration loss drain");
    boot_to_ready(&product, &inputs);
    step_until_attempt_active(&product, &inputs, &clock, 3u, &fake);
    failures += expect_true(fake.active, "measurement active before calibration loss");

    inputs.calibration_active_valid = false;
    inputs.calibration_status = APP_CAL_SERVICE_NO_VALID_CALIBRATION;
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 20u);
    failures += expect_true(fake.abort_called, "calibration validity loss aborts measurement");
    failures += expect_true(app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_METROLOGY,
                            "workspace held while calibration-loss abort drains");
    for (uint32_t now = 21u; now < 30u; now++)
    {
        app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, now);
    }
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_CALIBRATION_REQUIRED,
                            "calibration loss reaches required gate after drain");
    failures += expect_true(app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_FREE,
                            "calibration-loss drain releases workspace");
    return failures;
}

#if !WTK_PRODUCT_FACTORY_PROVISIONED
static void start_product_wizard_capture(app_product_t *product,
                                         app_product_inputs_t *inputs,
                                         const bsp_clock_summary_t *clock,
                                         fake_io_t *fake)
{
    inputs->calibration_status = APP_CAL_SERVICE_NO_VALID_CALIBRATION;
    inputs->calibration_active_valid = false;
    inputs->calibration_active_sequence = 0u;
    boot_to_ready(product, inputs);
    click_ok(product);
    app_product_step(product, inputs, clock, BSP_STATUS_OK, 3u);
    click_ok(product);
    app_product_step(product, inputs, clock, BSP_STATUS_OK, 4u);
    click_ok(product);
    for (uint32_t now = 5u; now < 20u; now++)
    {
        app_product_step(product, inputs, clock, BSP_STATUS_OK, now);
        if (fake->active)
        {
            return;
        }
    }
}

static int test_fault_during_calibration_capture_drains_runtime(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    fake.outcome_count = 20u;
    fake.abort_delay_steps = 2u;
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK,
                            "product init calibration fault drain");
    start_product_wizard_capture(&product, &inputs, &clock, &fake);
    failures += expect_true(fake.active, "calibration capture active before fault");
    failures += expect_true(app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_METROLOGY,
                            "calibration owns metrology workspace before fault");

    inputs.safety_fault_mask = APP_SAFETY_FAULT_METROLOGY_RUNTIME;
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 30u);
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_FAULT,
                            "fault presentation during calibration is immediate");
    failures += expect_true(fake.abort_called, "calibration capture abort requested");
    failures += expect_true(app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_METROLOGY,
                            "calibration workspace remains held during abort");
    for (uint32_t now = 31u; now < 45u; now++)
    {
        app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, now);
        if (app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_FREE)
        {
            break;
        }
    }
    failures += expect_true(app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_FREE,
                            "calibration abort drain releases workspace");
    failures += expect_true(fake.ack_count != 0u, "calibration abort acknowledged");
    return failures;
}

static int test_user_cancel_during_calibration_capture_drains_runtime(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    fake.outcome_count = 20u;
    fake.abort_delay_steps = 1u;
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK,
                            "product init calibration cancel drain");
    start_product_wizard_capture(&product, &inputs, &clock, &fake);
    failures += expect_true(fake.active, "calibration active before user cancel");
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_LONG_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_RELEASE);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 30u);
    failures += expect_true(fake.abort_called, "user cancel requests calibration abort");
    for (uint32_t now = 31u; now < 45u; now++)
    {
        app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, now);
        if (app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_FREE)
        {
            break;
        }
    }
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_FREE,
                            "user cancel releases workspace after safe drain");
    failures += expect_true(view.state == UI_PRODUCT_STATE_CALIBRATION_REQUIRED,
                            "mandatory canceled wizard returns to calibration gate");
    return failures;
}

#endif

static bsp_status_t init_product(app_product_t *product, fake_io_t *fake)
{
    init_test_cal_service();
    fake->measurement_io = make_io(fake);
    fake->calibration_io = make_cal_io(fake);
    return app_product_init(product, &g_service, &g_settings, &fake->measurement_io, &fake->calibration_io);
}

static app_product_inputs_t inputs_ready(void)
{
    return (app_product_inputs_t){
        .calibration_status = APP_CAL_SERVICE_ACTIVE_VALID,
        .calibration_active_valid = true,
        .calibration_active_sequence = 1u,
        .safety_result = {
            .measure_allowed = true,
            .primary_blocker = HW_SAFETY_MEASURE_ALLOWED,
        },
        .safety_fault_mask = 0u,
        .display_fault = false,
    };
}

static void boot_to_ready(app_product_t *product, const app_product_inputs_t *inputs)
{
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL, .sysclk_hz = 72000000u, .hse_ready = true};
    app_product_step(product, inputs, &clock, BSP_STATUS_OK, 0u);
    app_product_step(product, inputs, &clock, BSP_STATUS_OK, 1u);
    app_product_step(product, inputs, &clock, BSP_STATUS_OK, 2u);
}

static void send_button(app_product_t *product, button_id_t button, button_event_type_t type)
{
    send_button_at(product, button, type, 1u);
}

static void send_button_at(app_product_t *product, button_id_t button, button_event_type_t type, uint32_t now_ms)
{
    const button_event_t event = {.button = button, .type = type, .timestamp_ms = now_ms};
    app_product_handle_button_event(product, &event);
}

static void click_ok(app_product_t *product)
{
    send_button(product, BUTTON_ID_OK, BUTTON_EVENT_PRESS);
    send_button(product, BUTTON_ID_OK, BUTTON_EVENT_RELEASE);
}

static int test_boot_calibration_gate(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    app_product_t product;
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK, "product init");
    app_product_inputs_t inputs = inputs_ready();
    boot_to_ready(&product, &inputs);
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_READY, "valid calibration reaches ready");

    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK, "product reinit");
    inputs.calibration_status = APP_CAL_SERVICE_NO_VALID_CALIBRATION;
    inputs.calibration_active_valid = false;
    inputs.calibration_active_sequence = 0u;
    boot_to_ready(&product, &inputs);
    click_ok(&product);
    app_product_step(&product,
                     &inputs,
                     &(const bsp_clock_summary_t){.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                                  .sysclk_hz = 72000000u,
                                                  .hse_ready = true},
                     BSP_STATUS_OK,
                     3u);
    app_product_make_view(&product, &view);
#if !WTK_PRODUCT_FACTORY_PROVISIONED
    failures += expect_true(view.state == UI_PRODUCT_STATE_CALIBRATION_WIZARD,
                            "missing calibration starts mandatory wizard on short OK");
    failures += expect_true(view.wizard.mandatory, "missing calibration wizard is mandatory");
#else
    failures += expect_true(view.state == UI_PRODUCT_STATE_CALIBRATION_REQUIRED,
                            "factory blank remains blocked after OK");
#endif
    failures += expect_u32(fake.start_count, 0u, "cal gate starts no measurement acquisition");

    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK, "product storage reinit");
    inputs.calibration_status = APP_CAL_SERVICE_STORAGE_UNAVAILABLE;
    inputs.calibration_active_valid = false;
    inputs.calibration_active_sequence = 0u;
    boot_to_ready(&product, &inputs);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_CALIBRATION_REQUIRED, "storage unavailable blocks ready");
    failures += expect_true(view.storage_unavailable, "storage unavailable surfaced");
    return failures;
}

static int test_ok_gestures_and_measurement_flow(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    fake.outcome_count = 2u;
    fake.outcomes[0] = good_outcome(measurement_complex(50.0f, -500.0f), MEASUREMENT_INTERPRET_CAPACITIVE);
    fake.outcomes[1] = good_outcome(measurement_complex(50.0f, -50.0f), MEASUREMENT_INTERPRET_CAPACITIVE);
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL, .sysclk_hz = 72000000u, .hse_ready = true};
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK, "product init measurement");
    boot_to_ready(&product, &inputs);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_PRESS);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 3u);
    failures += expect_u32(fake.start_count, 0u, "OK press alone starts nothing");
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_RELEASE);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 4u);
    failures += expect_u32(fake.start_count, 0u, "start is deferred until session steps");
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 5u);
    failures += expect_u32(fake.start_count, 0u, "auto begin precedes hardware start");
#if !WTK_PRODUCT_FACTORY_PROVISIONED
    click_ok(&product);
#endif
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 6u);
    failures += expect_u32(fake.start_count, 1u, "repeated OK while measuring ignored");
    bool saw_partial = false;
    for (uint32_t now = 7u; now < 24u; now++)
    {
        app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, now);
        ui_product_view_t interim;
        app_product_make_view(&product, &interim);
        if (interim.has_measurement_result && interim.measurement_result_partial)
        {
            saw_partial = true;
            failures += expect_true(interim.measurement_result.derived_valid,
                                    "valid partial keeps numeric measurement visible");
        }
    }
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(saw_partial, "partial result published while measuring");
    failures += expect_true(view.state == UI_PRODUCT_STATE_RESULT, "measurement reaches result");
    failures += expect_true(view.has_measurement_result && !view.measurement_result_partial, "final result stored");

    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK, "product init long");
    boot_to_ready(&product, &inputs);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_LONG_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_RELEASE);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 25u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_MENU, "long OK opens menu");
    failures += expect_u32(view.menu.selected_index, 0u, "menu opens at calibration entry");
    failures += expect_u32(fake.start_count, 2u, "long OK starts no extra measurement");
    return failures;
}

static int test_failed_refinement_hides_stale_primary(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    fake.outcome_count = 2u;
    fake.outcomes[0] = good_outcome(measurement_complex(50.0f, -500.0f),
                                    MEASUREMENT_INTERPRET_CAPACITIVE);
    fake.outcomes[1].fail_phase05 = true;
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK,
                            "product init failed refinement");
    boot_to_ready(&product, &inputs);
    click_ok(&product);
    for (uint32_t now = 3u; now < 30u; now++)
    {
        app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, now);
    }
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(fake.start_count == 2u, "supporting attempt was requested");
    failures += expect_true(view.has_measurement_result && !view.measurement_result_partial,
                            "failed refinement publishes terminal status");
    failures += expect_true(!view.measurement_result.derived_valid,
                            "failed session hides stale primary number");
    return failures;
}

static int test_menu_calibration_status_and_dirty_candidate(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK,
                            "product init calibration menu");
    boot_to_ready(&product, &inputs);

    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_LONG_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_RELEASE);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 3u);
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_MENU, "long OK enters menu");
    failures += expect_u32(view.menu.selected_index, 0u, "calibration is first menu item");

    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 4u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_CALIBRATION_STATUS,
                            "menu calibration opens status screen");
    failures += expect_true(view.calibration_active_valid, "status exposes active calibration validity");
    failures += expect_u32(view.calibration_sequence, 1u, "status exposes active calibration sequence");
    failures += expect_u32(view.menu.calibration_load_preset, 0u, "calibration load preset defaults nominal");

    send_button(&product, BUTTON_ID_DOWN, BUTTON_EVENT_PRESS);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 5u);
    app_product_make_view(&product, &view);
#if !WTK_PRODUCT_FACTORY_PROVISIONED
    failures += expect_u32(view.menu.calibration_load_preset, 1u, "calibration status cycles load preset forward");

    send_button(&product, BUTTON_ID_UP, BUTTON_EVENT_PRESS);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 6u);
    app_product_make_view(&product, &view);
    failures += expect_u32(view.menu.calibration_load_preset, 0u, "calibration status cycles load preset backward");

#else
    failures += expect_u32(view.menu.calibration_load_preset, 0u,
                            "factory status does not edit LOAD presets");
#endif
    failures += expect_true(app_calibration_service_candidate_begin(&g_service) == BSP_STATUS_OK,
                            "dirty candidate setup");
    inputs.calibration_status = APP_CAL_SERVICE_CANDIDATE_DIRTY;
    inputs.calibration_active_valid = true;
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 7u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_CALIBRATION_STATUS,
                            "dirty candidate prevents overwriting manual wizard start");
    failures += expect_u32(fake.start_count, 0u, "dirty candidate starts no capture");
    return failures;
}

static int test_safety_fault_and_pages(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    fake.outcome_count = 1u;
    fake.outcomes[0] = good_outcome(measurement_complex(1000.0f, 0.0f), MEASUREMENT_INTERPRET_RESISTIVE);
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL, .sysclk_hz = 72000000u, .hse_ready = true};
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK, "product init pages");
    boot_to_ready(&product, &inputs);
    inputs.safety_result.measure_allowed = false;
    inputs.safety_result.primary_blocker = HW_SAFETY_BLOCKED_CHARGER;
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 3u);
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_SAFETY_BLOCKED, "charger blocks measurement");
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 4u);
    failures += expect_u32(fake.start_count, 0u, "safety block starts no measurement");

    inputs = inputs_ready();
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 5u);
    click_ok(&product);
    for (uint32_t now = 6u; now < 16u; now++)
    {
        app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, now);
    }
    send_button(&product, BUTTON_ID_DOWN, BUTTON_EVENT_PRESS);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 16u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.page == UI_PRODUCT_PAGE_DETAILS, "DOWN changes result page");
    inputs.safety_fault_mask = APP_SAFETY_FAULT_METROLOGY_RUNTIME;
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 17u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_FAULT, "fault overrides result");
    return failures;
}

static int test_display_menu_brightness_preview_no_step_persist(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK,
                            "product init display menu");
    boot_to_ready(&product, &inputs);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_LONG_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_RELEASE);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 3u);
    send_button(&product, BUTTON_ID_DOWN, BUTTON_EVENT_PRESS);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 4u);
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 5u);
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_DISPLAY_MENU, "display submenu opens");
    failures += expect_u32(view.menu.selected_index, 0u, "brightness selected");
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 6u);
    failures += expect_true(view.state != UI_PRODUCT_STATE_BRIGHTNESS_EDIT, "stale view not reused");
    send_button(&product, BUTTON_ID_DOWN, BUTTON_EVENT_PRESS);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 7u);
    app_product_outputs_t outputs;
    app_product_make_outputs(&product, &outputs);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_BRIGHTNESS_EDIT, "brightness editor active");
    failures += expect_u32(view.menu.brightness_percent, 30u, "brightness increments in 5 percent step");
    failures += expect_u32(outputs.backlight_percent, 30u, "brightness preview updates output");
    failures += expect_true(app_settings_service_dirty(&g_settings), "preview marks settings dirty");
    failures += expect_true(!app_settings_service_busy(&g_settings), "preview does not persist immediately");
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_LONG_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_RELEASE);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 8u);
    app_product_make_outputs(&product, &outputs);
    failures += expect_u32(outputs.backlight_percent, 25u, "long OK restores entry brightness");
    return failures;
}

static int test_backlight_timeout_wake_consumes_ok_gesture(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK,
                            "product init backlight timeout");
    boot_to_ready(&product, &inputs);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 60005u);
    app_product_outputs_t outputs;
    app_product_make_outputs(&product, &outputs);
    failures += expect_u32(outputs.backlight_percent, 0u, "timeout blanks backlight");
    send_button_at(&product, BUTTON_ID_OK, BUTTON_EVENT_PRESS, 60006u);
    send_button_at(&product, BUTTON_ID_OK, BUTTON_EVENT_LONG_PRESS, 60600u);
    send_button_at(&product, BUTTON_ID_OK, BUTTON_EVENT_RELEASE, 60700u);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 60700u);
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    app_product_make_outputs(&product, &outputs);
    failures += expect_u32(outputs.backlight_percent, 25u, "wake restores configured brightness");
    failures += expect_true(view.state == UI_PRODUCT_STATE_READY, "wake gesture does not open menu");
    failures += expect_u32(fake.start_count, 0u, "wake gesture starts no measurement");
    return failures;
}

static int test_sound_menu_toggle_updates_output(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK,
                            "product init sound menu");
    boot_to_ready(&product, &inputs);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_LONG_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_RELEASE);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 3u);
    send_button(&product, BUTTON_ID_DOWN, BUTTON_EVENT_PRESS);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 4u);
    send_button(&product, BUTTON_ID_DOWN, BUTTON_EVENT_PRESS);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 5u);
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 6u);
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_SOUND_MENU, "sound submenu opens");
    failures += expect_true(view.menu.sound_enabled, "sound initially enabled");
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 7u);
    app_product_outputs_t outputs;
    app_product_make_outputs(&product, &outputs);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_SOUND_MENU, "sound toggle stays in submenu");
    failures += expect_true(!outputs.sound_enabled, "sound output disabled");
    failures += expect_true(app_settings_service_dirty(&g_settings), "sound toggle marks settings dirty");
    return failures;
}

static int test_diagnostics_and_maintenance_menu_pages(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK,
                            "product init diagnostics menu");
    boot_to_ready(&product, &inputs);

    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_LONG_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_RELEASE);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 3u);
    for (uint32_t i = 0u; i < 4u; i++)
    {
        send_button(&product, BUTTON_ID_DOWN, BUTTON_EVENT_PRESS);
        app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 4u + i);
    }
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_MENU, "diagnostics still in main menu");
    failures += expect_u32(view.menu.selected_index, 4u, "diagnostics menu index");
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 9u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_DIAGNOSTICS, "diagnostics page opens");
    failures += expect_u32(fake.start_count, 0u, "diagnostics starts no measurement");

    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 10u);
    send_button(&product, BUTTON_ID_DOWN, BUTTON_EVENT_PRESS);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 11u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_MENU, "diagnostics returns to menu");
    failures += expect_u32(view.menu.selected_index, 5u, "maintenance menu index");
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 12u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_MAINTENANCE, "maintenance page opens");
    failures += expect_u32(view.menu.selected_index, 0u, "pc link selected");
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 13u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_PC_LINK_STATUS, "pc link status opens");
    failures += expect_u32(fake.start_count, 0u, "maintenance starts no measurement");
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 14u);
    send_button(&product, BUTTON_ID_DOWN, BUTTON_EVENT_PRESS);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 15u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_MAINTENANCE, "pc link returns to maintenance");
    failures += expect_u32(view.menu.selected_index, 1u, "resources selected");
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 16u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_RESOURCE_STATUS, "resource status opens");
    failures += expect_u32(fake.start_count, 0u, "resource status starts no measurement");
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 17u);
    send_button(&product, BUTTON_ID_DOWN, BUTTON_EVENT_PRESS);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 18u);
    app_product_make_view(&product, &view);
    failures += expect_u32(view.menu.selected_index, 2u, "maintenance back selected");
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 19u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_MENU, "maintenance back returns to main menu");
    failures += expect_u32(view.menu.selected_index, 5u, "main menu keeps maintenance selected");
    return failures;
}

static int test_missing_resources_force_pc_link_upload_mode(void)
{
    int failures = 0;
    fake_io_t fake = {0};
    app_product_t product;
    app_product_inputs_t inputs = inputs_ready();
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    inputs.resource_status = RESOURCE_STATUS_MISSING;
    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK,
                            "product init missing resources");
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 0u);
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_PC_LINK_STATUS,
                            "missing resources enter upload mode from boot");
    failures += expect_true(view.resource_status == RESOURCE_STATUS_MISSING,
                            "missing resource status remains visible");
    failures += expect_true(view.safety_fault_mask == 0u,
                            "resource upload mode is not a safety fault");
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 1u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_PC_LINK_STATUS,
                            "upload mode is the only accessible product screen without resources");
    failures += expect_u32(fake.start_count, 0u, "upload mode starts no measurement");

    inputs.resource_status = RESOURCE_STATUS_OK;
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 2u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_STARTUP,
                            "valid resources release normal boot flow");
    return failures;
}

#if WTK_PRODUCT_FACTORY_PROVISIONED
static void ac_fixture(fake_io_t *fake, fake_outcome_t outcome)
{
    *fake = (fake_io_t){0};
    fake->outcome_count = MEASUREMENT_AUTO_MAX_ATTEMPTS;
    for (uint8_t i = 0u; i < fake->outcome_count; i++) fake->outcomes[i] = outcome;
}

static void ac_run(app_product_t *product, const app_product_inputs_t *inputs)
{
    const bsp_clock_summary_t clock = {.source=BSP_CLOCK_SOURCE_HSE_PLL,
        .sysclk_hz=72000000u,.hse_ready=true};
    click_ok(product);
    for (uint32_t now = 3u; now < 150u; now++)
        app_product_step(product, inputs, &clock, BSP_STATUS_OK, now);
}

static int ac_snapshot(const char *directory, const char *name, const app_product_t *product)
{
    if (directory == NULL) return 0;
    char path[512];
    if (snprintf(path, sizeof(path), "%s/%s.view", directory, name) >= (int)sizeof(path)) return 1;
    FILE *file = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&file, path, "wb") != 0) return 1;
#else
    file = fopen(path, "wb");
    if (file == NULL) return 1;
#endif
    /* Ephemeral same-build host snapshot, not a persistent or device wire format. */
    const bool okay = fwrite(&product->view, sizeof(product->view), 1u, file) == 1u;
    return fclose(file) == 0 && okay ? 0 : 1;
}

static int test_factory_ac_results(const char *directory)
{
    int failures = 0;
    const fake_outcome_t cases[] = {
        {.z={1000.0f,0.0f},.interpretation=MEASUREMENT_INTERPRET_RESISTIVE},
        {.z={5.0f,0.0f},.interpretation=MEASUREMENT_INTERPRET_CAPACITIVE,.electrical_model=1u,.reactive_value=1.0e-7f},
        {.z={2.0f,0.0f},.interpretation=MEASUREMENT_INTERPRET_INDUCTIVE,.electrical_model=2u,.reactive_value=0.01f},
        {.z={1000.0f,150.0f},.interpretation=MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN},
        {.z={0.01f,0.0f},.interpretation=MEASUREMENT_INTERPRET_CAPACITIVE,.electrical_model=1u,.reactive_value=1.0e-6f},
        {.z={1000.0f,1.0e-8f},.interpretation=MEASUREMENT_INTERPRET_RESISTIVE},
        {.z={-1000.0f,500.0f},.interpretation=MEASUREMENT_INTERPRET_INDUCTIVE},
        {.z={1.0e9f,0.0f},.interpretation=MEASUREMENT_INTERPRET_RESISTIVE},
        {.z={1.0e-8f,0.0f},.interpretation=MEASUREMENT_INTERPRET_RESISTIVE},
        {.z={1000.0f,0.0f},.interpretation=MEASUREMENT_INTERPRET_RESISTIVE,.path_faults=1u},
        {.z={1000.0f,0.0f},.interpretation=MEASUREMENT_INTERPRET_RESISTIVE,.path_faults=3u},
        {.z={NAN,0.0f},.interpretation=MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN},
        {.z={FLT_MAX,FLT_MAX},.interpretation=MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN},
    };
    const char *names[] = {"resistor","capacitor","inductor","mixed","low-esr","near-zero-x",
        "negative-loss","open-like","short-like","hg-clipped","both-invalid","nonfinite","overflow"};
    app_product_inputs_t inputs = inputs_ready();
    for (size_t i = 0u; i < sizeof(cases)/sizeof(cases[0]); i++)
    {
        fake_io_t fake;
        ac_fixture(&fake, cases[i]);
        app_product_t product;
        failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK, names[i]);
        boot_to_ready(&product, &inputs);
        failures += expect_u32(fake.start_count, 0u, "boot never acquires");
        ac_run(&product, &inputs);
        const ui_product_measurement_t *result = &product.view.measurement_result;
        failures += expect_true(product.view.state == UI_PRODUCT_STATE_RESULT &&
            product.view.has_measurement_result && !product.view.measurement_result_partial, names[i]);
        const bool valid = i <= 5u || i == 9u;
        failures += expect_true(result->derived_valid == valid, names[i]);
        if (valid)
        {
            failures += expect_true(result->max_error_status == MEASUREMENT_ERROR_NOT_CHARACTERIZED,
                "persisted unqualified OSL has no physical error bound");
            failures += expect_u32(result->calibration_sequence, 1u, "OSL sequence identity");
            failures += expect_u32(result->session_sequence, 1u, "measurement sequence identity");
            failures += expect_true(result->frequency == fake.requests[product.runtime.measurement.policy.last_result.primary_attempt_index].frequency,
                "frequency belongs to selected primary attempt");
            failures += expect_true(result->amplitude == fake.requests[product.runtime.measurement.policy.last_result.primary_attempt_index].amplitude,
                "amplitude belongs to selected primary attempt");
            failures += expect_true(fabsf(result->phase_rad - atan2f(result->reactance_ohms,result->resistance_ohms)) < 0.002f,
                "displayed phase follows complex impedance, including dominant reactance");
        }
        const bool reactive = i == 1u || i == 2u || i == 4u;
        failures += expect_true(result->q_valid == reactive && result->d_valid == reactive, names[i]);
        failures += expect_true(result->esr_valid == (i == 1u || i == 4u), "ESR is capacitive series AC only");
        failures += expect_true(result->capacitance_valid == (i == 1u || i == 4u), "capacitive model gate");
        failures += expect_true(result->inductance_valid == (i == 2u), "inductive model gate");
        if (reactive)
        {
            char q[24], d[24];
            failures += expect_true(ui_format_q(result->q, q, sizeof(q)) == UI_FORMAT_STATUS_OK &&
                ui_format_d(result->d, d, sizeof(d)) == UI_FORMAT_STATUS_OK, "real Q/D formatters");
        }
        failures += ac_snapshot(directory, names[i], &product);
        if (i <= 2u)
        {
            send_button(&product, BUTTON_ID_DOWN, BUTTON_EVENT_PRESS);
            app_product_step(&product, &inputs, NULL, BSP_STATUS_OK, 151u);
            failures += expect_true(product.view.page == UI_PRODUCT_PAGE_DETAILS, "RESULT -> DETAILS");
            char name[32];
            (void)snprintf(name, sizeof(name), "%s-details", names[i]);
            failures += ac_snapshot(directory, name, &product);
            click_ok(&product);
            app_product_step(&product, &inputs, NULL, BSP_STATUS_OK, 152u);
            failures += expect_true(product.view.state == UI_PRODUCT_STATE_READY &&
                !product.view.has_measurement_result, "DETAILS -> READY clears result without acquisition");
        }
    }
    for (uint8_t quantity = 1u; quantity <= 8u; quantity++)
    {
        fake_io_t fake;
        fake_outcome_t outcome = cases[quantity == 4u ? 2u : 1u];
        outcome.invalid_quantity = quantity;
        ac_fixture(&fake, outcome);
        app_product_t product;
        failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK, "invalid field init");
        boot_to_ready(&product, &inputs); ac_run(&product, &inputs);
        const ui_product_measurement_t *result = &product.view.measurement_result;
        failures += expect_true(quantity == 1u ? !result->q_valid : quantity == 2u ? !result->d_valid :
            quantity == 3u ? !result->capacitance_valid : quantity == 4u ? !result->inductance_valid :
            !result->derived_valid, "nonfinite/incompatible field is not published");
    }
    fake_io_t ideal;
    fake_outcome_t ideal_cap = cases[1]; ideal_cap.z.re=0.0f;
    ac_fixture(&ideal,ideal_cap);
    app_product_t ideal_product;
    failures += expect_true(init_product(&ideal_product,&ideal)==BSP_STATUS_OK,"lossless init");
    boot_to_ready(&ideal_product,&inputs); ac_run(&ideal_product,&inputs);
    failures += expect_true(ideal_product.view.measurement_result.capacitance_valid &&
        !ideal_product.view.measurement_result.q_valid && !ideal_product.view.measurement_result.d_valid,
        "zero loss denominator cannot claim finite Q/D");
    return failures;
}

static int test_factory_ac_lifecycle(const char *directory)
{
    int failures = 0;
    const bsp_clock_summary_t clock = {.source=BSP_CLOCK_SOURCE_HSE_PLL,.sysclk_hz=72000000u,.hse_ready=true};
    app_product_inputs_t inputs = inputs_ready();
    fake_io_t fake;
    ac_fixture(&fake, good_outcome(measurement_complex(1000.0f,0.0f),MEASUREMENT_INTERPRET_RESISTIVE));
    app_product_t product;
    failures += expect_true(init_product(&product,&fake)==BSP_STATUS_OK,"repeat init");
    boot_to_ready(&product,&inputs); ac_run(&product,&inputs);
    click_ok(&product); app_product_step(&product,&inputs,&clock,BSP_STATUS_OK,151u);
    failures += expect_true(product.view.state==UI_PRODUCT_STATE_MEASURING &&
        !product.view.has_measurement_result,"new click clears old number immediately");
    failures += ac_snapshot(directory,"measuring",&product);
    for(uint32_t now=152u;now<170u;now++) app_product_step(&product,&inputs,&clock,BSP_STATUS_OK,now);
    failures += expect_u32(product.view.measurement_result.session_sequence,2u,"consecutive result has fresh identity");
    failures += expect_u32(fake.start_count,2u,"two clicks produce two captures");
    for(uint8_t i=fake.start_count;i<fake.outcome_count;i++) fake.outcomes[i].fail_phase05=true;
    click_ok(&product);
    for(uint32_t now=175u;now<200u;now++) app_product_step(&product,&inputs,&clock,BSP_STATUS_OK,now);
    failures += expect_true(product.view.state==UI_PRODUCT_STATE_RESULT &&
        !product.view.measurement_result.derived_valid && !product.view.measurement_result.q_valid &&
        product.view.measurement_result.max_error_status==MEASUREMENT_ERROR_INVALID_RESULT,
        "failed third capture cannot republish successful second measurement");
    for (uint8_t reason=0u;reason<6u;reason++)
    {
        inputs=inputs_ready(); ac_fixture(&fake,good_outcome(measurement_complex(1000.0f,0.0f),MEASUREMENT_INTERPRET_RESISTIVE));
        fake.abort_delay_steps=2u;
        failures += expect_true(init_product(&product,&fake)==BSP_STATUS_OK,"interrupt init");
        boot_to_ready(&product,&inputs); step_until_attempt_active(&product,&inputs,&clock,3u,&fake);
        if(reason==0u) click_ok(&product);
        if(reason==1u) { inputs.safety_result.measure_allowed=false; inputs.safety_result.primary_blocker=HW_SAFETY_BLOCKED_CHARGER; }
        if(reason==2u) { inputs.safety_result.measure_allowed=false; inputs.safety_result.primary_blocker=HW_SAFETY_BLOCKED_RESIDUAL; }
        if(reason==3u) inputs.calibration_active_valid=false;
        if(reason==4u) inputs.calibration_active_sequence=2u;
        if(reason==5u) inputs.resource_status=RESOURCE_STATUS_CORRUPT;
        for(uint32_t now=10u;now<26u;now++) app_product_step(&product,&inputs,&clock,BSP_STATUS_OK,now);
        failures += expect_true(fake.abort_called && !fake.active && !product.view.has_measurement_result,
            "interruption drains acquisition and cannot finish as valid");
        failures += expect_true(app_io_workspace_owner(&g_workspace)==APP_IO_WORKSPACE_OWNER_FREE,
            "workspace released by hardware owner after teardown");
        failures += expect_true(product.view.state== (reason==0u||reason==4u?UI_PRODUCT_STATE_READY:
            reason==3u?UI_PRODUCT_STATE_CALIBRATION_REQUIRED:reason==5u?UI_PRODUCT_STATE_PC_LINK_STATUS:
            UI_PRODUCT_STATE_SAFETY_BLOCKED),"interruption terminal state");
        if(reason==0u) failures += ac_snapshot(directory,"canceled-ready",&product);
    }
    /* A resource/calibration change while browsing a menu must also revoke old results. */
    inputs=inputs_ready(); ac_fixture(&fake,good_outcome(measurement_complex(1000.0f,0.0f),MEASUREMENT_INTERPRET_RESISTIVE));
    failures += expect_true(init_product(&product,&fake)==BSP_STATUS_OK,"stale menu init");
    boot_to_ready(&product,&inputs); ac_run(&product,&inputs);
    send_button(&product,BUTTON_ID_OK,BUTTON_EVENT_PRESS); send_button(&product,BUTTON_ID_OK,BUTTON_EVENT_LONG_PRESS);
    send_button(&product,BUTTON_ID_OK,BUTTON_EVENT_RELEASE); app_product_step(&product,&inputs,&clock,BSP_STATUS_OK,151u);
    inputs.calibration_active_sequence=2u; app_product_step(&product,&inputs,&clock,BSP_STATUS_OK,152u);
    failures += expect_true(!product.view.has_measurement_result,"OSL replacement invalidates menu's old result");
    inputs=inputs_ready();
    fake_outcome_t capacitor = good_outcome(measurement_complex(5.0f,0.0f),MEASUREMENT_INTERPRET_CAPACITIVE);
    capacitor.electrical_model=1u; capacitor.reactive_value=1.0e-7f;
    ac_fixture(&fake,capacitor);
    failures += expect_true(init_product(&product,&fake)==BSP_STATUS_OK,"partial init");
    boot_to_ready(&product,&inputs); click_ok(&product);
    bool saw_partial=false;
    for(uint32_t now=3u;now<150u;now++)
    {
        app_product_step(&product,&inputs,&clock,BSP_STATUS_OK,now);
        if(product.view.measurement_result_partial)
        {
            saw_partial=true;
            failures += expect_true(product.view.state==UI_PRODUCT_STATE_MEASURING,
                "partial remains visibly measuring");
            failures += ac_snapshot(directory,"partial",&product);
            click_ok(&product);
            break;
        }
    }
    failures += expect_true(saw_partial,"multifrequency session emits partial");
    for(uint32_t now=151u;now<175u;now++) app_product_step(&product,&inputs,&clock,BSP_STATUS_OK,now);
    failures += expect_true(product.view.state==UI_PRODUCT_STATE_READY && !product.view.has_measurement_result,
        "cancel after partial cannot publish a final number");
    return failures;
}
#endif

int main(int argc, char **argv)
{
    if ((argc == 2) && (strcmp(argv[1], "--sizes") == 0))
    {
        (void)printf("ui_product_measurement_t=%lu\n",
                     (unsigned long)sizeof(ui_product_measurement_t));
        (void)printf("ui_product_menu_t=%lu\n", (unsigned long)sizeof(ui_product_menu_t));
        (void)printf("ui_product_wizard_t=%lu\n", (unsigned long)sizeof(ui_product_wizard_t));
        (void)printf("ui_product_view_t=%lu\n", (unsigned long)sizeof(ui_product_view_t));
        (void)printf("ui_product_t=%lu\n", (unsigned long)sizeof(ui_product_t));
        (void)printf("app_measurement_session_io_t=%lu\n",
                     (unsigned long)sizeof(app_measurement_session_io_t));
        (void)printf("app_measurement_session_t=%lu\n",
                     (unsigned long)app_measurement_session_size_bytes());
        (void)printf("app_cal_session_io_t=%lu\n", (unsigned long)sizeof(app_cal_session_io_t));
        (void)printf("app_calibration_session_t=%lu\n",
                     (unsigned long)app_calibration_session_context_size_bytes());
        (void)printf("app_calibration_wizard_t=%lu\n",
                     (unsigned long)app_calibration_wizard_context_size_bytes());
        (void)printf("app_product_runtime_union_t=%lu\n",
                     (unsigned long)sizeof(((app_product_t *)0)->runtime));
        (void)printf("app_product_t=%lu\n", (unsigned long)app_product_context_size_bytes());
        (void)printf("resource_catalog_t=%lu\n", (unsigned long)sizeof(resource_catalog_t));
        (void)printf("ui_text_catalog_t=%lu\n", (unsigned long)sizeof(ui_text_catalog_t));
        (void)printf("app_settings_service_t=%lu\n", (unsigned long)sizeof(app_settings_service_t));
        return 0;
    }
    int failures = 0;
#if WTK_PRODUCT_FACTORY_PROVISIONED
    const char *directory = argc==3 && strcmp(argv[1],"--ac-snapshots")==0 ? argv[2] : NULL;
    failures += test_factory_ac_results(directory);
    failures += test_factory_ac_lifecycle(directory);
#endif
    failures += test_boot_calibration_gate();
    failures += test_ok_gestures_and_measurement_flow();
    failures += test_failed_refinement_hides_stale_primary();
    failures += test_menu_calibration_status_and_dirty_candidate();
    failures += test_safety_fault_and_pages();
    failures += test_display_menu_brightness_preview_no_step_persist();
    failures += test_backlight_timeout_wake_consumes_ok_gesture();
    failures += test_sound_menu_toggle_updates_output();
    failures += test_diagnostics_and_maintenance_menu_pages();
    failures += test_missing_resources_force_pc_link_upload_mode();
    failures += test_fault_during_measurement_capture_drains_runtime();
    failures += test_calibration_validity_loss_drains_measurement();
#if !WTK_PRODUCT_FACTORY_PROVISIONED
    failures += test_fault_during_calibration_capture_drains_runtime();
    failures += test_user_cancel_during_calibration_capture_drains_runtime();
#endif
    failures += expect_true(sizeof(ui_product_measurement_t) < sizeof(measurement_session_result_t),
                            "compact UI result is smaller than session result");
    failures += expect_true(sizeof(ui_product_measurement_t) < 128u,
                            "compact UI result remains under target size");
    return failures == 0 ? 0 : 1;
}
