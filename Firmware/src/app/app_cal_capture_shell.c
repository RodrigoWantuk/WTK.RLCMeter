/* Exclusive BRINGUP_CAL shell. Acquisition/permits are the existing hardware FSM. */
#include "app/app_shell.h"

#include "app/app_cal_capture_service.h"
#include "app/app_io_workspace.h"
#include "app/app_safety_fault.h"
#include "bsp/bsp_adc.h"
#include "bsp/bsp_clock.h"
#include "bsp/bsp_excitation.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_metrology_adc.h"
#include "bsp/bsp_reset.h"
#include "bsp/bsp_time.h"
#include "bsp/bsp_uart.h"
#include "bsp/bsp_watchdog.h"
#include "drivers/spi_bus.h"
#include "drivers/w25q.h"
#include "hardware/hw_aux_sensors.h"
#include "hardware/hw_backlight.h"
#include "hardware/hw_buzzer.h"
#include "hardware/hw_charger.h"
#include "hardware/hw_k2.h"
#include "hardware/hw_peripherals.h"
#include "storage/measurement_cal_w25q_adapter.h"

static hw_k1_t k1;
static hw_k2_t k2;
static hw_range_t range;
static hw_charger_t charger;
static hw_aux_sensors_t sensors;
static app_safety_fault_latch_t faults;
static app_io_workspace_t workspace;
static hw_metrology_measure_t measure;
static app_calibration_service_t calibration;
static app_cal_capture_service_t capture;
static w25q_device_t flash;
static uint16_t sine_table[HW_EXCITATION_LUT_POINTS];

static void latch(bsp_status_t status, uint32_t mask)
{
    if (status != BSP_STATUS_OK && status != BSP_STATUS_BUSY) app_safety_fault_latch(&faults, mask);
}

static bsp_status_t k1_write(bool high, void *user)
{ (void)user; return bsp_gpio_write_output(BSP_GPIO_OUTPUT_K1_CMD, high); }
static bsp_status_t k2_write(bool high, void *user)
{ (void)user; return bsp_gpio_write_output(BSP_GPIO_OUTPUT_K2_CMD, high); }
static bsp_status_t range_enable(bool high, void *user)
{ (void)user; return bsp_gpio_write_output(BSP_GPIO_OUTPUT_RANGE_EN, high); }
static bsp_status_t range_address(uint8_t value, void *user)
{
    (void)user;
    bsp_status_t status = bsp_gpio_write_output(BSP_GPIO_OUTPUT_RANGE_A0, (value&1u) != 0u);
    if (status == BSP_STATUS_OK) status = bsp_gpio_write_output(BSP_GPIO_OUTPUT_RANGE_A1, (value&2u) != 0u);
    if (status == BSP_STATUS_OK) status = bsp_gpio_write_output(BSP_GPIO_OUTPUT_RANGE_A2, (value&4u) != 0u);
    return status;
}
static bsp_status_t charger_read(bool *high, void *user)
{ (void)user; return bsp_gpio_read_input(BSP_GPIO_INPUT_CHARGER_DETECT, high); }
static bsp_status_t aux_start(bsp_adc_channel_t channel, uint32_t now, void *user)
{ (void)user; return bsp_adc_start(channel, now); }
static bsp_status_t aux_poll(uint16_t *raw, uint32_t now, void *user)
{ (void)user; return bsp_adc_poll(raw, now); }
static void aux_cancel(void *user)
{ (void)user; bsp_adc_cancel(); }

