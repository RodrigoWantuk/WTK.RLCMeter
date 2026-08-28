#include "app/app_shell.h"

#include "app/app_bringup_console.h"
#include "app/app_calibration_service.h"
#include "app/app_io_workspace.h"
#include "app/app_product.h"
#include "app/app_safety_fault.h"
#include "bsp/bsp_adc.h"
#include "bsp/bsp_clock.h"
#include "bsp/bsp_diagnostics.h"
#include "bsp/bsp_excitation.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_metrology_adc.h"
#include "bsp/bsp_quiet.h"
#include "bsp/bsp_reset.h"
#include "bsp/bsp_status.h"
#include "bsp/bsp_time.h"
#include "bsp/bsp_uart.h"
#include "bsp/bsp_watchdog.h"
#include "drivers/buttons.h"
#include "drivers/ili9341.h"
#include "drivers/spi_bus.h"
#include "drivers/w25q.h"
#include "hardware/hw_backlight.h"
#include "hardware/hw_aux_sensors.h"
#include "hardware/hw_buzzer.h"
#include "hardware/hw_charger.h"
#include "hardware/hw_excitation.h"
#include "hardware/hw_metrology_measure.h"
#include "hardware/hw_k2.h"
#include "hardware/hw_metrology_clock.h"
#include "hardware/hw_peripherals.h"
#include "hardware/hw_range.h"
#include "hardware/hw_safety.h"
#include "ui/ui_fallback_renderer.h"
#include "ui/ui_product.h"
#include "storage/measurement_cal_w25q_adapter.h"
#include "wtk_build_config.h"

typedef enum
{
    APP_SAFETY_SAFE_CHECK = 0,
    APP_SAFETY_WAIT_SAFE,
    APP_SAFETY_READY,
} app_safety_state_t;

static buttons_t g_buttons;
static ili9341_t g_display;
static w25q_device_t g_flash;
static hw_k1_t g_k1;
static hw_k2_t g_k2;
static hw_range_t g_range;
static hw_charger_t g_charger;
static hw_aux_sensors_t g_aux_sensors;
static app_safety_fault_latch_t g_safety_faults;
static hw_safety_result_t g_safety_result;
static app_calibration_service_t g_calibration_service;
static app_io_workspace_t g_io_workspace;
#if WTK_ENABLE_BRINGUP_CONSOLE
static app_bringup_console_t g_bringup_console;
static bool g_display_ready_reported = false;
#else
static app_product_t g_product;
static ui_product_t g_product_ui;
static hw_metrology_measure_t g_product_measure;
static uint16_t g_product_ccr_table[HW_EXCITATION_LUT_POINTS];
static bool g_product_display_fault = false;
#endif
static bool g_flash_fallback_drawn = false;
static bool g_safety_transition_reported = false;
static app_safety_state_t g_reported_safety_state = APP_SAFETY_SAFE_CHECK;
static hw_safety_primary_blocker_t g_reported_primary_blocker = HW_SAFETY_BLOCKED_SENSOR_INVALID;
static bsp_status_t g_clock_status = BSP_STATUS_ERROR;

static app_safety_state_t g_safety_state = APP_SAFETY_SAFE_CHECK;

static void app_latch_fault(uint32_t fault_mask);

static bsp_status_t app_write_k1_cmd(bool high, void *user_data)
{
    (void)user_data;
    return bsp_gpio_write_output(BSP_GPIO_OUTPUT_K1_CMD, high);
}

static bsp_status_t app_write_k2_cmd(bool high, void *user_data)
{
    (void)user_data;
    return bsp_gpio_write_output(BSP_GPIO_OUTPUT_K2_CMD, high);
}

static bsp_status_t app_write_range_enable(bool high, void *user_data)
{
    (void)user_data;
    return bsp_gpio_write_output(BSP_GPIO_OUTPUT_RANGE_EN, high);
}

static bsp_status_t app_write_range_address(uint8_t address, void *user_data)
{
    (void)user_data;
    bsp_status_t status = bsp_gpio_write_output(BSP_GPIO_OUTPUT_RANGE_A0, (address & 0x01u) != 0u);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    status = bsp_gpio_write_output(BSP_GPIO_OUTPUT_RANGE_A1, (address & 0x02u) != 0u);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    return bsp_gpio_write_output(BSP_GPIO_OUTPUT_RANGE_A2, (address & 0x04u) != 0u);
}

