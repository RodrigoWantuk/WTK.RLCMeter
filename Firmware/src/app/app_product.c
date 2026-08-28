#include "app/app_product.h"

#include <stddef.h>

enum
{
    APP_PRODUCT_RUNTIME_NONE = 0,
    APP_PRODUCT_RUNTIME_MEASUREMENT,
    APP_PRODUCT_RUNTIME_CALIBRATION,
    APP_PRODUCT_RUNTIME_TEARDOWN_NONE = 0,
    APP_PRODUCT_RUNTIME_TEARDOWN_MEASUREMENT,
    APP_PRODUCT_RUNTIME_TEARDOWN_CALIBRATION,
    APP_PRODUCT_MENU_CALIBRATION = 0,
    APP_PRODUCT_MENU_BACK = 1,
    APP_PRODUCT_MENU_COUNT = 2,
};

static ui_product_calibration_state_t ui_cal_state(app_cal_service_status_t status)
{
    switch (status)
    {
    case APP_CAL_SERVICE_ACTIVE_VALID:
        return UI_PRODUCT_CAL_ACTIVE_VALID;
    case APP_CAL_SERVICE_STORAGE_UNAVAILABLE:
    case APP_CAL_SERVICE_ERROR:
        return UI_PRODUCT_CAL_STORAGE_ERROR;
    case APP_CAL_SERVICE_UNINITIALIZED:
    case APP_CAL_SERVICE_READY:
    case APP_CAL_SERVICE_NO_VALID_CALIBRATION:
    case APP_CAL_SERVICE_WORKFLOW_ACTIVE:
    case APP_CAL_SERVICE_STORE_BUSY:
    case APP_CAL_SERVICE_CANDIDATE_DIRTY:
    default:
        return UI_PRODUCT_CAL_REQUIRED;
    }
}

static ui_product_battery_t ui_battery_state(hw_battery_state_t state)
{
    switch (state)
    {
    case HW_BATTERY_OK:
        return UI_PRODUCT_BATTERY_OK;
    case HW_BATTERY_LOW:
        return UI_PRODUCT_BATTERY_LOW;
    case HW_BATTERY_CRITICAL:
        return UI_PRODUCT_BATTERY_CRITICAL;
    case HW_BATTERY_UNKNOWN:
    default:
        return UI_PRODUCT_BATTERY_UNKNOWN;
    }
}

static ui_product_blocker_t ui_blocker(hw_safety_primary_blocker_t blocker)
{
    switch (blocker)
    {
    case HW_SAFETY_MEASURE_ALLOWED:
        return UI_PRODUCT_BLOCK_NONE;
    case HW_SAFETY_BLOCKED_FAULT:
        return UI_PRODUCT_BLOCK_FAULT;
    case HW_SAFETY_BLOCKED_CHARGER:
        return UI_PRODUCT_BLOCK_CHARGER;
    case HW_SAFETY_BLOCKED_SENSOR_INVALID:
        return UI_PRODUCT_BLOCK_SENSOR;
    case HW_SAFETY_BLOCKED_RESIDUAL:
        return UI_PRODUCT_BLOCK_RESIDUAL;
    case HW_SAFETY_BLOCKED_SUPPLY:
        return UI_PRODUCT_BLOCK_SUPPLY;
    case HW_SAFETY_BLOCKED_RANGE:
    default:
        return UI_PRODUCT_BLOCK_RANGE;
    }
}

static void mark_dirty(app_product_t *product)
{
    if (product != NULL)
    {
        product->view.generation++;
    }
}

static void set_state(app_product_t *product, ui_product_state_t state)
{
    if ((product != NULL) && (product->view.state != state))
    {
        product->view.state = state;
        mark_dirty(product);
    }
}

static void set_page(app_product_t *product, ui_product_page_t page)
{
    if ((product != NULL) && (product->view.page != page))
    {
        product->view.page = page;
        mark_dirty(product);
    }
}

static bool active_calibration_allows_ready(const app_product_inputs_t *inputs)
{
    return (inputs != NULL) && inputs->calibration_active_valid;
}

