#ifndef WTK_APP_PRODUCT_H
#define WTK_APP_PRODUCT_H

#include <stdbool.h>
#include <stdint.h>

#include "app/app_calibration_service.h"
#include "app/app_calibration_wizard.h"
#include "app/app_measurement_session.h"
#include "app/app_safety_fault.h"
#include "drivers/buttons.h"
#include "hardware/hw_power.h"
#include "hardware/hw_safety.h"
#include "ui/ui_product.h"

typedef struct
{
    app_cal_service_status_t calibration_status;
    bool calibration_active_valid;
    uint32_t calibration_active_sequence;
    int32_t temperature_mC;
    bool temperature_valid;
    hw_safety_result_t safety_result;
    hw_battery_state_t battery_state;
    uint32_t safety_fault_mask;
    bool display_ready;
    bool display_fault;
} app_product_inputs_t;

typedef struct
{
    union
    {
        app_measurement_session_t measurement;
        app_calibration_wizard_t calibration;
    } runtime;
    app_measurement_session_io_t measurement_io;
    app_cal_session_io_t calibration_io;
    app_calibration_service_t *calibration_service;
    measurement_auto_hint_t next_hint;
    ui_product_view_t view;
    uint32_t session_sequence;
    uint32_t calibration_sequence;
    uint8_t menu_index;
    uint8_t runtime_kind;
    uint8_t runtime_teardown_kind;
    uint8_t runtime_teardown_target_state;
    bool ok_armed;
    bool ok_long_seen;
    bool request_click;
    bool request_menu;
    bool request_page_next;
    bool request_page_prev;
    bool initialized;
} app_product_t;

bsp_status_t app_product_init(app_product_t *product,
                              app_calibration_service_t *calibration_service,
                              const app_measurement_session_io_t *measurement_io,
                              const app_cal_session_io_t *calibration_io);
void app_product_handle_button_event(app_product_t *product, const button_event_t *event);
void app_product_step(app_product_t *product,
                      const app_product_inputs_t *inputs,
                      const bsp_clock_summary_t *clock_summary,
                      bsp_status_t clock_status,
                      uint32_t now_ms);
void app_product_cancel(app_product_t *product);
void app_product_make_view(const app_product_t *product, ui_product_view_t *view);
uint32_t app_product_context_size_bytes(void);

#endif