static bsp_status_t app_read_charger_gpio(bool *high, void *user_data)
{
    (void)user_data;
    return bsp_gpio_read_input(BSP_GPIO_INPUT_CHARGER_DETECT, high);
}

static bsp_status_t app_adc_start(bsp_adc_channel_t channel, uint32_t now_ms, void *user_data)
{
    (void)user_data;
    return bsp_adc_start(channel, now_ms);
}

static bsp_status_t app_adc_poll(uint16_t *raw, uint32_t now_ms, void *user_data)
{
    (void)user_data;
    return bsp_adc_poll(raw, now_ms);
}

static void app_adc_cancel(void *user_data)
{
    (void)user_data;
    bsp_adc_cancel();
}

#if !WTK_ENABLE_BRINGUP_CONSOLE
static hw_excitation_mode_t app_bsp_excitation_mode(void)
{
    switch (bsp_excitation_mode())
    {
    case BSP_EXCITATION_MODE_NEUTRAL:
        return HW_EXCITATION_MODE_NEUTRAL;
    case BSP_EXCITATION_MODE_SINE:
        return HW_EXCITATION_MODE_SINE;
    case BSP_EXCITATION_MODE_OFF:
    default:
        return HW_EXCITATION_MODE_OFF;
    }
}

static bsp_status_t product_k1_force_safe(void *user)
{
    (void)user;
    return hw_k1_force_safe(&g_k1);
}

static bsp_status_t product_k1_request_measure(const hw_safety_result_t *permission, void *user)
{
    (void)user;
    return hw_k1_request_measure(&g_k1, permission);
}

static hw_k1_state_t product_k1_commanded_state(void *user)
{
    (void)user;
    return hw_k1_commanded_state(&g_k1);
}

static bsp_status_t product_range_request(hw_range_id_t id, uint32_t now_ms, void *user)
{
    (void)user;
    return hw_range_request(&g_range, id, now_ms);
}

static bsp_status_t product_range_step(uint32_t now_ms, void *user)
{
    (void)user;
    return hw_range_step(&g_range, now_ms);
}

static bool product_range_is_ready(void *user)
{
    (void)user;
    return hw_range_is_ready(&g_range);
}

static hw_range_id_t product_range_current_id(void *user)
{
    (void)user;
    return hw_range_get_current(&g_range);
}

static hw_safety_range_state_t product_range_safety_state(void *user)
{
    (void)user;
    return hw_range_safety_state(&g_range);
}

static bsp_status_t product_range_force_disabled(void *user)
{
    (void)user;
    return hw_range_force_disabled(&g_range);
}

static void product_quiet_request(bool requested, void *user)
{
    (void)user;
    hw_peripherals_request_quiet(requested);
}

static void product_aux_pause(void *user)
{
    (void)user;
    hw_aux_sensors_pause(&g_aux_sensors);
}

static void product_aux_resume(uint32_t now_ms, void *user)
{
    (void)user;
    hw_aux_sensors_resume(&g_aux_sensors, now_ms);
}

static bsp_status_t product_adc_acquire(uint32_t now_ms, void *user)
{
    (void)user;
    return bsp_metrology_adc_acquire(now_ms);
}