static bool state_accepts_measurement_request(ui_product_state_t state)
{
    return (state == UI_PRODUCT_STATE_READY) || (state == UI_PRODUCT_STATE_RESULT);
}

static app_measurement_session_t *measurement_runtime(app_product_t *product)
{
    return (product == NULL) ? NULL : &product->runtime.measurement;
}

static const app_measurement_session_t *measurement_runtime_const(const app_product_t *product)
{
    return (product == NULL) ? NULL : &product->runtime.measurement;
}

static app_calibration_wizard_t *wizard_runtime(app_product_t *product)
{
    return (product == NULL) ? NULL : &product->runtime.calibration;
}

static void update_wizard_view(app_product_t *product);

static bsp_status_t activate_measurement_runtime(app_product_t *product)
{
    if (product == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (product->runtime_kind == APP_PRODUCT_RUNTIME_MEASUREMENT)
    {
        return BSP_STATUS_OK;
    }
    if ((product->runtime_kind == APP_PRODUCT_RUNTIME_CALIBRATION) &&
        app_calibration_wizard_active(wizard_runtime(product)))
    {
        return BSP_STATUS_BUSY;
    }
    const bsp_status_t status =
        app_measurement_session_init(&product->runtime.measurement, &product->measurement_io);
    if (status == BSP_STATUS_OK)
    {
        product->runtime_kind = APP_PRODUCT_RUNTIME_MEASUREMENT;
    }
    return status;
}

static bsp_status_t activate_wizard_runtime(app_product_t *product)
{
    if (product == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (product->runtime_kind == APP_PRODUCT_RUNTIME_CALIBRATION)
    {
        return BSP_STATUS_OK;
    }
    if ((product->runtime_kind == APP_PRODUCT_RUNTIME_MEASUREMENT) &&
        app_measurement_session_active(measurement_runtime(product)))
    {
        return BSP_STATUS_BUSY;
    }
    const app_cal_fixture_profile_t fixture = {
        .load_z = app_calibration_fixture_profile_default_load,
        .user = NULL,
    };
    const bsp_status_t status = app_calibration_wizard_init(&product->runtime.calibration,
                                                           product->calibration_service,
                                                           &product->calibration_io,
                                                           &fixture);
    if (status == BSP_STATUS_OK)
    {
        product->runtime_kind = APP_PRODUCT_RUNTIME_CALIBRATION;
    }
    return status;
}

static void clear_requests(app_product_t *product)
{
    if (product != NULL)
    {
        product->request_click = false;
        product->request_menu = false;
        product->request_page_next = false;
        product->request_page_prev = false;
    }
}

static bool runtime_active(const app_product_t *product)
{
    if (product == NULL)
    {
        return false;
    }
    if (product->runtime_kind == APP_PRODUCT_RUNTIME_MEASUREMENT)
    {
        return app_measurement_session_active(measurement_runtime_const(product));
    }
    if (product->runtime_kind == APP_PRODUCT_RUNTIME_CALIBRATION)
    {
        return app_calibration_wizard_active(&product->runtime.calibration);
    }
    return false;
}

static void begin_runtime_teardown(app_product_t *product, ui_product_state_t target_state)
{
    if ((product == NULL) || (product->runtime_teardown_kind != APP_PRODUCT_RUNTIME_TEARDOWN_NONE))
    {
        return;
    }
    product->runtime_teardown_target_state = (uint8_t)target_state;
    if (product->runtime_kind == APP_PRODUCT_RUNTIME_MEASUREMENT)
    {
        product->runtime_teardown_kind = APP_PRODUCT_RUNTIME_TEARDOWN_MEASUREMENT;
        (void)app_measurement_session_cancel(measurement_runtime(product));
    }
    else if (product->runtime_kind == APP_PRODUCT_RUNTIME_CALIBRATION)
    {
        product->runtime_teardown_kind = APP_PRODUCT_RUNTIME_TEARDOWN_CALIBRATION;
        (void)app_calibration_wizard_cancel(wizard_runtime(product));
    }
}

static bool drain_runtime_teardown(app_product_t *product,
                                   const hw_safety_result_t *safety,
                                   const bsp_clock_summary_t *clock_summary,
                                   bsp_status_t clock_status,
                                   int32_t temperature_mC,
                                   bool temperature_valid,
                                   uint32_t now_ms)
{
    if ((product == NULL) ||
        (product->runtime_teardown_kind == APP_PRODUCT_RUNTIME_TEARDOWN_NONE))
    {
        return false;
    }

    if (product->runtime_teardown_kind == APP_PRODUCT_RUNTIME_TEARDOWN_MEASUREMENT)
    {
        if (app_measurement_session_active(measurement_runtime(product)))
        {
            (void)app_measurement_session_step(measurement_runtime(product), now_ms);
            return true;
        }
    }
    else if (product->runtime_teardown_kind == APP_PRODUCT_RUNTIME_TEARDOWN_CALIBRATION)
    {
        if (app_calibration_wizard_active(wizard_runtime(product)))
        {
            app_calibration_wizard_step(wizard_runtime(product),
                                        safety,
                                        clock_summary,
                                        clock_status,
                                        temperature_mC,
                                        temperature_valid,
                                        now_ms);
            update_wizard_view(product);
            return true;
        }
    }

    product->runtime_teardown_kind = APP_PRODUCT_RUNTIME_TEARDOWN_NONE;
    const ui_product_state_t target = (ui_product_state_t)product->runtime_teardown_target_state;
    (void)activate_measurement_runtime(product);
    set_state(product, target);
    return false;
}

static ui_product_measurement_t ui_measurement_from_result(const measurement_session_result_t *result)
{
    ui_product_measurement_t out = {0};
    if (result == NULL)
    {
        return out;
    }
    out.status = result->status;
    out.interpretation = result->classification.interpretation;
    out.confidence = result->confidence.publication_confidence;
    out.quality = result->confidence.measurement_quality;
    out.qualification = result->confidence.qualification;
    out.frequency = result->primary_attempt.config.frequency;
    out.amplitude = result->primary_attempt.config.amplitude;
    out.resistance_ohms = result->primary_attempt.derived.resistance_ohms;
    out.reactance_ohms = result->primary_attempt.derived.reactance_ohms;
    out.magnitude_ohms = result->primary_attempt.derived.magnitude_ohms;
    out.phase_rad = result->primary_attempt.derived.phase_rad;
    out.capacitance_f = result->primary_attempt.derived.capacitance_f;
    out.inductance_h = result->primary_attempt.derived.inductance_h;
    out.attempt_count = result->attempt_count;
    out.primary_attempt_index = result->primary_attempt_index;
    out.derived_valid = result->primary_attempt.derived.valid;
    out.capacitance_valid = result->primary_attempt.derived.capacitance_valid;
    out.inductance_valid = result->primary_attempt.derived.inductance_valid;
    return out;
}

static ui_product_wizard_state_t ui_wizard_state(app_cal_wizard_state_t state)
{
    switch (state)
    {
    case APP_CAL_WIZARD_INTRO:
        return UI_PRODUCT_WIZARD_INTRO;
    case APP_CAL_WIZARD_WAIT_OPEN_FIXTURE:
        return UI_PRODUCT_WIZARD_WAIT_OPEN;
    case APP_CAL_WIZARD_CAPTURE_OPEN:
        return UI_PRODUCT_WIZARD_CAPTURE_OPEN;
    case APP_CAL_WIZARD_WAIT_SHORT_FIXTURE:
        return UI_PRODUCT_WIZARD_WAIT_SHORT;
    case APP_CAL_WIZARD_CAPTURE_SHORT:
        return UI_PRODUCT_WIZARD_CAPTURE_SHORT;
    case APP_CAL_WIZARD_WAIT_LOAD_FIXTURE:
        return UI_PRODUCT_WIZARD_WAIT_LOAD;
    case APP_CAL_WIZARD_CAPTURE_LOAD:
        return UI_PRODUCT_WIZARD_CAPTURE_LOAD;
    case APP_CAL_WIZARD_RANGE_COMPLETE:
        return UI_PRODUCT_WIZARD_RANGE_COMPLETE;
    case APP_CAL_WIZARD_CONFIRM_SAVE:
        return UI_PRODUCT_WIZARD_CONFIRM_SAVE;
    case APP_CAL_WIZARD_COMMITTING:
        return UI_PRODUCT_WIZARD_COMMITTING;
    case APP_CAL_WIZARD_COMPLETE:
        return UI_PRODUCT_WIZARD_COMPLETE;
    case APP_CAL_WIZARD_FAILED:
        return UI_PRODUCT_WIZARD_FAILED;
    case APP_CAL_WIZARD_SAFETY_BLOCKED:
        return UI_PRODUCT_WIZARD_SAFETY_BLOCKED;
    case APP_CAL_WIZARD_CANCELING:
        return UI_PRODUCT_WIZARD_CANCELING;
    case APP_CAL_WIZARD_CANCELED:
        return UI_PRODUCT_WIZARD_CANCELED;
    case APP_CAL_WIZARD_IDLE:
    default:
        return UI_PRODUCT_WIZARD_IDLE;
    }
}

static ui_product_wizard_standard_t ui_wizard_standard(app_cal_standard_type_t standard)
{
    switch (standard)
    {
    case APP_CAL_STANDARD_OPEN:
        return UI_PRODUCT_WIZARD_STANDARD_OPEN;
    case APP_CAL_STANDARD_SHORT:
        return UI_PRODUCT_WIZARD_STANDARD_SHORT;
    case APP_CAL_STANDARD_LOAD:
    default:
        return UI_PRODUCT_WIZARD_STANDARD_LOAD;
    }
}

static void update_wizard_view(app_product_t *product)
{
    if ((product == NULL) || (product->runtime_kind != APP_PRODUCT_RUNTIME_CALIBRATION))
    {
        return;
    }
    app_cal_wizard_snapshot_t snapshot;
    app_calibration_wizard_snapshot(wizard_runtime(product), &snapshot);
    product->view.wizard = (ui_product_wizard_t){
        .state = (uint8_t)ui_wizard_state(snapshot.state),
        .mode = (uint8_t)snapshot.mode,
        .standard = (uint8_t)ui_wizard_standard(snapshot.standard),
        .error = (uint8_t)snapshot.error,
        .workflow_result = (uint8_t)snapshot.workflow_result,
        .solver_status = (uint8_t)snapshot.solver_status,
        .range_id = snapshot.range_id,
        .frequency = snapshot.frequency,
        .amplitude = snapshot.amplitude,
        .range_index = snapshot.range_index,
        .range_count = snapshot.range_count,
        .condition_index = snapshot.condition_index,
        .condition_count = snapshot.condition_count,
        .solved_count = snapshot.solved_count,
        .total_conditions = snapshot.total_conditions,
        .accepted = snapshot.accepted,
        .attempts = snapshot.attempts,
        .mandatory = snapshot.mandatory,
    };
    mark_dirty(product);
}

static void update_measurement_result(app_product_t *product, app_measurement_event_t event)
{
    if (product == NULL)
    {
        return;
    }
    if (event == APP_MEASUREMENT_EVENT_PARTIAL_RESULT)
    {
        const measurement_session_result_t *partial =
            app_measurement_session_partial(measurement_runtime(product));
        if (partial != NULL)
        {
            product->view.measurement_result = ui_measurement_from_result(partial);
            product->view.has_measurement_result = true;
            product->view.measurement_result_partial = true;
            mark_dirty(product);
        }
    }
    else if (event == APP_MEASUREMENT_EVENT_FINAL_RESULT)
    {
        const measurement_session_result_t *final =
            app_measurement_session_final(measurement_runtime(product));
        if (final != NULL)
        {
            product->view.measurement_result = ui_measurement_from_result(final);
            product->view.has_measurement_result = true;
            product->view.measurement_result_partial = false;
            product->next_hint = measurement_auto_make_hint(final);
            mark_dirty(product);
        }
    }
}

bsp_status_t app_product_init(app_product_t *product,
                              app_calibration_service_t *calibration_service,
                              const app_measurement_session_io_t *measurement_io,
                              const app_cal_session_io_t *calibration_io)
{
    if ((product == NULL) || (calibration_service == NULL) ||
        (measurement_io == NULL) || (calibration_io == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *product = (app_product_t){0};
    product->measurement_io = *measurement_io;
    product->calibration_io = *calibration_io;
    product->calibration_service = calibration_service;
    const bsp_status_t status = activate_measurement_runtime(product);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    product->view = (ui_product_view_t){
        .state = UI_PRODUCT_STATE_STARTUP,
        .page = UI_PRODUCT_PAGE_PRIMARY,
        .calibration_status = UI_PRODUCT_CAL_UNKNOWN,
        .safety_blocker = UI_PRODUCT_BLOCK_SENSOR,
        .battery_state = UI_PRODUCT_BATTERY_UNKNOWN,
        .menu = {
            .selected_index = APP_PRODUCT_MENU_CALIBRATION,
            .item_count = APP_PRODUCT_MENU_COUNT,
        },
        .generation = 1u,
    };
    product->initialized = true;
    return BSP_STATUS_OK;
}

void app_product_handle_button_event(app_product_t *product, const button_event_t *event)
{
    if ((product == NULL) || (event == NULL))
    {
        return;
    }
    if (event->button == BUTTON_ID_OK)
    {
        if (event->type == BUTTON_EVENT_PRESS)
        {
            product->ok_armed = true;
            product->ok_long_seen = false;
        }
        else if ((event->type == BUTTON_EVENT_LONG_PRESS) && product->ok_armed)
        {
            product->ok_long_seen = true;
            product->request_menu = true;
        }
        else if (event->type == BUTTON_EVENT_RELEASE)
        {
            if (product->ok_armed && !product->ok_long_seen)
            {
                product->request_click = true;
            }
            product->ok_armed = false;
            product->ok_long_seen = false;
        }
        return;
    }
    if ((event->type != BUTTON_EVENT_PRESS) && (event->type != BUTTON_EVENT_REPEAT))
    {
        return;
    }
    if (event->button == BUTTON_ID_UP)
    {
        product->request_page_prev = true;
    }
    else if (event->button == BUTTON_ID_DOWN)
    {
        product->request_page_next = true;
    }
}

void app_product_step(app_product_t *product,
                      const app_product_inputs_t *inputs,
                      const bsp_clock_summary_t *clock_summary,
                      bsp_status_t clock_status,
                      uint32_t now_ms)
{
    if ((product == NULL) || !product->initialized || (inputs == NULL))
    {
        return;
    }

    const ui_product_calibration_state_t next_cal = ui_cal_state(inputs->calibration_status);
    const ui_product_blocker_t next_blocker = ui_blocker(inputs->safety_result.primary_blocker);
    const ui_product_battery_t next_battery = ui_battery_state(inputs->battery_state);
    const bool next_storage_unavailable = inputs->calibration_status == APP_CAL_SERVICE_STORAGE_UNAVAILABLE;
    const uint8_t next_measurement_state =
        (product->runtime_kind == APP_PRODUCT_RUNTIME_MEASUREMENT) ?
            (uint8_t)app_measurement_session_state(measurement_runtime_const(product)) :
            0u;
    if ((product->view.calibration_status != next_cal) ||
        (product->view.safety_blocker != next_blocker) ||
        (product->view.battery_state != next_battery) ||
        (product->view.safety_fault_mask != inputs->safety_fault_mask) ||
        (product->view.display_ready != inputs->display_ready) ||
        (product->view.display_fault != inputs->display_fault) ||
        (product->view.storage_unavailable != next_storage_unavailable) ||
        (product->view.measurement_state != next_measurement_state) ||
        (product->view.calibration_active_valid != inputs->calibration_active_valid) ||
        (product->view.calibration_sequence != inputs->calibration_active_sequence))
    {
        product->view.calibration_status = next_cal;
        product->view.safety_blocker = next_blocker;
        product->view.battery_state = next_battery;
        product->view.safety_fault_mask = inputs->safety_fault_mask;
        product->view.display_ready = inputs->display_ready;
        product->view.display_fault = inputs->display_fault;
        product->view.storage_unavailable = next_storage_unavailable;
        product->view.measurement_state = next_measurement_state;
        product->view.calibration_active_valid = inputs->calibration_active_valid;
        product->view.calibration_sequence = inputs->calibration_active_sequence;
        mark_dirty(product);
    }

    if ((inputs->safety_fault_mask != 0u) || inputs->display_fault)
    {
        set_state(product, UI_PRODUCT_STATE_FAULT);
        if (runtime_active(product))
        {
            begin_runtime_teardown(product, UI_PRODUCT_STATE_FAULT);
            (void)drain_runtime_teardown(product,
                                         &inputs->safety_result,
                                         clock_summary,
                                         clock_status,
                                         inputs->temperature_mC,
                                         inputs->temperature_valid,
                                         now_ms);
        }
        clear_requests(product);
        return;
    }

    if (drain_runtime_teardown(product,
                               &inputs->safety_result,
                               clock_summary,
                               clock_status,
                               inputs->temperature_mC,
                               inputs->temperature_valid,
                               now_ms))
    {
        clear_requests(product);
        return;
    }

    if (product->view.state == UI_PRODUCT_STATE_STARTUP)
    {
        set_state(product, UI_PRODUCT_STATE_SELF_TEST);
        return;
    }
    if (product->view.state == UI_PRODUCT_STATE_SELF_TEST)
    {
        set_state(product, UI_PRODUCT_STATE_CALIBRATION_CHECK);
        return;
    }

    if (product->view.state == UI_PRODUCT_STATE_MENU)
    {
        if (product->request_page_next)
        {
            product->menu_index = (uint8_t)((product->menu_index + 1u) % APP_PRODUCT_MENU_COUNT);
            product->view.menu.selected_index = product->menu_index;
            mark_dirty(product);
        }
        if (product->request_page_prev)
        {
            product->menu_index = (product->menu_index == 0u) ?
                                      (uint8_t)(APP_PRODUCT_MENU_COUNT - 1u) :
                                      (uint8_t)(product->menu_index - 1u);
            product->view.menu.selected_index = product->menu_index;
            mark_dirty(product);
        }
        if (product->request_menu)
        {
            set_state(product,
                      (product->view.has_measurement_result && !product->view.measurement_result_partial) ?
                          UI_PRODUCT_STATE_RESULT :
                          UI_PRODUCT_STATE_READY);
        }
        else if (product->request_click)
        {
            if (product->menu_index == APP_PRODUCT_MENU_CALIBRATION)
            {
                set_state(product, UI_PRODUCT_STATE_CALIBRATION_STATUS);
            }
            else
            {
                set_state(product,
                          (product->view.has_measurement_result && !product->view.measurement_result_partial) ?
                              UI_PRODUCT_STATE_RESULT :
                              UI_PRODUCT_STATE_READY);
            }
        }
        product->request_click = false;
        product->request_menu = false;
        product->request_page_next = false;
        product->request_page_prev = false;
        return;
    }

    if (product->view.state == UI_PRODUCT_STATE_CALIBRATION_STATUS)
    {
        if (product->request_menu)
        {
            set_state(product, UI_PRODUCT_STATE_MENU);
        }
        else if (product->request_click && inputs->calibration_active_valid &&
                 (inputs->calibration_status != APP_CAL_SERVICE_STORAGE_UNAVAILABLE))
        {
            if (activate_wizard_runtime(product) == BSP_STATUS_OK)
            {
                product->calibration_sequence++;
                if (app_calibration_wizard_start(wizard_runtime(product),
                                                 APP_CAL_WIZARD_MODE_MANUAL,
                                                 product->calibration_sequence,
                                                 inputs->temperature_mC,
                                                 inputs->temperature_valid) == BSP_STATUS_OK)
                {
                    update_wizard_view(product);
                    set_state(product, UI_PRODUCT_STATE_CALIBRATION_WIZARD);
                }
            }
        }
        product->request_click = false;
        product->request_menu = false;
        product->request_page_next = false;
        product->request_page_prev = false;
        return;
    }

    if (product->view.state == UI_PRODUCT_STATE_CALIBRATION_WIZARD)
    {
        if (product->request_menu)
        {
            (void)app_calibration_wizard_cancel(wizard_runtime(product));
        }
        if (product->request_click)
        {
            app_calibration_wizard_confirm(wizard_runtime(product));
        }
        app_calibration_wizard_step(wizard_runtime(product),
                                    &inputs->safety_result,
                                    clock_summary,
                                    clock_status,
                                    inputs->temperature_mC,
                                    inputs->temperature_valid,
                                    now_ms);
        update_wizard_view(product);
        if (app_calibration_wizard_terminal(wizard_runtime(product)))
        {
            const app_cal_wizard_state_t wizard_state = wizard_runtime(product)->state;
            if (wizard_state == APP_CAL_WIZARD_COMPLETE)
            {
                (void)activate_measurement_runtime(product);
                set_state(product,
                          active_calibration_allows_ready(inputs) ||
                                  app_calibration_service_active_valid(product->calibration_service) ?
                              UI_PRODUCT_STATE_READY :
                              UI_PRODUCT_STATE_CALIBRATION_REQUIRED);
            }
            else if (wizard_state == APP_CAL_WIZARD_CANCELED)
            {
                (void)activate_measurement_runtime(product);
                set_state(product,
                          app_calibration_service_active_valid(product->calibration_service) ?
                              UI_PRODUCT_STATE_CALIBRATION_STATUS :
                              UI_PRODUCT_STATE_CALIBRATION_REQUIRED);
            }
        }
        product->request_click = false;
        product->request_menu = false;
        product->request_page_next = false;
        product->request_page_prev = false;
        return;
    }

    if (!active_calibration_allows_ready(inputs))
    {
        if ((product->runtime_kind == APP_PRODUCT_RUNTIME_MEASUREMENT) &&
            app_measurement_session_active(measurement_runtime(product)))
        {
            begin_runtime_teardown(product, UI_PRODUCT_STATE_CALIBRATION_REQUIRED);
            (void)drain_runtime_teardown(product,
                                         &inputs->safety_result,
                                         clock_summary,
                                         clock_status,
                                         inputs->temperature_mC,
                                         inputs->temperature_valid,
                                         now_ms);
            clear_requests(product);
            return;
        }
        if ((inputs->calibration_status != APP_CAL_SERVICE_STORAGE_UNAVAILABLE) &&
            product->request_click)
        {
            if (activate_wizard_runtime(product) == BSP_STATUS_OK)
            {
                product->calibration_sequence++;
                if (app_calibration_wizard_start(wizard_runtime(product),
                                                 APP_CAL_WIZARD_MODE_MANDATORY,
                                                 product->calibration_sequence,
                                                 inputs->temperature_mC,
                                                 inputs->temperature_valid) == BSP_STATUS_OK)
                {
                    update_wizard_view(product);
                    set_state(product, UI_PRODUCT_STATE_CALIBRATION_WIZARD);
                }
            }
        }
        else
        {
            set_state(product, UI_PRODUCT_STATE_CALIBRATION_REQUIRED);
        }
        product->request_click = false;
        product->request_menu = false;
        product->request_page_next = false;
        product->request_page_prev = false;
        return;
    }

    if ((product->runtime_kind == APP_PRODUCT_RUNTIME_MEASUREMENT) &&
        app_measurement_session_active(measurement_runtime(product)))
    {
        const app_measurement_event_t event = app_measurement_session_step(measurement_runtime(product), now_ms);
        update_measurement_result(product, event);
        set_state(product, app_measurement_session_active(measurement_runtime(product)) ?
                               UI_PRODUCT_STATE_MEASURING :
                               UI_PRODUCT_STATE_RESULT);
        product->request_click = false;
        product->request_page_next = false;
        product->request_page_prev = false;
        return;
    }

    if (!inputs->safety_result.measure_allowed)
    {
        set_state(product, UI_PRODUCT_STATE_SAFETY_BLOCKED);
        product->request_click = false;
        product->request_menu = false;
        product->request_page_next = false;
        product->request_page_prev = false;
        return;
    }

    if (product->request_menu)
    {
        product->menu_index = APP_PRODUCT_MENU_CALIBRATION;
        product->view.menu.selected_index = product->menu_index;
        product->view.menu.item_count = APP_PRODUCT_MENU_COUNT;
        mark_dirty(product);
        set_state(product, UI_PRODUCT_STATE_MENU);
        product->request_menu = false;
        product->request_click = false;
        product->request_page_next = false;
        product->request_page_prev = false;
        return;
    }

    if (product->view.has_measurement_result && !product->view.measurement_result_partial &&
        (product->view.state == UI_PRODUCT_STATE_RESULT))
    {
        if (product->request_page_next)
        {
            set_page(product,
                     (product->view.page == UI_PRODUCT_PAGE_PRIMARY) ? UI_PRODUCT_PAGE_DETAILS :
                                                                       UI_PRODUCT_PAGE_PRIMARY);
        }
        if (product->request_page_prev)
        {
            set_page(product,
                     (product->view.page == UI_PRODUCT_PAGE_PRIMARY) ? UI_PRODUCT_PAGE_DETAILS :
                                                                       UI_PRODUCT_PAGE_PRIMARY);
        }
    }
    product->request_page_next = false;
    product->request_page_prev = false;

    if (product->request_click && state_accepts_measurement_request(product->view.state))
    {
        if (activate_measurement_runtime(product) != BSP_STATUS_OK)
        {
            product->request_click = false;
            set_state(product, UI_PRODUCT_STATE_FAULT);
            return;
        }
        product->session_sequence++;
        const bsp_status_t start_status =
            app_measurement_session_start(measurement_runtime(product),
                                          MEASUREMENT_AUTO_MODE_CLICK,
                                          product->session_sequence,
                                          MEASUREMENT_QUALIFICATION_UNQUALIFIED,
                                          product->next_hint.valid ? &product->next_hint : NULL,
                                          clock_summary,
                                          clock_status,
                                          now_ms);
        if ((start_status == BSP_STATUS_BUSY) || (start_status == BSP_STATUS_OK))
        {
            product->view.measurement_result_partial = false;
            set_state(product, UI_PRODUCT_STATE_MEASURING);
        }
        product->request_click = false;
    }
    else
    {
        product->request_click = false;
        set_state(product,
                  (product->view.has_measurement_result && !product->view.measurement_result_partial) ?
                      UI_PRODUCT_STATE_RESULT :
                      UI_PRODUCT_STATE_READY);
    }
    product->view.session_sequence = product->session_sequence;
}

void app_product_cancel(app_product_t *product)
{
    if (product != NULL)
    {
        if (product->runtime_kind == APP_PRODUCT_RUNTIME_MEASUREMENT)
        {
            (void)app_measurement_session_cancel(measurement_runtime(product));
        }
        else if (product->runtime_kind == APP_PRODUCT_RUNTIME_CALIBRATION)
        {
            (void)app_calibration_wizard_cancel(wizard_runtime(product));
        }
    }
}

void app_product_make_view(const app_product_t *product, ui_product_view_t *view)
{
    if ((product != NULL) && (view != NULL))
    {
        *view = product->view;
    }
}

uint32_t app_product_context_size_bytes(void)
{
    return (uint32_t)sizeof(app_product_t);
}
