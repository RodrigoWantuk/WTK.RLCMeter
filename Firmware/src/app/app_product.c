#include "app/app_product.h"

#include <stddef.h>

#if defined(__GNUC__)
#define WTK_NOINLINE __attribute__((noinline))
#else
#define WTK_NOINLINE
#endif

enum
{
    APP_PRODUCT_RUNTIME_NONE = 0,
    APP_PRODUCT_RUNTIME_MEASUREMENT,
    APP_PRODUCT_RUNTIME_CALIBRATION,
    APP_PRODUCT_RUNTIME_TEARDOWN_NONE = 0,
    APP_PRODUCT_RUNTIME_TEARDOWN_MEASUREMENT,
    APP_PRODUCT_RUNTIME_TEARDOWN_CALIBRATION,
    APP_PRODUCT_MAIN_CALIBRATION = 0,
    APP_PRODUCT_MAIN_DISPLAY,
    APP_PRODUCT_MAIN_SOUND,
    APP_PRODUCT_MAIN_LANGUAGE,
    APP_PRODUCT_MAIN_ABOUT,
    APP_PRODUCT_MAIN_BACK,
    APP_PRODUCT_MAIN_COUNT,
    APP_PRODUCT_DISPLAY_BRIGHTNESS = 0,
    APP_PRODUCT_DISPLAY_TIMEOUT,
    APP_PRODUCT_DISPLAY_BACK,
    APP_PRODUCT_DISPLAY_COUNT,
    APP_PRODUCT_SOUND_TOGGLE = 0,
    APP_PRODUCT_SOUND_BACK,
    APP_PRODUCT_SOUND_COUNT,
    APP_PRODUCT_LANGUAGE_EN = 0,
    APP_PRODUCT_LANGUAGE_PT_BR,
    APP_PRODUCT_LANGUAGE_BACK,
    APP_PRODUCT_LANGUAGE_COUNT,
};

static void mark_dirty(app_product_t *product);
static void set_state(app_product_t *product, ui_product_state_t state);