static bsp_status_t product_adc_start_capture(uint32_t *raw_words,
                                              uint32_t word_count,
                                              const hw_metrology_adc_profile_t *profile,
                                              void *user)
{
    (void)user;
    if (profile == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    return bsp_metrology_adc_start_capture(raw_words, word_count, profile->tim2_arr, profile->tim2_ccr2);
}

static void product_adc_stop(void *user)
{
    (void)user;
    bsp_metrology_adc_stop();
}

static bsp_status_t product_adc_restore(uint32_t now_ms, void *user)
{
    (void)user;
    return bsp_metrology_adc_restore(now_ms);
}

static bool product_adc_dma_complete(void *user)
{
    (void)user;
    return bsp_metrology_adc_dma_complete();
}

static bool product_adc_dma_error(void *user)
{
    (void)user;
    return bsp_metrology_adc_dma_error();
}

static bsp_status_t product_excitation_off(void *user)
{
    (void)user;
    return bsp_excitation_off();
}

static bsp_status_t product_excitation_neutral(void *user)
{
    (void)user;
    return bsp_excitation_neutral();
}

static bsp_status_t product_excitation_sine(hw_excitation_freq_t frequency,
                                            hw_excitation_amp_t amplitude,
                                            void *user)
{
    (void)user;
    hw_excitation_freq_profile_t profile;
    if (hw_excitation_freq_profile(frequency, &profile) != BSP_STATUS_OK)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (hw_excitation_fill_ccr_table(g_product_ccr_table, HW_EXCITATION_LUT_POINTS, amplitude) != BSP_STATUS_OK)
    {
        return BSP_STATUS_ERROR;
    }
    return bsp_excitation_sine(profile.rcr, g_product_ccr_table, HW_EXCITATION_LUT_POINTS);
}

static hw_excitation_mode_t product_excitation_mode(void *user)
{
    (void)user;
    return app_bsp_excitation_mode();
}

static bool product_excitation_dma_error(void *user)
{
    (void)user;
    return bsp_excitation_dma_error();
}

static hw_charger_state_t product_charger_state(void *user)
{
    (void)user;
    return hw_charger_get_state(&g_charger);
}

static uint32_t product_safety_fault_mask(void *user)
{
    (void)user;
    return app_safety_fault_mask(&g_safety_faults);
}

static bsp_status_t product_permit_issue_input(hw_measure_permit_issue_input_t *input, void *user)
{
    (void)user;
    if (input == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    hw_aux_sensors_snapshot_t snapshot;
    const uint32_t now_ms = bsp_time_now_ms();
    hw_aux_sensors_snapshot(&g_aux_sensors, now_ms, &snapshot);
    input->charger = hw_charger_get_state(&g_charger);
    input->residual = snapshot.residual_state;
    input->residual_age_ms = snapshot.residual_age_ms;
    input->battery = snapshot.battery_state;
    input->battery_age_ms = snapshot.battery_age_ms;
    input->range = hw_range_safety_state(&g_range);
    input->range_id = hw_range_get_current(&g_range);
    input->k1_state = hw_k1_commanded_state(&g_k1);
    input->safety_fault_mask = app_safety_fault_mask(&g_safety_faults);
    return BSP_STATUS_OK;
}

static bsp_status_t product_permit_validate_input(hw_measure_permit_validate_input_t *input, void *user)
{
    (void)user;
    if (input == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    input->charger = hw_charger_get_state(&g_charger);
    input->range = hw_range_safety_state(&g_range);
    input->range_id = hw_range_get_current(&g_range);
    input->k1_state = hw_k1_commanded_state(&g_k1);
    input->safety_fault_mask = app_safety_fault_mask(&g_safety_faults);
    return BSP_STATUS_OK;
}

static void product_latch_k1_io_fault(void *user)
{
    (void)user;
    app_latch_fault(APP_SAFETY_FAULT_K1_IO);
}

static void product_latch_range_io_fault(void *user)
{
    (void)user;
    app_latch_fault(APP_SAFETY_FAULT_RANGE_IO);
}

static void product_latch_adc_runtime_fault(void *user)
{
    (void)user;
    app_latch_fault(APP_SAFETY_FAULT_ADC_RUNTIME);
}

static void product_latch_metrology_runtime_fault(void *user)
{
    (void)user;
    app_latch_fault(APP_SAFETY_FAULT_METROLOGY_RUNTIME);
}

static bsp_status_t product_init_metrology_measure(void)
{
    const hw_metrology_measure_io_t io = {
        .k1_force_safe = product_k1_force_safe,
        .k1_request_measure = product_k1_request_measure,
        .k1_commanded_state = product_k1_commanded_state,
        .range_request = product_range_request,
        .range_step = product_range_step,
        .range_is_ready = product_range_is_ready,
        .range_current_id = product_range_current_id,
        .range_safety_state = product_range_safety_state,
        .range_force_disabled = product_range_force_disabled,
        .quiet_request = product_quiet_request,
        .aux_pause = product_aux_pause,
        .aux_resume = product_aux_resume,
        .adc_acquire = product_adc_acquire,
        .adc_start_capture = product_adc_start_capture,
        .adc_stop = product_adc_stop,
        .adc_restore = product_adc_restore,
        .adc_dma_complete = product_adc_dma_complete,
        .adc_dma_error = product_adc_dma_error,
        .excitation_off = product_excitation_off,
        .excitation_neutral = product_excitation_neutral,
        .excitation_sine = product_excitation_sine,
        .excitation_mode = product_excitation_mode,
        .excitation_dma_error = product_excitation_dma_error,
        .charger_state = product_charger_state,
        .safety_fault_mask = product_safety_fault_mask,
        .permit_issue_input = product_permit_issue_input,
        .permit_validate_input = product_permit_validate_input,
        .latch_k1_io_fault = product_latch_k1_io_fault,
        .latch_range_io_fault = product_latch_range_io_fault,
        .latch_adc_runtime_fault = product_latch_adc_runtime_fault,
        .latch_metrology_runtime_fault = product_latch_metrology_runtime_fault,
        .user = NULL,
    };
    return hw_metrology_measure_init(&g_product_measure,
                                     &io,
                                     app_io_workspace_metrology_raw_words(&g_io_workspace),
                                     HW_METROLOGY_RAW_WORD_COUNT);
}

static bsp_status_t product_auto_start_attempt(const hw_metrology_measure_request_t *request,
                                               uint32_t now_ms,
                                               void *user)
{
    (void)user;
    const bsp_status_t workspace_status =
        app_io_workspace_acquire(&g_io_workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
    if (workspace_status != BSP_STATUS_OK)
    {
        return workspace_status;
    }
    const bsp_status_t status = hw_metrology_measure_start(&g_product_measure, request, now_ms);
    if ((status != BSP_STATUS_OK) && (status != BSP_STATUS_BUSY))
    {
        (void)app_io_workspace_release(&g_io_workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
    }
    return status;
}

static bsp_status_t product_auto_step_attempt(uint32_t now_ms, void *user)
{
    (void)user;
    return hw_metrology_measure_step(&g_product_measure, now_ms);
}

static bool product_auto_attempt_active(void *user)
{
    (void)user;
    return hw_metrology_measure_active(&g_product_measure);
}

static bool product_auto_attempt_done(void *user)
{
    (void)user;
    return hw_metrology_measure_state(&g_product_measure) == HW_METROLOGY_MEASURE_DONE;
}

static bool product_auto_attempt_dumpable(void *user)
{
    (void)user;
    return hw_metrology_measure_dumpable(&g_product_measure);
}

static const hw_metrology_block_t *product_auto_attempt_block(void *user)
{
    (void)user;
    return hw_metrology_measure_block(&g_product_measure);
}

static hw_metrology_measure_error_t product_auto_attempt_error(void *user)
{
    (void)user;
    return hw_metrology_measure_error(&g_product_measure);
}

static void product_auto_attempt_acknowledge(void *user)
{
    (void)user;
    hw_metrology_measure_acknowledge(&g_product_measure);
    if (app_io_workspace_owner(&g_io_workspace) == APP_IO_WORKSPACE_OWNER_METROLOGY)
    {
        (void)app_io_workspace_release(&g_io_workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
    }
}

static bsp_status_t product_auto_attempt_abort(void *user)
{
    (void)user;
    return hw_metrology_measure_abort(&g_product_measure);
}

static bsp_status_t product_auto_process_block(const hw_metrology_block_t *block,
                                               const measurement_attempt_config_t *attempt,
                                               measurement_calibrated_result_t *result,
                                               void *user)
{
    (void)user;
    if ((attempt == NULL) || (result == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    const measurement_cal_key_t key = measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                                                          MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                                          attempt->range_id,
                                                          attempt->frequency,
                                                          attempt->amplitude);
    return measurement_cal_process_block(block,
                                         app_calibration_service_active_set(&g_calibration_service),
                                         &key,
                                         false,
                                         result);
}

static bsp_status_t product_init_controller(void)
{
    const bsp_status_t measure_status = product_init_metrology_measure();
    if (measure_status != BSP_STATUS_OK)
    {
        return measure_status;
    }
    const app_measurement_session_io_t session_io = {
        .start_attempt = product_auto_start_attempt,
        .step_attempt = product_auto_step_attempt,
        .attempt_active = product_auto_attempt_active,
        .attempt_done = product_auto_attempt_done,
        .attempt_dumpable = product_auto_attempt_dumpable,
        .attempt_block = product_auto_attempt_block,
        .attempt_error = product_auto_attempt_error,
        .attempt_acknowledge = product_auto_attempt_acknowledge,
        .attempt_abort = product_auto_attempt_abort,
        .process_block = product_auto_process_block,
        .user = NULL,
    };
    const app_cal_session_io_t calibration_io = {
        .start_capture = product_auto_start_attempt,
        .step_capture = product_auto_step_attempt,
        .capture_active = product_auto_attempt_active,
        .capture_done = product_auto_attempt_done,
        .capture_dumpable = product_auto_attempt_dumpable,
        .capture_block = product_auto_attempt_block,
        .capture_error = product_auto_attempt_error,
        .capture_acknowledge = product_auto_attempt_acknowledge,
        .capture_abort = product_auto_attempt_abort,
        .user = NULL,
    };
    ui_product_init(&g_product_ui);
    return app_product_init(&g_product, &g_calibration_service, &session_io, &calibration_io);
}
#endif

static void app_latch_fault(uint32_t fault_mask)
{
    app_safety_fault_latch(&g_safety_faults, fault_mask);
}

static void app_record_status_fault(bsp_status_t status, uint32_t fault_mask)
{
    if (status != BSP_STATUS_OK)
    {
        app_latch_fault(fault_mask);
    }
}

static uint8_t app_read_button_mask(void)
{
    uint8_t mask = 0u;
    bool active = false;

    if ((bsp_gpio_read_input(BSP_GPIO_INPUT_BUTTON_UP, &active) == BSP_STATUS_OK) && active)
    {
        mask |= (uint8_t)(1u << (uint8_t)BUTTON_ID_UP);
    }
    if ((bsp_gpio_read_input(BSP_GPIO_INPUT_BUTTON_OK, &active) == BSP_STATUS_OK) && active)
    {
        mask |= (uint8_t)(1u << (uint8_t)BUTTON_ID_OK);
    }
    if ((bsp_gpio_read_input(BSP_GPIO_INPUT_BUTTON_DOWN, &active) == BSP_STATUS_OK) && active)
    {
        mask |= (uint8_t)(1u << (uint8_t)BUTTON_ID_DOWN);
    }

    return mask;
}

static void app_log_button_event(const button_event_t *event)
{
    if (event == NULL)
    {
        return;
    }

    if (bsp_quiet_requested())
    {
        return;
    }

#if WTK_ENABLE_BRINGUP_CONSOLE
    const char *button_name = "UNKNOWN";
    switch (event->button)
    {
    case BUTTON_ID_UP:
        button_name = "UP";
        break;
    case BUTTON_ID_OK:
        button_name = "OK";
        break;
    case BUTTON_ID_DOWN:
        button_name = "DOWN";
        break;
    default:
        break;
    }

    bsp_uart_write_cstr("button: ");
    bsp_uart_write_cstr(button_name);
    bsp_uart_write_cstr(" ");
    bsp_uart_write_cstr(button_event_type_string(event->type));
    bsp_uart_write_cstr("\r\n");
#else
    bsp_diagnostics_write(BSP_LOG_LEVEL_DEBUG, button_event_type_string(event->type));
#endif
}

static const char *app_safety_state_string(app_safety_state_t state)
{
    switch (state)
    {
    case APP_SAFETY_READY:
        return "READY";
    case APP_SAFETY_WAIT_SAFE:
        return "WAIT_SAFE";
    case APP_SAFETY_SAFE_CHECK:
    default:
        return "SAFE_CHECK";
    }
}

static void app_update_safety_state(void)
{
    const uint32_t now_ms = bsp_time_now_ms();
    const hw_safety_input_t safety_input = {
        .charger = hw_charger_get_state(&g_charger),
        .residual = hw_aux_sensors_residual_state(&g_aux_sensors, now_ms),
        .battery = hw_aux_sensors_battery_state(&g_aux_sensors, now_ms),
        .range = hw_range_safety_state(&g_range),
        .application_fault = app_safety_fault_any(&g_safety_faults),
    };

    g_safety_result = hw_safety_evaluate(&safety_input);
    if (g_safety_result.measure_allowed)
    {
        g_safety_state = APP_SAFETY_READY;
    }
    else
    {
        g_safety_state = APP_SAFETY_WAIT_SAFE;
    }

    if (!hw_metrology_measure_k1_owned())
    {
        const bsp_status_t k1_safe_status = hw_k1_force_safe(&g_k1);
        app_record_status_fault(k1_safe_status, APP_SAFETY_FAULT_K1_IO);
    }

    const uint32_t range_kill_faults =
        app_safety_fault_mask(&g_safety_faults) & ~(uint32_t)APP_SAFETY_FAULT_CLOCK;
    if (range_kill_faults != 0u)
    {
        const bsp_status_t range_status = hw_range_force_disabled(&g_range);
        app_record_status_fault(range_status, APP_SAFETY_FAULT_RANGE_IO);
    }

    if (!g_safety_transition_reported ||
        (g_safety_state != g_reported_safety_state) ||
        (g_safety_result.primary_blocker != g_reported_primary_blocker))
    {
        bsp_diagnostics_write_key_value_text("safety_state", app_safety_state_string(g_safety_state));
        bsp_diagnostics_write_key_value_text("safety_block",
                                             hw_safety_primary_blocker_string(g_safety_result.primary_blocker));
        g_reported_safety_state = g_safety_state;
        g_reported_primary_blocker = g_safety_result.primary_blocker;
        g_safety_transition_reported = true;
    }
}

static void app_step(void)
{
    const uint32_t now_ms = bsp_time_now_ms();
    const bsp_status_t range_step_status = hw_range_step(&g_range, now_ms);
    if ((range_step_status != BSP_STATUS_OK) && (range_step_status != BSP_STATUS_BUSY))
    {
        app_latch_fault(APP_SAFETY_FAULT_RANGE_IO);
    }
    const bsp_status_t sensor_status = hw_aux_sensors_step(&g_aux_sensors, now_ms);
    if ((sensor_status != BSP_STATUS_OK) && (sensor_status != BSP_STATUS_BUSY))
    {
        app_latch_fault(APP_SAFETY_FAULT_ADC_RUNTIME);
    }
    if (hw_aux_sensors_fault_mask(&g_aux_sensors) != 0u)
    {
        app_latch_fault(APP_SAFETY_FAULT_ADC_RUNTIME);
    }
#if !WTK_ENABLE_BRINGUP_CONSOLE
    const bsp_status_t cal_step_status = app_calibration_service_step(&g_calibration_service, now_ms);
    if ((cal_step_status != BSP_STATUS_OK) && (cal_step_status != BSP_STATUS_BUSY))
    {
        bsp_diagnostics_write_key_value_text("calibration_step", bsp_status_string(cal_step_status));
    }
#endif
    app_update_safety_state();

    buttons_update(&g_buttons, app_read_button_mask(), now_ms);

    button_event_t event;
    while (buttons_pop_event(&g_buttons, &event))
    {
        app_log_button_event(&event);
#if !WTK_ENABLE_BRINGUP_CONSOLE
        app_product_handle_button_event(&g_product, &event);
#endif
    }

    (void)ili9341_init_step(&g_display, now_ms);
#if WTK_ENABLE_BRINGUP_CONSOLE
    if (g_display.ready && !g_display_ready_reported)
    {
        bsp_uart_write_cstr("display: READY\r\n");
        g_display_ready_reported = true;
    }
#endif
#if WTK_ENABLE_BRINGUP_CONSOLE
    if (g_display.ready && !g_flash.detected && !g_flash_fallback_drawn)
    {
        if (ui_fallback_draw_text(&g_display, 8u, 8u, "FLASH ERROR", 0xFFFFu, 0x0000u) == BSP_STATUS_OK)
        {
            g_flash_fallback_drawn = true;
            bsp_uart_write_cstr("fallback_ui: FLASH_ERROR_DRAWN\r\n");
        }
    }
#endif

#if WTK_ENABLE_BRINGUP_CONSOLE
    app_bringup_console_step(&g_bringup_console,
                         &g_flash,
                         &g_display,
                         &g_range,
                         &g_charger,
                         &g_aux_sensors,
                         &g_k1,
                         &g_safety_result,
                         &g_safety_faults,
                         now_ms);
    if (!app_bringup_console_flash_busy(&g_bringup_console) && !app_bringup_console_capture_busy(&g_bringup_console))
    {
        (void)w25q_device_poll(&g_flash, now_ms);
    }
#else
    hw_aux_sensors_snapshot_t product_sensor_snapshot;
    hw_aux_sensors_snapshot(&g_aux_sensors, now_ms, &product_sensor_snapshot);
    const int32_t ntc_temperature_mC =
        product_sensor_snapshot.ntc_temperature_valid ?
            (int32_t)(product_sensor_snapshot.ntc_temperature_c * 1000.0f) :
            0;
    app_product_inputs_t product_inputs = {
        .calibration_status = app_calibration_service_status(&g_calibration_service),
        .calibration_active_valid = app_calibration_service_active_valid(&g_calibration_service),
        .calibration_active_sequence = app_calibration_service_active_sequence(&g_calibration_service),
        .temperature_mC = ntc_temperature_mC,
        .temperature_valid = product_sensor_snapshot.ntc_temperature_valid,
        .safety_result = g_safety_result,
        .battery_state = product_sensor_snapshot.battery_state,
        .safety_fault_mask = app_safety_fault_mask(&g_safety_faults),
        .display_ready = g_display.ready,
        .display_fault = g_product_display_fault || (g_display.init_state == ILI9341_INIT_ERROR),
    };
    app_product_step(&g_product,
                     &product_inputs,
                     bsp_clock_get_summary(),
                     g_clock_status,
                     now_ms);
    ui_product_view_t product_view;
    app_product_make_view(&g_product, &product_view);
    ui_product_request(&g_product_ui, &product_view);
    const bsp_status_t ui_status =
        ui_product_step(&g_product_ui, &g_display, hw_peripherals_quiet_requested());
    if ((ui_status != BSP_STATUS_OK) && (ui_status != BSP_STATUS_BUSY))
    {
        g_product_display_fault = true;
    }
    if (!app_calibration_service_busy(&g_calibration_service))
    {
        (void)w25q_device_poll(&g_flash, now_ms);
    }
#endif
    hw_buzzer_step(now_ms);
}

void app_shell_run(void)
{
    const bsp_reset_reason_t reset_reason = bsp_reset_capture_reason();

    app_safety_fault_init(&g_safety_faults);
    app_calibration_service_init(&g_calibration_service);
    app_io_workspace_init(&g_io_workspace);
    app_calibration_service_attach_workspace(&g_calibration_service, &g_io_workspace);
    const bsp_status_t gpio_status = bsp_gpio_init_safe();
    app_record_status_fault(gpio_status, APP_SAFETY_FAULT_GPIO_INIT);
    const bsp_status_t clock_status = bsp_clock_init();
    g_clock_status = clock_status;
    (void)bsp_time_init();
    (void)bsp_uart_init(115200u);

    bsp_diagnostics_boot_banner(reset_reason, clock_status);
    if (!hw_metrology_clock_ready(bsp_clock_get_summary(), clock_status))
    {
        app_latch_fault(APP_SAFETY_FAULT_CLOCK);
    }
    (void)bsp_watchdog_start();

    buttons_init(&g_buttons, NULL);
    const hw_k1_io_t k1_io = {
        .write_cmd = app_write_k1_cmd,
        .user_data = NULL,
    };
    const hw_k2_io_t k2_io = {
        .write_cmd = app_write_k2_cmd,
        .user_data = NULL,
    };
    const hw_range_io_t range_io = {
        .write_enable = app_write_range_enable,
        .write_address = app_write_range_address,
        .user_data = NULL,
    };
    const hw_charger_io_t charger_io = {
        .read_gpio = app_read_charger_gpio,
        .user_data = NULL,
    };
    const bsp_status_t k1_status = hw_k1_init(&g_k1, &k1_io);
    app_record_status_fault(k1_status, APP_SAFETY_FAULT_K1_IO);
    bsp_diagnostics_write_key_value_text("k1", bsp_status_string(k1_status));
    (void)bsp_excitation_init();
    const bsp_status_t k2_status = hw_k2_init(&g_k2, &k2_io);
    app_record_status_fault(k2_status, APP_SAFETY_FAULT_K2_IO);
    bsp_diagnostics_write_key_value_text("k2", bsp_status_string(k2_status));
    const bsp_status_t range_status = hw_range_init(&g_range, &range_io);
    app_record_status_fault(range_status, APP_SAFETY_FAULT_RANGE_IO);
    bsp_diagnostics_write_key_value_text("range", bsp_status_string(range_status));
    const bsp_status_t charger_init_status = hw_charger_init(&g_charger, &charger_io);
    bsp_diagnostics_write_key_value_text("charger_init", bsp_status_string(charger_init_status));
    const bsp_status_t adc_status = bsp_adc_init(bsp_time_now_ms());
    app_record_status_fault(adc_status, APP_SAFETY_FAULT_ADC_INIT);
    bsp_diagnostics_write_key_value_text("adc", bsp_status_string(adc_status));
    const hw_aux_adc_io_t aux_adc_io = {
        .start = app_adc_start,
        .poll = app_adc_poll,
        .cancel = app_adc_cancel,
        .user_data = NULL,
    };
    const bsp_status_t aux_status = hw_aux_sensors_init(&g_aux_sensors, &aux_adc_io, bsp_time_now_ms());
    app_record_status_fault(aux_status, APP_SAFETY_FAULT_ADC_INIT);
    bsp_diagnostics_write_key_value_text("aux_sensors", bsp_status_string(aux_status));
    bsp_diagnostics_write_key_value_text("charger", hw_charger_state_string(hw_charger_get_state(&g_charger)));
    bsp_diagnostics_write_key_value_text("k2_topology", hw_lowz_bank_mode_string(hw_k2_topology(&g_k2).lowz_bank_mode));
    bsp_diagnostics_write_key_value_hex8("safety_faults", app_safety_fault_mask(&g_safety_faults));
    g_safety_result = hw_safety_evaluate(NULL);
    g_safety_state = APP_SAFETY_SAFE_CHECK;
    g_safety_transition_reported = false;
    g_reported_safety_state = APP_SAFETY_SAFE_CHECK;
    g_reported_primary_blocker = HW_SAFETY_BLOCKED_SENSOR_INVALID;
#if WTK_ENABLE_BRINGUP_CONSOLE
    app_bringup_console_init(&g_bringup_console);
    app_bringup_console_attach_calibration_service(&g_bringup_console, &g_calibration_service);
    app_bringup_console_attach_workspace(&g_bringup_console, &g_io_workspace);
    g_display_ready_reported = false;
#else
    const bsp_status_t product_status = product_init_controller();
    app_record_status_fault(product_status, APP_SAFETY_FAULT_METROLOGY_RUNTIME);
    bsp_diagnostics_write_key_value_text("product", bsp_status_string(product_status));
    g_product_display_fault = false;
#endif
    w25q_device_init(&g_flash);
    ili9341_init_context(&g_display);
    g_flash_fallback_drawn = false;

    const bsp_status_t spi_status = spi_bus_init();
    bsp_diagnostics_write_key_value_text("spi2", bsp_status_string(spi_status));

    const bsp_status_t backlight_status = hw_backlight_init();
    bsp_diagnostics_write_key_value_text("backlight", bsp_status_string(backlight_status));
    if (backlight_status == BSP_STATUS_OK)
    {
        (void)hw_backlight_set_percent(25u);
        bsp_diagnostics_write_key_value_u32("backlight_pwm_hz", hw_backlight_pwm_hz());
    }

    const bsp_status_t buzzer_status = hw_buzzer_init();
    bsp_diagnostics_write_key_value_text("buzzer", bsp_status_string(buzzer_status));

    const w25q_status_t flash_status = w25q_device_probe(&g_flash);
    bsp_diagnostics_write_key_value_text("w25q", w25q_status_string(flash_status));
    if (flash_status == W25Q_STATUS_OK)
    {
        const measurement_cal_store_io_t cal_io = measurement_cal_w25q_store_io(&g_flash);
        const bsp_status_t cal_status =
            app_calibration_service_load(&g_calibration_service, &cal_io, g_flash.part.capacity_bytes);
        bsp_diagnostics_write_key_value_text("calibration", bsp_status_string(cal_status));
        bsp_diagnostics_write_key_value_text("calibration_state",
                                             app_calibration_service_status_string(
                                                 app_calibration_service_status(&g_calibration_service)));
        const unsigned int jedec = ((unsigned int)g_flash.part.jedec.manufacturer_id << 16u) |
                                   ((unsigned int)g_flash.part.jedec.memory_type << 8u) |
                                   (unsigned int)g_flash.part.jedec.capacity_code;
        bsp_diagnostics_write_key_value_text("w25q_part", g_flash.part.name);
        bsp_diagnostics_write_key_value_hex8("w25q_jedec", jedec);
        bsp_diagnostics_write_key_value_u32("w25q_capacity", g_flash.part.capacity_bytes);
        bsp_diagnostics_write_key_value_u32("w25q_test_sector",
                                            w25q_reserved_test_sector_address(g_flash.part.capacity_bytes));
    }
    else
    {
        app_calibration_service_mark_storage_unavailable(&g_calibration_service);
        bsp_diagnostics_write_key_value_text("calibration_state",
                                             app_calibration_service_status_string(
                                                 app_calibration_service_status(&g_calibration_service)));
    }

    ili9341_init_start(&g_display, bsp_time_now_ms());

    for (;;)
    {
        app_step();
        bsp_diagnostics_step();
        bsp_watchdog_service();
    }
}