static bsp_status_t force_safe(void *user)
{ (void)user; return hw_k1_force_safe(&k1); }
static bsp_status_t request_measure(const hw_safety_result_t *permission, void *user)
{ (void)user; return hw_k1_request_measure(&k1, permission); }
static hw_k1_state_t k1_state(void *user)
{ (void)user; return hw_k1_commanded_state(&k1); }
static bsp_status_t range_request(hw_range_id_t id, uint32_t now, void *user)
{ (void)user; return hw_range_request(&range, id, now); }
static bsp_status_t range_step(uint32_t now, void *user)
{ (void)user; return hw_range_step(&range, now); }
static bool range_ready(void *user)
{ (void)user; return hw_range_is_ready(&range); }
static hw_range_id_t range_id(void *user)
{ (void)user; return hw_range_get_current(&range); }
static hw_safety_range_state_t range_state(void *user)
{ (void)user; return hw_range_safety_state(&range); }
static bsp_status_t range_disable(void *user)
{ (void)user; return hw_range_force_disabled(&range); }
static void quiet(bool requested, void *user)
{ (void)user; hw_peripherals_request_quiet(requested); }
static void pause(void *user)
{ (void)user; hw_aux_sensors_pause(&sensors); }
static void resume(uint32_t now, void *user)
{ (void)user; hw_aux_sensors_resume(&sensors, now); }
static bsp_status_t adc_acquire(uint32_t now, void *user)
{ (void)user; return bsp_metrology_adc_acquire(now); }
static bsp_status_t adc_start(uint32_t *raw, uint32_t count, const hw_metrology_adc_profile_t *profile, void *user)
{ (void)user; return bsp_metrology_adc_start_capture(raw, count, profile->tim2_arr, profile->tim2_ccr2); }
static void adc_stop(void *user)
{ (void)user; bsp_metrology_adc_stop(); }
static bsp_status_t adc_restore(uint32_t now, void *user)
{ (void)user; return bsp_metrology_adc_restore(now); }
static bool dma_done(void *user)
{ (void)user; return bsp_metrology_adc_dma_complete(); }
static bool dma_error(void *user)
{ (void)user; return bsp_metrology_adc_dma_error(); }
static bsp_status_t exc_off(void *user)
{ (void)user; return bsp_excitation_off(); }
static bsp_status_t exc_neutral(void *user)
{ (void)user; return bsp_excitation_neutral(); }
static bsp_status_t exc_sine(hw_excitation_freq_t frequency, hw_excitation_amp_t amplitude, void *user)
{
    (void)user;
    hw_excitation_freq_profile_t profile;
    if (hw_excitation_freq_profile(frequency, &profile) != BSP_STATUS_OK ||
        hw_excitation_fill_ccr_table(sine_table, HW_EXCITATION_LUT_POINTS, amplitude) != BSP_STATUS_OK)
        return BSP_STATUS_INVALID_ARG;
    return bsp_excitation_sine(profile.rcr, sine_table, HW_EXCITATION_LUT_POINTS);
}
static hw_excitation_mode_t exc_mode(void *user)
{
    (void)user;
    switch (bsp_excitation_mode())
    {
    case BSP_EXCITATION_MODE_NEUTRAL: return HW_EXCITATION_MODE_NEUTRAL;
    case BSP_EXCITATION_MODE_SINE: return HW_EXCITATION_MODE_SINE;
    default: return HW_EXCITATION_MODE_OFF;
    }
}
static bool exc_error(void *user)
{ (void)user; return bsp_excitation_dma_error(); }
static hw_charger_state_t charger_state(void *user)
{ (void)user; return hw_charger_get_state(&charger); }
static uint32_t fault_mask(void *user)
{ (void)user; return app_safety_fault_mask(&faults); }
static bsp_status_t permit_issue(hw_measure_permit_issue_input_t *input, void *user)
{
    (void)user;
    if (input == NULL) return BSP_STATUS_INVALID_ARG;
    hw_aux_sensors_snapshot_t state;
    hw_aux_sensors_snapshot(&sensors, bsp_time_now_ms(), &state);
    *input = (hw_measure_permit_issue_input_t){.charger = charger_state(NULL),
        .residual = state.residual_state, .residual_age_ms = state.residual_age_ms,
        .battery = state.battery_state, .battery_age_ms = state.battery_age_ms,
        .range = range_state(NULL), .range_id = range_id(NULL), .k1_state = k1_state(NULL),
        .safety_fault_mask = fault_mask(NULL)};
    return BSP_STATUS_OK;
}
static bsp_status_t permit_validate(hw_measure_permit_validate_input_t *input, void *user)
{
    (void)user;
    if (input == NULL) return BSP_STATUS_INVALID_ARG;
    *input = (hw_measure_permit_validate_input_t){.charger = charger_state(NULL),
        .range = range_state(NULL), .range_id = range_id(NULL), .k1_state = k1_state(NULL),
        .safety_fault_mask = fault_mask(NULL)};
    return BSP_STATUS_OK;
}
static void fault_k1(void *user)
{ (void)user; app_safety_fault_latch(&faults, APP_SAFETY_FAULT_K1_IO); }
static void fault_range(void *user)
{ (void)user; app_safety_fault_latch(&faults, APP_SAFETY_FAULT_RANGE_IO); }
static void fault_adc(void *user)
{ (void)user; app_safety_fault_latch(&faults, APP_SAFETY_FAULT_ADC_RUNTIME); }
static void fault_measure(void *user)
{ (void)user; app_safety_fault_latch(&faults, APP_SAFETY_FAULT_METROLOGY_RUNTIME); }