static ui_product_calibration_state_t ui_cal_state(app_cal_service_status_t status,
                                                  bool calibration_active_valid)
{
    if (calibration_active_valid)
    {
        return UI_PRODUCT_CAL_ACTIVE_VALID;
    }
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

static app_settings_t current_settings(const app_product_t *product)
{
    const app_settings_t *settings =
        (product == NULL) ? NULL : app_settings_service_current(product->settings_service);
    return (settings == NULL) ? app_settings_defaults() : *settings;
}

static void sync_settings_view(app_product_t *product)
{
    if (product == NULL)
    {
        return;
    }
    const app_settings_t settings = current_settings(product);
    const uint16_t timeout_seconds = (uint16_t)settings.backlight_timeout;
    if ((product->view.menu.brightness_percent != settings.brightness_percent) ||
        (product->view.menu.timeout_seconds != timeout_seconds) ||
        (product->view.menu.sound_enabled != settings.sound_enabled) ||
        (product->view.menu.language_id != settings.language_id) ||
        (product->view.menu.dirty != app_settings_service_dirty(product->settings_service)) ||
        (product->view.menu.save_failed != app_settings_service_save_failed(product->settings_service)))
    {
        product->view.menu.brightness_percent = settings.brightness_percent;
        product->view.menu.timeout_seconds = timeout_seconds;
        product->view.menu.sound_enabled = settings.sound_enabled;
        product->view.menu.language_id = settings.language_id;
        product->view.menu.dirty = app_settings_service_dirty(product->settings_service);
        product->view.menu.save_failed = app_settings_service_save_failed(product->settings_service);
        mark_dirty(product);
    }
}

static void set_menu(app_product_t *product,
                     ui_product_state_t state,
                     uint8_t selected,
                     uint8_t count)
{
    if (product == NULL)
    {
        return;
    }
    product->menu_index = selected;
    product->view.menu.selected_index = selected;
    product->view.menu.item_count = count;
    sync_settings_view(product);
    set_state(product, state);
    mark_dirty(product);
}

static uint8_t state_menu_count(ui_product_state_t state)
{
    switch (state)
    {
    case UI_PRODUCT_STATE_DISPLAY_MENU:
    case UI_PRODUCT_STATE_BRIGHTNESS_EDIT:
    case UI_PRODUCT_STATE_TIMEOUT_EDIT:
        return APP_PRODUCT_DISPLAY_COUNT;
    case UI_PRODUCT_STATE_SOUND_MENU:
        return APP_PRODUCT_SOUND_COUNT;
    case UI_PRODUCT_STATE_LANGUAGE_MENU:
        return APP_PRODUCT_LANGUAGE_COUNT;
    case UI_PRODUCT_STATE_MENU:
    default:
        return APP_PRODUCT_MAIN_COUNT;
    }
}

static bool state_is_menu_like(ui_product_state_t state)
{
    return (state == UI_PRODUCT_STATE_MENU) ||
           (state == UI_PRODUCT_STATE_DISPLAY_MENU) ||
           (state == UI_PRODUCT_STATE_SOUND_MENU) ||
           (state == UI_PRODUCT_STATE_LANGUAGE_MENU) ||
           (state == UI_PRODUCT_STATE_BRIGHTNESS_EDIT) ||
           (state == UI_PRODUCT_STATE_TIMEOUT_EDIT);
}

static uint8_t clamp_brightness_step(int16_t value)
{
    if (value < 5)
    {
        return 5u;
    }
    if (value > 100)
    {
        return 100u;
    }
    return (uint8_t)(((value + 2) / 5) * 5);
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
        product->measurement_deferred_for_settings = false;
        product->calibration_deferred_for_settings = false;
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

static void request_settings_save(app_product_t *product)
{
    if (product != NULL)
    {
        product->settings_save_requested = true;
        sync_settings_view(product);
    }
}

static void apply_settings(app_product_t *product, const app_settings_t *settings)
{
    if ((product != NULL) && (settings != NULL) &&
        (app_settings_service_set(product->settings_service, settings) == BSP_STATUS_OK))
    {
        sync_settings_view(product);
    }
}

static WTK_NOINLINE void update_menu_navigation(app_product_t *product)
{
    const uint8_t count = state_menu_count(product->view.state);
    if (product->request_page_next)
    {
        product->menu_index = (uint8_t)((product->menu_index + 1u) % count);
        product->view.menu.selected_index = product->menu_index;
        mark_dirty(product);
    }
    if (product->request_page_prev)
    {
        product->menu_index = (product->menu_index == 0u) ? (uint8_t)(count - 1u) :
                                                            (uint8_t)(product->menu_index - 1u);
        product->view.menu.selected_index = product->menu_index;
        mark_dirty(product);
    }
}

static WTK_NOINLINE void handle_display_editor(app_product_t *product, bool brightness_editor)
{
    app_settings_t settings = current_settings(product);
    if (product->request_page_next || product->request_page_prev)
    {
        if (brightness_editor)
        {
            const int16_t delta = product->request_page_next ? 5 : -5;
            settings.brightness_percent = clamp_brightness_step((int16_t)settings.brightness_percent + delta);
        }
        else
        {
            settings.backlight_timeout = product->request_page_next ?
                                             app_backlight_timeout_next(settings.backlight_timeout) :
                                             app_backlight_timeout_prev(settings.backlight_timeout);
        }
        apply_settings(product, &settings);
    }
    if (product->request_menu)
    {
        apply_settings(product, &product->edit_entry_settings);
        set_menu(product, UI_PRODUCT_STATE_DISPLAY_MENU, 0u, APP_PRODUCT_DISPLAY_COUNT);
    }
    else if (product->request_click)
    {
        request_settings_save(product);
        set_menu(product,
                 UI_PRODUCT_STATE_DISPLAY_MENU,
                 brightness_editor ? APP_PRODUCT_DISPLAY_BRIGHTNESS : APP_PRODUCT_DISPLAY_TIMEOUT,
                 APP_PRODUCT_DISPLAY_COUNT);
    }
    clear_requests(product);
}

static WTK_NOINLINE void service_pending_settings_save(app_product_t *product,
                                          const app_product_inputs_t *inputs,
                                          uint32_t now_ms)
{
    if ((product == NULL) || (inputs == NULL))
    {
        return;
    }
    if (app_settings_service_busy(product->settings_service))
    {
        if (inputs->settings_storage_busy)
        {
            sync_settings_view(product);
            return;
        }
        const bsp_status_t status = app_settings_service_step(product->settings_service, now_ms);
        if ((status != BSP_STATUS_OK) && (status != BSP_STATUS_BUSY))
        {
            product->settings_save_requested = false;
        }
        sync_settings_view(product);
        return;
    }
    (void)app_settings_service_acknowledge(product->settings_service);
    if (product->settings_save_requested && app_settings_service_dirty(product->settings_service) &&
        !inputs->settings_storage_busy)
    {
        const bsp_status_t status = app_settings_service_save_start(product->settings_service, now_ms);
        if ((status != BSP_STATUS_OK) && (status != BSP_STATUS_BUSY))
        {
            product->settings_save_requested = false;
        }
        sync_settings_view(product);
    }
    if (!app_settings_service_dirty(product->settings_service))
    {
        product->settings_save_requested = false;
    }
}

static bool calibration_forces_awake(const app_product_t *product)
{
    if ((product == NULL) || (product->runtime_kind != APP_PRODUCT_RUNTIME_CALIBRATION))
    {
        return false;
    }
    switch ((app_cal_wizard_state_t)product->runtime.calibration.state)
    {
    case APP_CAL_WIZARD_CAPTURE_OPEN:
    case APP_CAL_WIZARD_CAPTURE_SHORT:
    case APP_CAL_WIZARD_CAPTURE_LOAD:
    case APP_CAL_WIZARD_COMMITTING:
    case APP_CAL_WIZARD_FAILED:
    case APP_CAL_WIZARD_SAFETY_BLOCKED:
    case APP_CAL_WIZARD_CANCELING:
        return true;
    default:
        return false;
    }
}

static WTK_NOINLINE void update_backlight_idle(app_product_t *product,
                                  const app_product_inputs_t *inputs,
                                  uint32_t now_ms)
{
    if ((product == NULL) || (inputs == NULL))
    {
        return;
    }
    const bool force_awake =
        (product->view.state == UI_PRODUCT_STATE_MEASURING) ||
        (product->view.state == UI_PRODUCT_STATE_FAULT) ||
        (product->view.state == UI_PRODUCT_STATE_SAFETY_BLOCKED) ||
        calibration_forces_awake(product) ||
        !inputs->safety_result.measure_allowed;
    if (force_awake)
    {
        if (product->backlight_sleeping)
        {
            product->backlight_sleeping = false;
            mark_dirty(product);
        }
        product->last_activity_ms = now_ms;
        return;
    }
    const app_settings_t settings = current_settings(product);
    if (settings.backlight_timeout == APP_BACKLIGHT_TIMEOUT_OFF)
    {
        return;
    }
    const uint32_t timeout_ms = (uint32_t)settings.backlight_timeout * 1000u;
    if (!product->backlight_sleeping &&
        ((uint32_t)(now_ms - product->last_activity_ms) >= timeout_ms))
    {
        product->backlight_sleeping = true;
        mark_dirty(product);
    }
}

bsp_status_t app_product_init(app_product_t *product,
                              app_calibration_service_t *calibration_service,
                              app_settings_service_t *settings_service,
                              const app_measurement_session_io_t *measurement_io,
                              const app_cal_session_io_t *calibration_io)
{
    if ((product == NULL) || (calibration_service == NULL) || (settings_service == NULL) ||
        (measurement_io == NULL) || (calibration_io == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *product = (app_product_t){0};
    product->measurement_io = *measurement_io;
    product->calibration_io = *calibration_io;
    product->calibration_service = calibration_service;
    product->settings_service = settings_service;
    product->last_activity_ms = 0u;
    product->wake_consume_button = BUTTON_ID_COUNT;
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
            .selected_index = APP_PRODUCT_MAIN_CALIBRATION,
            .item_count = APP_PRODUCT_MAIN_COUNT,
            .brightness_percent = current_settings(product).brightness_percent,
            .timeout_seconds = (uint16_t)current_settings(product).backlight_timeout,
            .sound_enabled = current_settings(product).sound_enabled,
            .language_id = current_settings(product).language_id,
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
    if (product->backlight_sleeping && (event->type == BUTTON_EVENT_PRESS))
    {
        product->backlight_sleeping = false;
        product->wake_consume_active = true;
        product->wake_consume_button = event->button;
        product->last_activity_ms = event->timestamp_ms;
        mark_dirty(product);
        return;
    }
    if (product->wake_consume_active)
    {
        if ((event->button == product->wake_consume_button) &&
            (event->type == BUTTON_EVENT_RELEASE))
        {
            product->wake_consume_active = false;
            product->wake_consume_button = BUTTON_ID_COUNT;
            product->ok_armed = false;
            product->ok_long_seen = false;
        }
        return;
    }
    if (event->type == BUTTON_EVENT_PRESS)
    {
        product->last_activity_ms = event->timestamp_ms;
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

    service_pending_settings_save(product, inputs, now_ms);
    sync_settings_view(product);
    update_backlight_idle(product, inputs, now_ms);

    const ui_product_calibration_state_t next_cal =
        ui_cal_state(inputs->calibration_status, inputs->calibration_active_valid);
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
        (product->view.resource_status != inputs->resource_status) ||
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
        product->view.resource_status = inputs->resource_status;
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

    if (inputs->resource_status != RESOURCE_STATUS_OK)
    {
        set_state(product, UI_PRODUCT_STATE_RESOURCE_ERROR);
        if (runtime_active(product))
        {
            begin_runtime_teardown(product, UI_PRODUCT_STATE_RESOURCE_ERROR);
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

    if (state_is_menu_like(product->view.state))
    {
        if (product->view.state == UI_PRODUCT_STATE_BRIGHTNESS_EDIT)
        {
            handle_display_editor(product, true);
            return;
        }
        if (product->view.state == UI_PRODUCT_STATE_TIMEOUT_EDIT)
        {
            handle_display_editor(product, false);
            return;
        }
        update_menu_navigation(product);
        if (product->request_menu)
        {
            if (product->view.state == UI_PRODUCT_STATE_MENU)
            {
                set_state(product,
                          (product->view.has_measurement_result && !product->view.measurement_result_partial) ?
                              UI_PRODUCT_STATE_RESULT :
                              UI_PRODUCT_STATE_READY);
            }
            else
            {
                set_menu(product, UI_PRODUCT_STATE_MENU, APP_PRODUCT_MAIN_CALIBRATION, APP_PRODUCT_MAIN_COUNT);
            }
        }
        else if (product->request_click)
        {
            if (product->view.state == UI_PRODUCT_STATE_MENU)
            {
                if (product->menu_index == APP_PRODUCT_MAIN_CALIBRATION)
                {
                    set_state(product, UI_PRODUCT_STATE_CALIBRATION_STATUS);
                }
                else if (product->menu_index == APP_PRODUCT_MAIN_DISPLAY)
                {
                    set_menu(product, UI_PRODUCT_STATE_DISPLAY_MENU, APP_PRODUCT_DISPLAY_BRIGHTNESS, APP_PRODUCT_DISPLAY_COUNT);
                }
                else if (product->menu_index == APP_PRODUCT_MAIN_SOUND)
                {
                    set_menu(product, UI_PRODUCT_STATE_SOUND_MENU, APP_PRODUCT_SOUND_TOGGLE, APP_PRODUCT_SOUND_COUNT);
                }
                else if (product->menu_index == APP_PRODUCT_MAIN_LANGUAGE)
                {
                    set_menu(product,
                             UI_PRODUCT_STATE_LANGUAGE_MENU,
                             APP_PRODUCT_LANGUAGE_EN,
                             APP_PRODUCT_LANGUAGE_COUNT);
                }
                else if (product->menu_index == APP_PRODUCT_MAIN_ABOUT)
                {
                    set_state(product, UI_PRODUCT_STATE_ABOUT);
                }
                else
                {
                    set_state(product,
                              (product->view.has_measurement_result && !product->view.measurement_result_partial) ?
                                  UI_PRODUCT_STATE_RESULT :
                                  UI_PRODUCT_STATE_READY);
                }
            }
            else if (product->view.state == UI_PRODUCT_STATE_DISPLAY_MENU)
            {
                if (product->menu_index == APP_PRODUCT_DISPLAY_BRIGHTNESS)
                {
                    product->edit_entry_settings = current_settings(product);
                    set_state(product, UI_PRODUCT_STATE_BRIGHTNESS_EDIT);
                }
                else if (product->menu_index == APP_PRODUCT_DISPLAY_TIMEOUT)
                {
                    product->edit_entry_settings = current_settings(product);
                    set_state(product, UI_PRODUCT_STATE_TIMEOUT_EDIT);
                }
                else
                {
                    set_menu(product, UI_PRODUCT_STATE_MENU, APP_PRODUCT_MAIN_DISPLAY, APP_PRODUCT_MAIN_COUNT);
                }
            }
            else if (product->view.state == UI_PRODUCT_STATE_SOUND_MENU)
            {
                if (product->menu_index == APP_PRODUCT_SOUND_TOGGLE)
                {
                    app_settings_t settings = current_settings(product);
                    settings.sound_enabled = !settings.sound_enabled;
                    apply_settings(product, &settings);
                    request_settings_save(product);
                    if (settings.sound_enabled)
                    {
                        product->tone_sequence++;
                    }
                }
                else
                {
                    set_menu(product, UI_PRODUCT_STATE_MENU, APP_PRODUCT_MAIN_SOUND, APP_PRODUCT_MAIN_COUNT);
                }
            }
            else if (product->view.state == UI_PRODUCT_STATE_LANGUAGE_MENU)
            {
                if ((product->menu_index == APP_PRODUCT_LANGUAGE_EN) ||
                    (product->menu_index == APP_PRODUCT_LANGUAGE_PT_BR))
                {
                    app_settings_t settings = current_settings(product);
                    settings.language_id =
                        (product->menu_index == APP_PRODUCT_LANGUAGE_EN) ?
                            (uint8_t)UI_LANGUAGE_EN :
                            (uint8_t)UI_LANGUAGE_PT_BR;
                    apply_settings(product, &settings);
                    request_settings_save(product);
                }
                else
                {
                    set_menu(product, UI_PRODUCT_STATE_MENU, APP_PRODUCT_MAIN_LANGUAGE, APP_PRODUCT_MAIN_COUNT);
                }
            }
        }
        product->request_click = false;
        product->request_menu = false;
        product->request_page_next = false;
        product->request_page_prev = false;
        return;
    }

    if (product->view.state == UI_PRODUCT_STATE_ABOUT)
    {
        if (product->request_menu || product->request_click)
        {
            set_menu(product, UI_PRODUCT_STATE_MENU, APP_PRODUCT_MAIN_ABOUT, APP_PRODUCT_MAIN_COUNT);
        }
        clear_requests(product);
        return;
    }

    if (product->view.state == UI_PRODUCT_STATE_CALIBRATION_STATUS)
    {
        if (product->request_menu)
        {
            set_state(product, UI_PRODUCT_STATE_MENU);
        }
        else if ((product->request_click || product->calibration_deferred_for_settings) &&
                 inputs->calibration_active_valid &&
                 (inputs->calibration_status != APP_CAL_SERVICE_STORAGE_UNAVAILABLE))
        {
            if (app_settings_service_busy(product->settings_service) || inputs->settings_storage_busy)
            {
                product->calibration_deferred_for_settings = true;
            }
            else if (activate_wizard_runtime(product) == BSP_STATUS_OK)
            {
                product->calibration_deferred_for_settings = false;
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
            (product->request_click || product->calibration_deferred_for_settings))
        {
            if (app_settings_service_busy(product->settings_service) || inputs->settings_storage_busy)
            {
                product->calibration_deferred_for_settings = true;
            }
            else if (activate_wizard_runtime(product) == BSP_STATUS_OK)
            {
                product->calibration_deferred_for_settings = false;
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
        set_menu(product, UI_PRODUCT_STATE_MENU, APP_PRODUCT_MAIN_CALIBRATION, APP_PRODUCT_MAIN_COUNT);
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

    if (product->request_click && state_accepts_measurement_request(product->view.state) &&
        (app_settings_service_busy(product->settings_service) || inputs->settings_storage_busy))
    {
        product->measurement_deferred_for_settings = true;
        product->request_click = false;
    }

    if ((product->request_click || product->measurement_deferred_for_settings) &&
        state_accepts_measurement_request(product->view.state) &&
        !app_settings_service_busy(product->settings_service) &&
        !inputs->settings_storage_busy)
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
        product->measurement_deferred_for_settings = false;
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

void app_product_make_view(const app_product_t *product, ui_product_view_t *view)
{
    if ((product != NULL) && (view != NULL))
    {
        *view = product->view;
    }
}

void app_product_make_outputs(const app_product_t *product, app_product_outputs_t *outputs)
{
    if ((product == NULL) || (outputs == NULL))
    {
        return;
    }
    const app_settings_t settings = current_settings(product);
    *outputs = (app_product_outputs_t){
        .backlight_percent = product->backlight_sleeping ? 0u : settings.brightness_percent,
        .sound_enabled = settings.sound_enabled,
        .tone_frequency_hz = (product->tone_sequence != 0u) ? 2000u : 0u,
        .tone_duration_ms = (product->tone_sequence != 0u) ? 30u : 0u,
        .tone_sequence = product->tone_sequence,
    };
}

uint32_t app_product_context_size_bytes(void)
{
    return (uint32_t)sizeof(app_product_t);
}
