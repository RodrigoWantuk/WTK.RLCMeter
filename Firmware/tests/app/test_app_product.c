#include "app/app_product.h"
#include "app/app_io_workspace.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    measurement_complex_t z;
    measurement_interpretation_t interpretation;
    bool fail_phase05;
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
} fake_io_t;

static app_calibration_service_t g_service;
static app_io_workspace_t g_workspace;

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
    fake->active = false;
    fake->done = false;
    fake->dumpable = false;
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
    result->impedance = (measurement_impedance_result_t){
        .status = MEASUREMENT_STATUS_OK,
        .channel = MEASUREMENT_RETURN_1X,
        .vs_v = {0.100f, 0.0f},
        .vx_v = {0.030f, 0.0f},
        .z_ohms = outcome->z,
    };
    const measurement_dsp_config_t config = measurement_dsp_config_ideal(attempt->range_id);
    result->derived = measurement_derive_quantities(outcome->z,
                                                    frequency_hz(attempt->frequency),
                                                    &config,
                                                    MEASUREMENT_STATUS_OK);
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
    app_io_workspace_init(&g_workspace);
    app_calibration_service_attach_workspace(&g_service, &g_workspace);
}

static bsp_status_t init_product(app_product_t *product, fake_io_t *fake)
{
    init_test_cal_service();
    app_measurement_session_io_t io = make_io(fake);
    app_cal_session_io_t cal_io = make_cal_io(fake);
    return app_product_init(product, &g_service, &io, &cal_io);
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
        .battery_state = HW_BATTERY_OK,
        .safety_fault_mask = 0u,
        .display_ready = true,
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
    const button_event_t event = {.button = button, .type = type, .timestamp_ms = 1u};
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
    failures += expect_true(view.state == UI_PRODUCT_STATE_CALIBRATION_WIZARD,
                            "missing calibration starts mandatory wizard on short OK");
    failures += expect_true(view.wizard.mandatory, "missing calibration wizard is mandatory");
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
    click_ok(&product);
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
        }
    }
    ui_product_view_t view;
    app_product_make_view(&product, &view);
    failures += expect_true(saw_partial, "partial result published while measuring");
    failures += expect_true(view.state == UI_PRODUCT_STATE_RESULT, "measurement reaches result");
    failures += expect_true(view.has_measurement_result && !view.measurement_result_partial, "final result stored");
    failures += expect_u32(view.measurement_result.primary_attempt_index, 0u, "primary survives refinement");

    failures += expect_true(init_product(&product, &fake) == BSP_STATUS_OK, "product init long");
    boot_to_ready(&product, &inputs);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_LONG_PRESS);
    send_button(&product, BUTTON_ID_OK, BUTTON_EVENT_RELEASE);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 25u);
    app_product_make_view(&product, &view);
    failures += expect_true(view.state == UI_PRODUCT_STATE_MENU, "long OK opens menu");
    failures += expect_u32(view.menu.item_count, 2u, "menu has calibration and back");
    failures += expect_u32(fake.start_count, 2u, "long OK starts no extra measurement");
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

    failures += expect_true(app_calibration_service_candidate_begin(&g_service) == BSP_STATUS_OK,
                            "dirty candidate setup");
    inputs.calibration_status = APP_CAL_SERVICE_CANDIDATE_DIRTY;
    inputs.calibration_active_valid = true;
    click_ok(&product);
    app_product_step(&product, &inputs, &clock, BSP_STATUS_OK, 5u);
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

int main(int argc, char **argv)
{
    if ((argc == 2) && (strcmp(argv[1], "--sizes") == 0))
    {
        (void)printf("app_product_t=%lu\n", (unsigned long)app_product_context_size_bytes());
        (void)printf("ui_product_measurement_t=%lu\n",
                     (unsigned long)sizeof(ui_product_measurement_t));
        return 0;
    }
    int failures = 0;
    failures += test_boot_calibration_gate();
    failures += test_ok_gestures_and_measurement_flow();
    failures += test_menu_calibration_status_and_dirty_candidate();
    failures += test_safety_fault_and_pages();
    failures += expect_true(sizeof(ui_product_measurement_t) < sizeof(measurement_session_result_t),
                            "compact UI result is smaller than session result");
    failures += expect_true(sizeof(ui_product_measurement_t) < 128u,
                            "compact UI result remains under target size");
    return failures == 0 ? 0 : 1;
}
