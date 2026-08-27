#include "app/app_product.h"

#include <stddef.h>

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
        product->generation++;
        product->view.generation = product->generation;
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
    if ((product != NULL) && (product->page != page))
    {
        product->page = page;
        product->view.page = page;
        mark_dirty(product);
    }
}

static bool calibration_allows_ready(app_cal_service_status_t status)
{
    return status == APP_CAL_SERVICE_ACTIVE_VALID;
}

static bool state_accepts_measurement_request(ui_product_state_t state)
{
    return (state == UI_PRODUCT_STATE_READY) || (state == UI_PRODUCT_STATE_RESULT);
}

static void update_measurement_result(app_product_t *product, app_measurement_event_t event)
{
    if (product == NULL)
    {
        return;
    }
    if (event == APP_MEASUREMENT_EVENT_PARTIAL_RESULT)
    {
        const measurement_session_result_t *partial = app_measurement_session_partial(&product->measurement);
        if (partial != NULL)
        {
            product->view.measurement_result = *partial;
            product->view.has_measurement_result = true;
            product->view.measurement_result_partial = true;
            product->have_partial = true;
            mark_dirty(product);
        }
    }
    else if (event == APP_MEASUREMENT_EVENT_FINAL_RESULT)
    {
        const measurement_session_result_t *final = app_measurement_session_final(&product->measurement);
        if (final != NULL)
        {
            product->view.measurement_result = *final;
            product->view.has_measurement_result = true;
            product->view.measurement_result_partial = false;
            product->have_final = true;
            product->have_partial = false;
            product->next_hint = measurement_auto_make_hint(final);
            mark_dirty(product);
        }
    }
}

bsp_status_t app_product_init(app_product_t *product, const app_measurement_session_io_t *measurement_io)
{
    if ((product == NULL) || (measurement_io == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *product = (app_product_t){0};
    const bsp_status_t status = app_measurement_session_init(&product->measurement, measurement_io);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    product->page = UI_PRODUCT_PAGE_PRIMARY;
    product->view = (ui_product_view_t){
        .state = UI_PRODUCT_STATE_STARTUP,
        .page = UI_PRODUCT_PAGE_PRIMARY,
        .calibration_status = UI_PRODUCT_CAL_UNKNOWN,
        .safety_blocker = UI_PRODUCT_BLOCK_SENSOR,
        .battery_state = UI_PRODUCT_BATTERY_UNKNOWN,
        .generation = 1u,
    };
    product->generation = 1u;
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
    const uint8_t next_measurement_state = (uint8_t)app_measurement_session_state(&product->measurement);
    if ((product->view.calibration_status != next_cal) ||
        (product->view.safety_blocker != next_blocker) ||
        (product->view.battery_state != next_battery) ||
        (product->view.safety_fault_mask != inputs->safety_fault_mask) ||
        (product->view.display_ready != inputs->display_ready) ||
        (product->view.display_fault != inputs->display_fault) ||
        (product->view.storage_unavailable != next_storage_unavailable) ||
        (product->view.measurement_state != next_measurement_state))
    {
        product->view.calibration_status = next_cal;
        product->view.safety_blocker = next_blocker;
        product->view.battery_state = next_battery;
        product->view.safety_fault_mask = inputs->safety_fault_mask;
        product->view.display_ready = inputs->display_ready;
        product->view.display_fault = inputs->display_fault;
        product->view.storage_unavailable = next_storage_unavailable;
        product->view.measurement_state = next_measurement_state;
        mark_dirty(product);
    }

    if ((inputs->safety_fault_mask != 0u) || inputs->display_fault)
    {
        if (app_measurement_session_active(&product->measurement))
        {
            (void)app_measurement_session_cancel(&product->measurement);
        }
        set_state(product, UI_PRODUCT_STATE_FAULT);
        product->request_click = false;
        product->request_menu = false;
        product->request_page_next = false;
        product->request_page_prev = false;
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

    if (!calibration_allows_ready(inputs->calibration_status))
    {
        if (app_measurement_session_active(&product->measurement))
        {
            (void)app_measurement_session_cancel(&product->measurement);
        }
        set_state(product, UI_PRODUCT_STATE_CALIBRATION_REQUIRED);
        product->request_click = false;
        product->request_menu = false;
        product->request_page_next = false;
        product->request_page_prev = false;
        return;
    }

    if (app_measurement_session_active(&product->measurement))
    {
        const app_measurement_event_t event = app_measurement_session_step(&product->measurement, now_ms);
        update_measurement_result(product, event);
        set_state(product, app_measurement_session_active(&product->measurement) ?
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
        product->view.menu_not_implemented = true;
        mark_dirty(product);
        product->request_menu = false;
    }

    if (product->have_final && product->view.state == UI_PRODUCT_STATE_RESULT)
    {
        if (product->request_page_next)
        {
            set_page(product,
                     (product->page == UI_PRODUCT_PAGE_PRIMARY) ? UI_PRODUCT_PAGE_DETAILS :
                                                                  UI_PRODUCT_PAGE_PRIMARY);
        }
        if (product->request_page_prev)
        {
            set_page(product,
                     (product->page == UI_PRODUCT_PAGE_PRIMARY) ? UI_PRODUCT_PAGE_DETAILS :
                                                                  UI_PRODUCT_PAGE_PRIMARY);
        }
    }
    product->request_page_next = false;
    product->request_page_prev = false;

    if (product->request_click && state_accepts_measurement_request(product->view.state))
    {
        product->session_sequence++;
        const bsp_status_t start_status =
            app_measurement_session_start(&product->measurement,
                                          MEASUREMENT_AUTO_MODE_CLICK,
                                          product->session_sequence,
                                          MEASUREMENT_QUALIFICATION_UNQUALIFIED,
                                          product->next_hint.valid ? &product->next_hint : NULL,
                                          clock_summary,
                                          clock_status,
                                          now_ms);
        if ((start_status == BSP_STATUS_BUSY) || (start_status == BSP_STATUS_OK))
        {
            product->have_partial = false;
            product->view.measurement_result_partial = false;
            set_state(product, UI_PRODUCT_STATE_MEASURING);
        }
        product->request_click = false;
    }
    else
    {
        product->request_click = false;
        set_state(product, product->have_final ? UI_PRODUCT_STATE_RESULT : UI_PRODUCT_STATE_READY);
    }
    product->view.session_sequence = product->session_sequence;
}

void app_product_cancel(app_product_t *product)
{
    if (product != NULL)
    {
        (void)app_measurement_session_cancel(&product->measurement);
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