static const hw_metrology_measure_io_t hardware_io = {
    .k1_force_safe = force_safe, .k1_request_measure = request_measure, .k1_commanded_state = k1_state,
    .range_request = range_request, .range_step = range_step, .range_is_ready = range_ready,
    .range_current_id = range_id, .range_safety_state = range_state, .range_force_disabled = range_disable,
    .quiet_request = quiet, .aux_pause = pause, .aux_resume = resume,
    .adc_acquire = adc_acquire, .adc_start_capture = adc_start, .adc_stop = adc_stop, .adc_restore = adc_restore,
    .adc_dma_complete = dma_done, .adc_dma_error = dma_error,
    .excitation_off = exc_off, .excitation_neutral = exc_neutral, .excitation_sine = exc_sine,
    .excitation_mode = exc_mode, .excitation_dma_error = exc_error,
    .charger_state = charger_state, .safety_fault_mask = fault_mask,
    .permit_issue_input = permit_issue, .permit_validate_input = permit_validate,
    .latch_k1_io_fault = fault_k1, .latch_range_io_fault = fault_range,
    .latch_adc_runtime_fault = fault_adc, .latch_metrology_runtime_fault = fault_measure,
};

static bsp_status_t capture_start(const hw_metrology_measure_request_t *request, uint32_t now, void *user)
{
    (void)user;
    const bsp_status_t owned = app_io_workspace_acquire(&workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
    if (owned != BSP_STATUS_OK) return owned;
    const bsp_status_t status = hw_metrology_measure_start(&measure, request, now);
    if (status != BSP_STATUS_OK && status != BSP_STATUS_BUSY)
        (void)app_io_workspace_release(&workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
    return status;
}
static bsp_status_t capture_step(uint32_t now, void *user)
{ (void)user; return hw_metrology_measure_step(&measure, now); }
static bool capture_active(void *user)
{ (void)user; return hw_metrology_measure_active(&measure); }
static bool capture_done(void *user)
{ (void)user; return hw_metrology_measure_state(&measure) == HW_METROLOGY_MEASURE_DONE; }
static bool capture_dumpable(void *user)
{ (void)user; return hw_metrology_measure_dumpable(&measure); }
static const hw_metrology_block_t *capture_block(void *user)
{ (void)user; return hw_metrology_measure_block(&measure); }
static hw_metrology_measure_error_t capture_error(void *user)
{ (void)user; return hw_metrology_measure_error(&measure); }
static void capture_ack(void *user)
{
    (void)user;
    hw_metrology_measure_acknowledge(&measure);
    if (app_io_workspace_owner(&workspace) == APP_IO_WORKSPACE_OWNER_METROLOGY)
        (void)app_io_workspace_release(&workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
}
static bsp_status_t capture_abort(void *user)
{ (void)user; return hw_metrology_measure_abort(&measure); }
static const app_cal_session_io_t capture_io = {
    .start_capture = capture_start, .step_capture = capture_step, .capture_active = capture_active,
    .capture_done = capture_done, .capture_dumpable = capture_dumpable, .capture_block = capture_block,
    .capture_error = capture_error, .capture_acknowledge = capture_ack, .capture_abort = capture_abort,
};

static void service_snapshot(app_cal_capture_snapshot_t *result, void *user)
{
    (void)user;
    hw_aux_sensors_snapshot_t state;
    hw_aux_sensors_snapshot(&sensors, bsp_time_now_ms(), &state);
    const hw_safety_input_t input = {.charger = charger_state(NULL), .residual = state.residual_state,
        .battery = state.battery_state, .range = range_state(NULL), .application_fault = fault_mask(NULL) != 0u};
    const hw_safety_result_t permission = hw_safety_evaluate(&input);
    *result = (app_cal_capture_snapshot_t){.safety_faults = fault_mask(NULL),
        .safety_blocks = permission.blocker_flags,
        .temperature_valid = state.ntc_temperature_valid,
        .temperature_mC = state.ntc_temperature_valid ? (int32_t)(state.ntc_temperature_c*1000.0f) : 0,
        /* Disabled range is expected before preflight; the FSM selects it before issuing a permit. */
        .factory_allowed = (permission.blocker_flags & ~(uint32_t)HW_SAFETY_BLOCK_RANGE) == 0u,
        .transfer_safe = k1_state(NULL) == HW_K1_STATE_SAFE && exc_mode(NULL) == HW_EXCITATION_MODE_OFF &&
            range_state(NULL) == HW_RANGE_DISABLED && !hw_peripherals_quiet_requested() && !capture_active(NULL)};
}
static bsp_status_t try_write(uint8_t byte, void *user)
{ (void)user; return bsp_uart_try_write_byte(byte); }

void app_shell_run(void)
{
    (void)bsp_reset_capture_reason();
    app_safety_fault_init(&faults);
    latch(bsp_gpio_init_safe(), APP_SAFETY_FAULT_GPIO_INIT);
    const bsp_status_t clock_status = bsp_clock_init();
    latch(clock_status, APP_SAFETY_FAULT_CLOCK);
    (void)bsp_time_init();
    (void)bsp_uart_init(115200u);
    (void)bsp_watchdog_start();
    const hw_k1_io_t k1_io = {.write_cmd = k1_write};
    const hw_k2_io_t k2_io = {.write_cmd = k2_write};
    const hw_range_io_t range_io = {.write_enable = range_enable, .write_address = range_address};
    const hw_charger_io_t charger_io = {.read_gpio = charger_read};
    const hw_aux_adc_io_t aux_io = {.start = aux_start, .poll = aux_poll, .cancel = aux_cancel};
    latch(hw_k1_init(&k1, &k1_io), APP_SAFETY_FAULT_K1_IO);
    latch(hw_k2_init(&k2, &k2_io), APP_SAFETY_FAULT_K2_IO);
    latch(hw_range_init(&range, &range_io), APP_SAFETY_FAULT_RANGE_IO);
    (void)hw_charger_init(&charger, &charger_io);
    latch(bsp_excitation_init(), APP_SAFETY_FAULT_METROLOGY_RUNTIME);
    latch(bsp_adc_init(bsp_time_now_ms()), APP_SAFETY_FAULT_ADC_INIT);
    latch(hw_aux_sensors_init(&sensors, &aux_io, bsp_time_now_ms()), APP_SAFETY_FAULT_ADC_INIT);
    (void)hw_backlight_init();
    (void)hw_backlight_set_percent(25u);
    (void)hw_buzzer_init();
    hw_buzzer_set_enabled(false);
    app_io_workspace_init(&workspace);
    app_calibration_service_init(&calibration);
    app_calibration_service_attach_workspace(&calibration, &workspace);
    (void)spi_bus_init();
    w25q_device_init(&flash);
    if (w25q_device_probe(&flash) == W25Q_STATUS_OK)
    {
        const measurement_cal_store_io_t io = measurement_cal_w25q_store_io(&flash);
        (void)app_calibration_service_load(&calibration, &io, flash.part.capacity_bytes);
    }
    else app_calibration_service_mark_storage_unavailable(&calibration);
    latch(hw_metrology_measure_init(&measure, &hardware_io,
        app_io_workspace_metrology_raw_words(&workspace), HW_METROLOGY_RAW_WORD_COUNT), APP_SAFETY_FAULT_METROLOGY_RUNTIME);
    app_cal_capture_io_t io = {.snapshot = service_snapshot, .try_write_byte = try_write};
    bsp_reset_read_device_uid(io.device_uid);
    latch(app_cal_capture_init(&capture, &calibration, &capture_io, &io,
        bsp_clock_get_summary(), clock_status), APP_SAFETY_FAULT_METROLOGY_RUNTIME);
    for (;;)
    {
        const uint32_t now = bsp_time_now_ms();
        (void)hw_charger_get_state(&charger);
        latch(hw_range_step(&range, now), APP_SAFETY_FAULT_RANGE_IO);
        latch(hw_aux_sensors_step(&sensors, now), APP_SAFETY_FAULT_ADC_RUNTIME);
        if (hw_aux_sensors_fault_mask(&sensors) != 0u) fault_adc(NULL);
        if (!hw_metrology_measure_k1_owned()) latch(hw_k1_force_safe(&k1), APP_SAFETY_FAULT_K1_IO);
        if (fault_mask(NULL) != 0u) latch(hw_range_force_disabled(&range), APP_SAFETY_FAULT_RANGE_IO);
        for (uint8_t n = 0u; n < 32u; n++)
        {
            uint8_t byte;
            if (bsp_uart_try_read_byte(&byte) != BSP_STATUS_OK) break;
            app_cal_capture_receive_byte(&capture, byte, now);
        }
        app_cal_capture_step(&capture, now);
        hw_buzzer_step(now);
        bsp_watchdog_service();
    }
}
