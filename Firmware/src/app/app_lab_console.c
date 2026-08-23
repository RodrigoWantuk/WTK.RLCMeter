#include "app/app_lab_console.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/app_measurement_session.h"
#include "app/app_safety_fault.h"
#include "bsp/bsp_adc.h"
#include "bsp/bsp_status.h"
#include "bsp/bsp_uart.h"
#include "drivers/ili9341.h"
#include "drivers/w25q.h"
#include "hardware/hw_aux_sensors.h"
#include "hardware/hw_charger.h"
#include "hardware/hw_buzzer.h"
#include "hardware/hw_k1.h"
#include "hardware/hw_measure_permit.h"
#include "hardware/hw_peripherals.h"
#include "hardware/hw_range.h"
#include "hardware/hw_excitation.h"
#include "hardware/hw_metrology_clock.h"
#include "hardware/hw_metrology_raw.h"
#include "hardware/hw_safety.h"
#include "measurement/measurement_calibration.h"
#include "measurement/measurement_calibration_store.h"
#include "measurement/measurement_dsp.h"
#include "measurement/measurement_engine.h"
#include "bsp/bsp_clock.h"
#include "bsp/bsp_time.h"
#include "bsp/bsp_excitation.h"
#include "bsp/bsp_metrology_adc.h"
#include "wtk_build_config.h"

#if WTK_ENABLE_LAB_DIAGNOSTICS

static void write_text(const char *text)
{
    (void)bsp_uart_write_cstr(text);
}

static void write_u32(uint32_t value)
{
    char buffer[10];
    uint8_t index = 0u;

    if (value == 0u)
    {
        write_text("0");
        return;
    }

    while ((value > 0u) && (index < (uint8_t)sizeof(buffer)))
    {
        buffer[index] = (char)('0' + (char)(value % 10u));
        value /= 10u;
        index++;
    }

    while (index > 0u)
    {
        index--;
        (void)bsp_uart_write(&buffer[index], 1u);
    }
}

static void write_i32(int32_t value)
{
    if (value < 0)
    {
        write_text("-");
        write_u32((uint32_t)(-value));
    }
    else
    {
        write_u32((uint32_t)value);
    }
}

static int32_t milli_from_float(float value)
{
    const float scaled = value * 1000.0f;
    return (int32_t)((scaled >= 0.0f) ? (scaled + 0.5f) : (scaled - 0.5f));
}

static void write_hex8(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    write_text("0x");
    for (int8_t shift = 28; shift >= 0; shift -= 4)
    {
        const uint32_t nibble = (value >> (uint32_t)shift) & 0xFu;
        (void)bsp_uart_write(&digits[nibble], 1u);
    }
}

static bool text_equals(const char *left, const char *right)
{
    size_t index = 0u;
    while ((left[index] != '\0') && (right[index] != '\0'))
    {
        if (left[index] != right[index])
        {
            return false;
        }
        index++;
    }

    return left[index] == right[index];
}

static bool text_starts_with(const char *text, const char *prefix)
{
    size_t index = 0u;
    while (prefix[index] != '\0')
    {
        if (text[index] != prefix[index])
        {
            return false;
        }
        index++;
    }

    return true;
}

static bool parse_u16(const char *text, uint16_t *value)
{
    uint32_t parsed = 0u;
    size_t index = 0u;

    if ((text == NULL) || (value == NULL) || (text[0] == '\0'))
    {
        return false;
    }

    while ((text[index] >= '0') && (text[index] <= '9'))
    {
        parsed = (parsed * 10u) + (uint32_t)(text[index] - '0');
        if (parsed > 65535u)
        {
            return false;
        }
        index++;
    }

    if (text[index] != '\0')
    {
        return false;
    }

    *value = (uint16_t)parsed;
    return true;
}

static void write_flash_info(const w25q_device_t *flash)
{
    if ((flash == NULL) || !flash->detected)
    {
        write_text("lab flash info: NOT_DETECTED\r\n");
        return;
    }

    write_text("lab flash info: ");
    write_text(flash->part.name);
    write_text(" ");
    write_hex8(((uint32_t)flash->part.jedec.manufacturer_id << 16u) |
               ((uint32_t)flash->part.jedec.memory_type << 8u) |
               (uint32_t)flash->part.jedec.capacity_code);
    write_text(" ");
    write_u32(flash->part.capacity_bytes);
    write_text("\r\n");
}

static void write_auto_policy(void)
{
    write_text("auto policy: INITIAL range=");
    write_text(hw_range_id_string(HW_RANGE_ID_1K));
    write_text(" freq=");
    write_text(hw_excitation_freq_token(HW_EXCITATION_FREQ_1KHZ));
    write_text(" amp=");
    write_text(hw_excitation_amp_token(HW_EXCITATION_AMP_100MVRMS));
    write_text("\r\n");
    write_text("auto policy: limits attempts=");
    write_u32(MEASUREMENT_AUTO_MAX_ATTEMPTS);
    write_text(" range_transitions=");
    write_u32(MEASUREMENT_AUTO_MAX_RANGE_TRANSITIONS);
    write_text(" frequency_refinements=");
    write_u32(MEASUREMENT_AUTO_MAX_FREQUENCY_REFINEMENTS);
    write_text("\r\n");
    write_text("auto policy: forbidden_10r_500mv=");
    write_text(measurement_auto_condition_allowed(HW_RANGE_ID_10R,
                                                 HW_EXCITATION_FREQ_1KHZ,
                                                 HW_EXCITATION_AMP_500MVRMS)
                   ? "0\r\n"
                   : "1\r\n");
    write_text("auto policy: unqualified_clean_confidence=");
    write_text(measurement_confidence_string(MEASUREMENT_CONFIDENCE_LOW_CONFIDENCE));
    write_text(" qualification=");
    write_text(measurement_qualification_string(MEASUREMENT_QUALIFICATION_UNQUALIFIED));
    write_text("\r\n");
}

static void write_calibration_status(const w25q_device_t *flash)
{
    write_text("CAL_SET schema=");
    write_u32(MEASUREMENT_CAL_SCHEMA_VERSION);
    write_text(" model=");
    write_u32(MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1);
    write_text(" hw=");
    write_hex8(MEASUREMENT_CAL_HARDWARE_REV1);
    write_text(" status=");
    write_text(measurement_cal_resolve_status_string(MEASUREMENT_CAL_RESOLVE_MISSING));
    write_text(" source=IDEAL uncalibrated=1\r\n");
    write_text("CAL_STORE frame_max=");
    write_u32(MEASUREMENT_CAL_MAX_FRAME_BYTES);
    write_text(" context=");
    write_u32(measurement_cal_store_context_size_bytes());
    write_text("\r\n");
    if ((flash != NULL) && flash->detected)
    {
        storage_partition_t cal_a;
        storage_partition_t cal_b;
        if (storage_layout_partition(flash->part.capacity_bytes, STORAGE_PARTITION_CALIBRATION_A, &cal_a) &&
            storage_layout_partition(flash->part.capacity_bytes, STORAGE_PARTITION_CALIBRATION_B, &cal_b))
        {
            write_text("CAL_SLOT A start=");
            write_hex8(cal_a.start);
            write_text(" size=");
            write_u32(cal_a.size);
            write_text("\r\nCAL_SLOT B start=");
            write_hex8(cal_b.start);
            write_text(" size=");
            write_u32(cal_b.size);
            write_text("\r\n");
            return;
        }
    }
    write_text("CAL_SLOT unavailable\r\n");
}

static bool parse_range_id(const char *line, hw_range_id_t *id)
{
    if ((line == NULL) || (id == NULL))
    {
        return false;
    }
    if (text_equals(line, "lab range 10r"))
    {
        *id = HW_RANGE_ID_10R;
        return true;
    }
    if (text_equals(line, "lab range 100r"))
    {
        *id = HW_RANGE_ID_100R;
        return true;
    }
    if (text_equals(line, "lab range 1k"))
    {
        *id = HW_RANGE_ID_1K;
        return true;
    }
    if (text_equals(line, "lab range 10k"))
    {
        *id = HW_RANGE_ID_10K;
        return true;
    }
    if (text_equals(line, "lab range 100k"))
    {
        *id = HW_RANGE_ID_100K;
        return true;
    }
    if (text_equals(line, "lab range 1m"))
    {
        *id = HW_RANGE_ID_1M;
        return true;
    }
    return false;
}

static void write_range_status(const hw_range_t *range)
{
    if (range == NULL)
    {
        write_text("lab range: INVALID\r\n");
        return;
    }

    write_text("lab range: ");
    write_text(hw_range_fsm_state_string(hw_range_get_state(range)));
    write_text(" requested=");
    write_text(hw_range_id_string(hw_range_get_requested(range)));
    write_text(" current=");
    write_text(hw_range_id_string(hw_range_get_current(range)));
    write_text("\r\n");
}

static void write_safety_status(const hw_safety_result_t *safety)
{
    if (safety == NULL)
    {
        write_text("lab safety: UNKNOWN\r\n");
        return;
    }

    write_text("lab safety: ");
    write_text(hw_safety_primary_blocker_string(safety->primary_blocker));
    write_text(" flags=");
    write_u32(safety->blocker_flags);
    write_text(" allowed=");
    write_text(safety->measure_allowed ? "1" : "0");
    write_text("\r\n");
}

static void write_sensor_mv(const char *name, float value)
{
    write_text(name);
    write_i32(milli_from_float(value));
    write_text("mV\r\n");
}

static void write_sensors_status(const hw_aux_sensors_t *sensors, uint32_t now_ms)
{
    if (sensors == NULL)
    {
        write_text("lab sensors: INVALID\r\n");
        return;
    }

    hw_aux_sensors_snapshot_t snapshot;
    hw_aux_sensors_snapshot(sensors, now_ms, &snapshot);

    write_text("lab sensors vmid_raw: ");
    write_u32(snapshot.vmid_raw);
    write_text(" valid=");
    write_text(snapshot.vmid_valid ? "1" : "0");
    write_text("\r\n");
    write_sensor_mv("lab sensors vmid: ", snapshot.vmid_v);

    write_text("lab sensors ov_raw: hi=");
    write_u32(snapshot.ov_hi_raw);
    write_text(" lo=");
    write_u32(snapshot.ov_lo_raw);
    write_text("\r\n");
    write_sensor_mv("lab sensors safe_hi: ", snapshot.safe_hi_v);
    write_sensor_mv("lab sensors safe_lo: ", snapshot.safe_lo_v);
    write_sensor_mv("lab sensors residual_diff: ", snapshot.residual_diff_v);
    write_text("lab sensors residual_state: ");
    write_text(hw_residual_state_string(snapshot.residual_state));
    write_text(" safe_count=");
    write_u32(snapshot.residual_safe_count);
    write_text(" age_ms=");
    write_u32(snapshot.residual_age_ms);
    write_text("\r\n");

    write_sensor_mv("lab sensors battery: ", snapshot.battery_v);
    write_text("lab sensors battery_state: ");
    write_text(hw_battery_state_string(snapshot.battery_state));
    write_text(" raw=");
    write_u32(snapshot.battery_raw);
    write_text(" age_ms=");
    write_u32(snapshot.battery_age_ms);
    write_text("\r\n");

    write_text("lab sensors ntc: raw=");
    write_u32(snapshot.ntc_raw);
    write_text(" mv=");
    write_i32(milli_from_float(snapshot.ntc_voltage_v));
    write_text(" resistance_ohm=");
    write_u32((uint32_t)((snapshot.ntc_resistance_ohm >= 0.0f) ? snapshot.ntc_resistance_ohm : 0.0f));
    write_text(" valid=");
    write_text(snapshot.ntc_valid ? "1" : "0");
    write_text(" ntc_temperature_mC=");
    write_i32(milli_from_float(snapshot.ntc_temperature_c));
    write_text(" ntc_temperature_valid=");
    write_text(snapshot.ntc_temperature_valid ? "1" : "0");
    write_text(" age_ms=");
    write_u32(snapshot.ntc_age_ms);
    write_text("\r\n");
}

static void write_permit_status(const hw_range_t *range,
                                hw_charger_t *charger,
                                const hw_aux_sensors_t *sensors,
                                const hw_k1_t *k1,
                                const app_safety_fault_latch_t *faults,
                                uint32_t now_ms)
{
    hw_aux_sensors_snapshot_t snapshot;
    if ((range == NULL) || (charger == NULL) || (sensors == NULL) || (k1 == NULL))
    {
        write_text("lab permit: eligible=0 reason=INVALID\r\n");
        return;
    }

    hw_aux_sensors_snapshot(sensors, now_ms, &snapshot);
    const hw_measure_permit_issue_input_t input = {
        .charger = hw_charger_get_state(charger),
        .residual = snapshot.residual_state,
        .residual_age_ms = snapshot.residual_age_ms,
        .battery = snapshot.battery_state,
        .battery_age_ms = snapshot.battery_age_ms,
        .range = hw_range_safety_state(range),
        .range_id = hw_range_get_current(range),
        .k1_state = hw_k1_commanded_state(k1),
        .safety_fault_mask = app_safety_fault_mask(faults),
    };
    const hw_measure_permit_issue_result_t result = hw_measure_permit_check_issue(&input);
    write_text("lab permit: eligible=");
    write_text(result.issued ? "1" : "0");
    write_text(" reason=");
    write_text(hw_measure_permit_rejection_string(result.reason));
    write_text("\r\n");
}

static void write_adc_status(const hw_aux_sensors_t *sensors)
{
    write_text("lab adc: ");
    if (sensors == NULL)
    {
        write_text("INVALID\r\n");
        return;
    }
    write_text(bsp_adc_state_string(bsp_adc_is_busy() ? BSP_ADC_STATE_BUSY : BSP_ADC_STATE_IDLE));
    write_text(" channel=");
    write_text(bsp_adc_channel_string(hw_aux_sensors_current_channel(sensors)));
    write_text(" last=");
    write_text(bsp_status_string(hw_aux_sensors_last_adc_status(sensors)));
    write_text("\r\n");
}

static void fill_pattern(uint8_t *pattern, size_t size)
{
    for (size_t i = 0u; i < size; i++)
    {
        pattern[i] = (uint8_t)(0xA5u ^ (uint8_t)(i * 17u));
    }
}

static void selftest_error(app_lab_console_t *console, const char *reason)
{
    console->flash_test_state = APP_LAB_FLASH_TEST_ERROR;
    write_text("lab flash selftest: FAIL ");
    write_text(reason);
    write_text("\r\n");
}

static void start_flash_selftest(app_lab_console_t *console, const w25q_device_t *flash)
{
    if ((console == NULL) || (flash == NULL) || !flash->detected)
    {
        write_text("lab flash selftest: FAIL NOT_DETECTED\r\n");
        return;
    }

    if (hw_peripherals_quiet_requested())
    {
        write_text("lab flash selftest: BUSY\r\n");
        return;
    }

    if ((console->flash_test_state != APP_LAB_FLASH_TEST_IDLE) &&
        (console->flash_test_state != APP_LAB_FLASH_TEST_COMPLETE) &&
        (console->flash_test_state != APP_LAB_FLASH_TEST_ERROR))
    {
        write_text("lab flash selftest: BUSY\r\n");
        return;
    }

    console->test_sector_address = w25q_reserved_test_sector_address(flash->part.capacity_bytes);
    fill_pattern(console->pattern, sizeof(console->pattern));
    console->busy_observed = false;
    console->flash_test_state = APP_LAB_FLASH_TEST_ERASE_START;
    write_text("lab flash selftest: START\r\n");
}

static void step_flash_selftest(app_lab_console_t *console, w25q_device_t *flash, uint32_t now_ms)
{
    if ((console == NULL) || (flash == NULL))
    {
        return;
    }

    switch (console->flash_test_state)
    {
    case APP_LAB_FLASH_TEST_IDLE:
    case APP_LAB_FLASH_TEST_COMPLETE:
    case APP_LAB_FLASH_TEST_ERROR:
        return;
    case APP_LAB_FLASH_TEST_ERASE_START:
        if (w25q_device_sector_erase_start(flash, console->test_sector_address, now_ms) != W25Q_STATUS_BUSY)
        {
            selftest_error(console, "ERASE_START");
            return;
        }
        console->flash_test_state = APP_LAB_FLASH_TEST_ERASE_WAIT;
        return;
    case APP_LAB_FLASH_TEST_ERASE_WAIT:
    case APP_LAB_FLASH_TEST_PROGRAM_WAIT:
    case APP_LAB_FLASH_TEST_CLEANUP_ERASE_WAIT:
    {
        const w25q_status_t status = w25q_device_poll(flash, now_ms);
        if (status == W25Q_STATUS_BUSY)
        {
            console->busy_observed = true;
            return;
        }
        if (status != W25Q_STATUS_OK)
        {
            selftest_error(console, "POLL");
            return;
        }
        if (!console->busy_observed)
        {
            selftest_error(console, "NO_BUSY");
            return;
        }
        console->busy_observed = false;
        if (console->flash_test_state == APP_LAB_FLASH_TEST_ERASE_WAIT)
        {
            console->flash_test_state = APP_LAB_FLASH_TEST_VERIFY_ERASE;
        }
        else if (console->flash_test_state == APP_LAB_FLASH_TEST_PROGRAM_WAIT)
        {
            console->flash_test_state = APP_LAB_FLASH_TEST_READBACK;
        }
        else
        {
            console->flash_test_state = APP_LAB_FLASH_TEST_COMPLETE;
            write_text("lab flash selftest: PASS\r\n");
        }
        return;
    }
    case APP_LAB_FLASH_TEST_VERIFY_ERASE:
        if (w25q_device_read(flash, console->test_sector_address, console->readback, sizeof(console->readback)) !=
            W25Q_STATUS_OK)
        {
            selftest_error(console, "READ_ERASE");
            return;
        }
        for (size_t i = 0u; i < sizeof(console->readback); i++)
        {
            if (console->readback[i] != 0xFFu)
            {
                selftest_error(console, "VERIFY_ERASE");
                return;
            }
        }
        console->flash_test_state = APP_LAB_FLASH_TEST_PROGRAM_START;
        return;
    case APP_LAB_FLASH_TEST_PROGRAM_START:
        if (w25q_device_page_program_start(flash,
                                           console->test_sector_address,
                                           console->pattern,
                                           sizeof(console->pattern),
                                           now_ms) != W25Q_STATUS_BUSY)
        {
            selftest_error(console, "PROGRAM_START");
            return;
        }
        console->flash_test_state = APP_LAB_FLASH_TEST_PROGRAM_WAIT;
        return;
    case APP_LAB_FLASH_TEST_READBACK:
        if (w25q_device_read(flash, console->test_sector_address, console->readback, sizeof(console->readback)) !=
            W25Q_STATUS_OK)
        {
            selftest_error(console, "READBACK");
            return;
        }
        console->flash_test_state = APP_LAB_FLASH_TEST_VERIFY;
        return;
    case APP_LAB_FLASH_TEST_VERIFY:
        for (size_t i = 0u; i < sizeof(console->pattern); i++)
        {
            if (console->readback[i] != console->pattern[i])
            {
                selftest_error(console, "VERIFY_PROGRAM");
                return;
            }
        }
        console->flash_test_state = APP_LAB_FLASH_TEST_CLEANUP_ERASE_START;
        return;
    case APP_LAB_FLASH_TEST_CLEANUP_ERASE_START:
        if (w25q_device_sector_erase_start(flash, console->test_sector_address, now_ms) != W25Q_STATUS_BUSY)
        {
            selftest_error(console, "CLEANUP_START");
            return;
        }
        console->flash_test_state = APP_LAB_FLASH_TEST_CLEANUP_ERASE_WAIT;
        return;
    default:
        selftest_error(console, "STATE");
        return;
    }
}

static hw_excitation_mode_t lab_bsp_excitation_mode(void)
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

static bsp_status_t lab_k1_force_safe(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if ((console == NULL) || (console->k1_ref == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    return hw_k1_force_safe(console->k1_ref);
}

static hw_k1_state_t lab_k1_commanded_state(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if ((console == NULL) || (console->k1_ref == NULL))
    {
        return HW_K1_STATE_UNKNOWN;
    }
    return hw_k1_commanded_state(console->k1_ref);
}

static bsp_status_t lab_range_request(hw_range_id_t id, uint32_t now_ms, void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if (console->range_ref == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    return hw_range_request(console->range_ref, id, now_ms);
}

static bsp_status_t lab_range_step(uint32_t now_ms, void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if (console->range_ref == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    return hw_range_step(console->range_ref, now_ms);
}

static bool lab_range_is_ready(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    return (console->range_ref != NULL) && hw_range_is_ready(console->range_ref);
}

static bsp_status_t lab_range_force_disabled(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if (console->range_ref == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    return hw_range_force_disabled(console->range_ref);
}

static void lab_quiet_request(bool requested, void *user)
{
    (void)user;
    hw_peripherals_request_quiet(requested);
}

static void lab_aux_pause(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if (console->sensors_ref != NULL)
    {
        hw_aux_sensors_pause(console->sensors_ref);
    }
}

static void lab_aux_resume(uint32_t now_ms, void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if (console->sensors_ref != NULL)
    {
        hw_aux_sensors_resume(console->sensors_ref, now_ms);
    }
}

static bsp_status_t lab_adc_acquire(uint32_t now_ms, void *user)
{
    (void)user;
    return bsp_metrology_adc_acquire(now_ms);
}

static bsp_status_t lab_adc_start_capture(uint32_t *raw_words,
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

static void lab_adc_stop(void *user)
{
    (void)user;
    bsp_metrology_adc_stop();
}

static bsp_status_t lab_adc_restore(uint32_t now_ms, void *user)
{
    (void)user;
    return bsp_metrology_adc_restore(now_ms);
}

static bool lab_adc_dma_complete(void *user)
{
    (void)user;
    return bsp_metrology_adc_dma_complete();
}

static bool lab_adc_dma_error(void *user)
{
    (void)user;
    return bsp_metrology_adc_dma_error();
}

static bsp_status_t lab_excitation_off(void *user)
{
    (void)user;
    return bsp_excitation_off();
}

static bsp_status_t lab_excitation_neutral(void *user)
{
    (void)user;
    return bsp_excitation_neutral();
}

static bsp_status_t lab_excitation_sine(hw_excitation_freq_t frequency,
                                        hw_excitation_amp_t amplitude,
                                        void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    hw_excitation_freq_profile_t profile;
    if (hw_excitation_freq_profile(frequency, &profile) != BSP_STATUS_OK)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (hw_excitation_fill_ccr_table(console->ccr_table, HW_EXCITATION_LUT_POINTS, amplitude) != BSP_STATUS_OK)
    {
        return BSP_STATUS_ERROR;
    }
    return bsp_excitation_sine(profile.rcr, console->ccr_table, HW_EXCITATION_LUT_POINTS);
}

static hw_excitation_mode_t lab_excitation_mode(void *user)
{
    (void)user;
    return lab_bsp_excitation_mode();
}

static bool lab_excitation_dma_error(void *user)
{
    (void)user;
    return bsp_excitation_dma_error();
}

static hw_charger_state_t lab_charger_state(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if (console->charger_ref == NULL)
    {
        return HW_CHARGER_UNKNOWN;
    }
    return hw_charger_get_state(console->charger_ref);
}

static void lab_latch_adc_runtime_fault(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if (console->faults_ref != NULL)
    {
        app_safety_fault_latch(console->faults_ref, APP_SAFETY_FAULT_ADC_RUNTIME);
    }
}

static void lab_latch_k1_io_fault(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if (console->faults_ref != NULL)
    {
        app_safety_fault_latch(console->faults_ref, APP_SAFETY_FAULT_K1_IO);
    }
}

static void lab_latch_range_io_fault(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if (console->faults_ref != NULL)
    {
        app_safety_fault_latch(console->faults_ref, APP_SAFETY_FAULT_RANGE_IO);
    }
}

static void lab_latch_metrology_runtime_fault(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if (console->faults_ref != NULL)
    {
        app_safety_fault_latch(console->faults_ref, APP_SAFETY_FAULT_METROLOGY_RUNTIME);
    }
}

static bsp_status_t lab_k1_request_measure(const hw_safety_result_t *permission, void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if ((console == NULL) || (console->k1_ref == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    return hw_k1_request_measure(console->k1_ref, permission);
}

static hw_range_id_t lab_range_current_id(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if ((console == NULL) || (console->range_ref == NULL))
    {
        return HW_RANGE_ID_INVALID;
    }
    return hw_range_get_current(console->range_ref);
}

static hw_safety_range_state_t lab_range_safety_state(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if ((console == NULL) || (console->range_ref == NULL))
    {
        return HW_RANGE_INVALID;
    }
    return hw_range_safety_state(console->range_ref);
}

static uint32_t lab_safety_fault_mask(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if ((console == NULL) || (console->faults_ref == NULL))
    {
        return 0u;
    }
    return app_safety_fault_mask(console->faults_ref);
}

static bsp_status_t lab_permit_issue_input(hw_measure_permit_issue_input_t *input, void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if ((console == NULL) || (input == NULL) || (console->range_ref == NULL) ||
        (console->charger_ref == NULL) || (console->sensors_ref == NULL) || (console->k1_ref == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    hw_aux_sensors_snapshot_t snapshot;
    const uint32_t now_ms = bsp_time_now_ms();
    hw_aux_sensors_snapshot(console->sensors_ref, now_ms, &snapshot);
    input->charger = hw_charger_get_state(console->charger_ref);
    input->residual = snapshot.residual_state;
    input->residual_age_ms = snapshot.residual_age_ms;
    input->battery = snapshot.battery_state;
    input->battery_age_ms = snapshot.battery_age_ms;
    input->range = hw_range_safety_state(console->range_ref);
    input->range_id = hw_range_get_current(console->range_ref);
    input->k1_state = hw_k1_commanded_state(console->k1_ref);
    input->safety_fault_mask = lab_safety_fault_mask(user);
    return BSP_STATUS_OK;
}

static bsp_status_t lab_permit_validate_input(hw_measure_permit_validate_input_t *input, void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if ((console == NULL) || (input == NULL) || (console->range_ref == NULL) ||
        (console->charger_ref == NULL) || (console->k1_ref == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    input->charger = hw_charger_get_state(console->charger_ref);
    input->range = hw_range_safety_state(console->range_ref);
    input->range_id = hw_range_get_current(console->range_ref);
    input->k1_state = hw_k1_commanded_state(console->k1_ref);
    input->safety_fault_mask = lab_safety_fault_mask(user);
    return BSP_STATUS_OK;
}

static void lab_bind_refs(app_lab_console_t *console,
                          hw_range_t *range,
                          hw_charger_t *charger,
                          hw_aux_sensors_t *sensors,
                          hw_k1_t *k1,
                          app_safety_fault_latch_t *faults)
{
    console->range_ref = range;
    console->charger_ref = charger;
    console->sensors_ref = sensors;
    console->k1_ref = k1;
    console->faults_ref = faults;
}

static bsp_status_t lab_init_metrology_session(app_lab_console_t *console)
{
    const hw_metrology_session_io_t io = {
        .k1_force_safe = lab_k1_force_safe,
        .k1_commanded_state = lab_k1_commanded_state,
        .range_request = lab_range_request,
        .range_step = lab_range_step,
        .range_is_ready = lab_range_is_ready,
        .range_force_disabled = lab_range_force_disabled,
        .quiet_request = lab_quiet_request,
        .aux_pause = lab_aux_pause,
        .aux_resume = lab_aux_resume,
        .adc_acquire = lab_adc_acquire,
        .adc_start_capture = lab_adc_start_capture,
        .adc_stop = lab_adc_stop,
        .adc_restore = lab_adc_restore,
        .adc_dma_complete = lab_adc_dma_complete,
        .adc_dma_error = lab_adc_dma_error,
        .excitation_off = lab_excitation_off,
        .excitation_neutral = lab_excitation_neutral,
        .excitation_sine = lab_excitation_sine,
        .excitation_mode = lab_excitation_mode,
        .excitation_dma_error = lab_excitation_dma_error,
        .charger_state = lab_charger_state,
        .latch_k1_io_fault = lab_latch_k1_io_fault,
        .latch_range_io_fault = lab_latch_range_io_fault,
        .latch_adc_runtime_fault = lab_latch_adc_runtime_fault,
        .latch_metrology_runtime_fault = lab_latch_metrology_runtime_fault,
        .user = console,
    };
    return hw_metrology_session_init(&console->session,
                                     &io,
                                     bsp_metrology_adc_raw_words(),
                                     HW_METROLOGY_RAW_WORD_COUNT);
}

static bsp_status_t lab_init_metrology_measure(app_lab_console_t *console)
{
    const hw_metrology_measure_io_t io = {
        .k1_force_safe = lab_k1_force_safe,
        .k1_request_measure = lab_k1_request_measure,
        .k1_commanded_state = lab_k1_commanded_state,
        .range_request = lab_range_request,
        .range_step = lab_range_step,
        .range_is_ready = lab_range_is_ready,
        .range_current_id = lab_range_current_id,
        .range_safety_state = lab_range_safety_state,
        .range_force_disabled = lab_range_force_disabled,
        .quiet_request = lab_quiet_request,
        .aux_pause = lab_aux_pause,
        .aux_resume = lab_aux_resume,
        .adc_acquire = lab_adc_acquire,
        .adc_start_capture = lab_adc_start_capture,
        .adc_stop = lab_adc_stop,
        .adc_restore = lab_adc_restore,
        .adc_dma_complete = lab_adc_dma_complete,
        .adc_dma_error = lab_adc_dma_error,
        .excitation_off = lab_excitation_off,
        .excitation_neutral = lab_excitation_neutral,
        .excitation_sine = lab_excitation_sine,
        .excitation_mode = lab_excitation_mode,
        .excitation_dma_error = lab_excitation_dma_error,
        .charger_state = lab_charger_state,
        .safety_fault_mask = lab_safety_fault_mask,
        .permit_issue_input = lab_permit_issue_input,
        .permit_validate_input = lab_permit_validate_input,
        .latch_k1_io_fault = lab_latch_k1_io_fault,
        .latch_range_io_fault = lab_latch_range_io_fault,
        .latch_adc_runtime_fault = lab_latch_adc_runtime_fault,
        .latch_metrology_runtime_fault = lab_latch_metrology_runtime_fault,
        .user = console,
    };
    return hw_metrology_measure_init(&console->measure,
                                     &io,
                                     bsp_metrology_adc_raw_words(),
                                     HW_METROLOGY_RAW_WORD_COUNT);
}

static bsp_status_t lab_auto_start_attempt(const hw_metrology_measure_request_t *request,
                                           uint32_t now_ms,
                                           void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    return (console == NULL) ? BSP_STATUS_INVALID_ARG :
                               hw_metrology_measure_start(&console->measure, request, now_ms);
}

static bsp_status_t lab_auto_step_attempt(uint32_t now_ms, void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    return (console == NULL) ? BSP_STATUS_INVALID_ARG :
                               hw_metrology_measure_step(&console->measure, now_ms);
}

static bool lab_auto_attempt_active(void *user)
{
    const app_lab_console_t *console = (const app_lab_console_t *)user;
    return (console != NULL) && hw_metrology_measure_active(&console->measure);
}

static bool lab_auto_attempt_done(void *user)
{
    const app_lab_console_t *console = (const app_lab_console_t *)user;
    return (console != NULL) &&
           (hw_metrology_measure_state(&console->measure) == HW_METROLOGY_MEASURE_DONE);
}

static bool lab_auto_attempt_dumpable(void *user)
{
    const app_lab_console_t *console = (const app_lab_console_t *)user;
    return (console != NULL) && hw_metrology_measure_dumpable(&console->measure);
}

static const hw_metrology_block_t *lab_auto_attempt_block(void *user)
{
    const app_lab_console_t *console = (const app_lab_console_t *)user;
    return (console == NULL) ? NULL : hw_metrology_measure_block(&console->measure);
}

static hw_metrology_measure_error_t lab_auto_attempt_error(void *user)
{
    const app_lab_console_t *console = (const app_lab_console_t *)user;
    return (console == NULL) ? HW_METROLOGY_MEASURE_ERR_INVALID :
                               hw_metrology_measure_error(&console->measure);
}

static void lab_auto_attempt_acknowledge(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    if (console != NULL)
    {
        hw_metrology_measure_acknowledge(&console->measure);
    }
}

static bsp_status_t lab_auto_attempt_abort(void *user)
{
    app_lab_console_t *console = (app_lab_console_t *)user;
    return (console == NULL) ? BSP_STATUS_INVALID_ARG :
                               hw_metrology_measure_abort(&console->measure);
}

static bsp_status_t lab_auto_process_block(const hw_metrology_block_t *block,
                                           const measurement_attempt_config_t *attempt,
                                           measurement_result_t *result,
                                           void *user)
{
    (void)user;
    if ((block == NULL) || (attempt == NULL) || (result == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    measurement_adc_calibration_t adc;
    measurement_dsp_config_t config;
    measurement_calibration_provenance_t provenance;
    const measurement_cal_key_t key = measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                                                          MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1,
                                                          attempt->range_id,
                                                          attempt->frequency,
                                                          attempt->amplitude,
                                                          MEASUREMENT_RETURN_1X,
                                                          (uint8_t)attempt->ret_strategy);
    (void)measurement_cal_resolve(NULL, &key, true, &adc, &config, &provenance);
    return measurement_process_block(block, &adc, &config, result);
}

static bsp_status_t lab_init_auto_measure(app_lab_console_t *console)
{
    const app_measurement_session_io_t io = {
        .start_attempt = lab_auto_start_attempt,
        .step_attempt = lab_auto_step_attempt,
        .attempt_active = lab_auto_attempt_active,
        .attempt_done = lab_auto_attempt_done,
        .attempt_dumpable = lab_auto_attempt_dumpable,
        .attempt_block = lab_auto_attempt_block,
        .attempt_error = lab_auto_attempt_error,
        .attempt_acknowledge = lab_auto_attempt_acknowledge,
        .attempt_abort = lab_auto_attempt_abort,
        .process_block = lab_auto_process_block,
        .user = console,
    };
    return app_measurement_session_init(&console->auto_measure, &io);
}

static const char *lab_range_dump_token(hw_range_id_t id)
{
    switch (id)
    {
    case HW_RANGE_ID_10R:
        return "10r";
    case HW_RANGE_ID_100R:
        return "100r";
    case HW_RANGE_ID_1K:
        return "1k";
    case HW_RANGE_ID_10K:
        return "10k";
    case HW_RANGE_ID_100K:
        return "100k";
    case HW_RANGE_ID_1M:
        return "1m";
    case HW_RANGE_ID_INVALID:
    default:
        return "invalid";
    }
}

static bool parse_capture_tokens(const char *line,
                                 hw_excitation_freq_t *frequency,
                                 hw_excitation_amp_t *amplitude,
                                 hw_range_id_t *range_id)
{
    if (!text_starts_with(line, "lab metrology capture "))
    {
        return false;
    }

    const char *cursor = line + 22;
    char freq_token[8] = {0};
    char amp_token[8] = {0};
    char range_token[8] = {0};
    size_t index = 0u;

    while ((*cursor != '\0') && (*cursor != ' ') && (index < (sizeof(freq_token) - 1u)))
    {
        freq_token[index++] = *cursor++;
    }
    if (*cursor != ' ')
    {
        return false;
    }
    cursor++;
    index = 0u;
    while ((*cursor != '\0') && (*cursor != ' ') && (index < (sizeof(amp_token) - 1u)))
    {
        amp_token[index++] = *cursor++;
    }
    if (*cursor != ' ')
    {
        return false;
    }
    cursor++;
    index = 0u;
    while ((*cursor != '\0') && (*cursor != ' ') && (index < (sizeof(range_token) - 1u)))
    {
        range_token[index++] = *cursor++;
    }
    if (*cursor != '\0')
    {
        return false;
    }

    return hw_excitation_parse_freq_token(freq_token, frequency) &&
           hw_excitation_parse_amp_token(amp_token, amplitude) &&
           hw_excitation_parse_range_token(range_token, range_id);
}

static bool parse_measure_tokens(const char *line,
                                 hw_excitation_freq_t *frequency,
                                 hw_excitation_amp_t *amplitude,
                                 hw_range_id_t *range_id)
{
    if (!text_starts_with(line, "lab metrology measure "))
    {
        return false;
    }

    const char *cursor = line + 22;
    char freq_token[8] = {0};
    char amp_token[8] = {0};
    char range_token[8] = {0};
    size_t index = 0u;

    while ((*cursor != '\0') && (*cursor != ' ') && (index < (sizeof(freq_token) - 1u)))
    {
        freq_token[index++] = *cursor++;
    }
    if (*cursor != ' ')
    {
        return false;
    }
    cursor++;
    index = 0u;
    while ((*cursor != '\0') && (*cursor != ' ') && (index < (sizeof(amp_token) - 1u)))
    {
        amp_token[index++] = *cursor++;
    }
    if (*cursor != ' ')
    {
        return false;
    }
    cursor++;
    index = 0u;
    while ((*cursor != '\0') && (*cursor != ' ') && (index < (sizeof(range_token) - 1u)))
    {
        range_token[index++] = *cursor++;
    }
    if (*cursor != '\0')
    {
        return false;
    }

    return hw_excitation_parse_freq_token(freq_token, frequency) &&
           hw_excitation_parse_amp_token(amp_token, amplitude) &&
           hw_excitation_parse_range_token(range_token, range_id);
}

static bool lab_metrology_busy(const app_lab_console_t *console)
{
    return hw_metrology_session_active(&console->session) ||
           hw_metrology_measure_active(&console->measure) ||
           app_measurement_session_active(&console->auto_measure) ||
           console->dump_active ||
           (hw_metrology_session_state(&console->session) == HW_METROLOGY_SESSION_DONE) ||
           (hw_metrology_measure_state(&console->measure) == HW_METROLOGY_MEASURE_DONE);
}

static void lab_write_attempt_config(const measurement_attempt_config_t *attempt)
{
    if (attempt == NULL)
    {
        write_text("number=0\r\n");
        return;
    }
    write_text("number=");
    write_u32(attempt->attempt_number);
    write_text("\r\nreason=");
    write_text(measurement_attempt_reason_string(attempt->reason));
    write_text("\r\nrange=");
    write_text(lab_range_dump_token(attempt->range_id));
    write_text("\r\nfrequency=");
    write_text(hw_excitation_freq_token(attempt->frequency));
    write_text("\r\namplitude=");
    write_text(hw_excitation_amp_token(attempt->amplitude));
    write_text("\r\nret_strategy=");
    write_text(measurement_ret_strategy_string(attempt->ret_strategy));
    write_text("\r\n");
}

static void lab_write_auto_result(const char *prefix, const measurement_session_result_t *result)
{
    write_text(prefix);
    write_text("\r\n");
    if (result == NULL)
    {
        write_text("status=FAILED\r\n");
        return;
    }
    write_text("status=");
    write_text(measurement_auto_status_string(result->status));
    write_text("\r\nprimary_attempt=");
    write_u32((result->primary_attempt_index == MEASUREMENT_AUTO_INDEX_NONE) ?
                  0u :
                  result->primary_attempt.config.attempt_number);
    write_text("\r\nattempts=");
    write_u32(result->attempt_count);
    write_text("\r\nclassification=");
    write_text(measurement_interpretation_string(result->classification.interpretation));
    write_text("\r\nquality=");
    write_text(measurement_quality_string(result->confidence.measurement_quality));
    write_text("\r\nqualification=");
    write_text(measurement_qualification_string(result->confidence.qualification));
    write_text("\r\nconfidence=");
    write_text(measurement_confidence_string(result->confidence.publication_confidence));
    write_text("\r\nreasons=");
    write_hex8(result->confidence.reason_flags | result->classification.reason_flags);
    write_text("\r\nnext_reason=");
    write_text(measurement_attempt_reason_string(result->continuation_reason));
    write_text("\r\n");
}

static void lab_start_auto_measure(app_lab_console_t *console, uint32_t now_ms)
{
    if (lab_metrology_busy(console))
    {
        write_text("lab auto: BUSY\r\n");
        return;
    }
    const bsp_clock_summary_t *clock = bsp_clock_get_summary();
    const bsp_status_t clock_status =
        hw_metrology_clock_ready(clock, BSP_STATUS_OK) ? BSP_STATUS_OK : BSP_STATUS_ERROR;
    const measurement_auto_hint_t *hint = console->auto_hint.valid ? &console->auto_hint : NULL;
    const uint32_t sequence = console->auto_sequence + 1u;
    const bsp_status_t status = app_measurement_session_start(&console->auto_measure,
                                                              MEASUREMENT_AUTO_MODE_CLICK,
                                                              sequence,
                                                              MEASUREMENT_QUALIFICATION_UNQUALIFIED,
                                                              hint,
                                                              clock,
                                                              clock_status,
                                                              now_ms);
    if (status == BSP_STATUS_BUSY)
    {
        console->auto_sequence = sequence;
        return;
    }
    write_text("lab auto: ERROR\r\n");
}

static void lab_start_metrology_capture(app_lab_console_t *console,
                                        hw_excitation_freq_t frequency,
                                        hw_excitation_amp_t amplitude,
                                        hw_range_id_t range_id,
                                        uint32_t now_ms)
{
    if (lab_metrology_busy(console))
    {
        write_text("lab metrology: BUSY\r\n");
        return;
    }

    const bsp_clock_summary_t *clock = bsp_clock_get_summary();
    const bsp_status_t clock_status =
        hw_metrology_clock_ready(clock, BSP_STATUS_OK) ? BSP_STATUS_OK : BSP_STATUS_ERROR;
    const hw_metrology_session_request_t request = {
        .clock_summary = clock,
        .clock_init_status = clock_status,
        .frequency = frequency,
        .amplitude = amplitude,
        .range_id = range_id,
    };

    const bsp_status_t status = hw_metrology_session_start(&console->session, &request, now_ms);
    if (status == BSP_STATUS_BUSY)
    {
        write_text("lab metrology: START\r\n");
        return;
    }
    if (status == BSP_STATUS_NOT_SUPPORTED)
    {
        write_text("lab metrology: FORBIDDEN_AMPLITUDE\r\n");
        return;
    }
    if (status == BSP_STATUS_ERROR)
    {
        write_text("lab metrology: CLOCK_NOT_READY\r\n");
        return;
    }
    write_text("lab metrology: INVALID_ARG\r\n");
}

static void lab_start_metrology_measure(app_lab_console_t *console,
                                        hw_excitation_freq_t frequency,
                                        hw_excitation_amp_t amplitude,
                                        hw_range_id_t range_id,
                                        uint32_t now_ms)
{
    if (lab_metrology_busy(console))
    {
        write_text("lab metrology: BUSY\r\n");
        return;
    }

    const bsp_clock_summary_t *clock = bsp_clock_get_summary();
    const bsp_status_t clock_status =
        hw_metrology_clock_ready(clock, BSP_STATUS_OK) ? BSP_STATUS_OK : BSP_STATUS_ERROR;
    const hw_metrology_measure_request_t request = {
        .clock_summary = clock,
        .clock_init_status = clock_status,
        .frequency = frequency,
        .amplitude = amplitude,
        .range_id = range_id,
    };

    const bsp_status_t status = hw_metrology_measure_start(&console->measure, &request, now_ms);
    if (status == BSP_STATUS_BUSY)
    {
        write_text("lab metrology: START\r\n");
        return;
    }
    if (status == BSP_STATUS_NOT_SUPPORTED)
    {
        write_text("lab metrology: FORBIDDEN_AMPLITUDE\r\n");
        return;
    }
    if (status == BSP_STATUS_ERROR)
    {
        write_text("lab metrology: CLOCK_NOT_READY\r\n");
        return;
    }
    write_text("lab metrology: INVALID_ARG\r\n");
}

static const char *lab_metrology_mode_string(hw_metrology_mode_t mode)
{
    switch (mode)
    {
    case HW_METROLOGY_MODE_DUT_MEASURE:
        return "DUT_MEASURE";
    case HW_METROLOGY_MODE_CAPTURE:
    default:
        return "CAPTURE";
    }
}

static hw_excitation_freq_t lab_frequency_from_hz(uint32_t frequency_hz)
{
    switch (frequency_hz)
    {
    case 100u:
        return HW_EXCITATION_FREQ_100HZ;
    case 1000u:
        return HW_EXCITATION_FREQ_1KHZ;
    case 10000u:
        return HW_EXCITATION_FREQ_10KHZ;
    default:
        return HW_EXCITATION_FREQ_INVALID;
    }
}

static void lab_dump_metrology_dsp_summary(const hw_metrology_block_t *block)
{
    measurement_adc_calibration_t adc;
    measurement_dsp_config_t config;
    measurement_calibration_provenance_t provenance;
    measurement_result_t result;
    const measurement_cal_key_t key = measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                                                          MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1,
                                                          block->range_id,
                                                          lab_frequency_from_hz(block->excitation_frequency_hz),
                                                          HW_EXCITATION_AMP_100MVRMS,
                                                          MEASUREMENT_RETURN_1X,
                                                          (uint8_t)MEASUREMENT_RET_STRATEGY_DSP_AUTO);
    (void)measurement_cal_resolve(NULL, &key, true, &adc, &config, &provenance);

    write_text("DSP_BEGIN v=1\r\n");
    write_text("calibration=");
    write_text(measurement_cal_resolve_status_string(provenance.status));
    write_text("\r\n");
    if (measurement_process_block(block, &adc, &config, &result) != BSP_STATUS_OK)
    {
        write_text("dsp_status=");
        write_text(measurement_status_string(result.status));
        write_text("\r\nDSP_END\r\n");
        return;
    }

    write_text("dsp_status=");
    write_text(measurement_status_string(result.status));
    write_text("\r\nreturn_channel=");
    write_text((result.selected_channel == MEASUREMENT_RETURN_HG) ? "RET_HG" : "RET_1X");
    write_text("\r\nz_real_mohm=");
    write_i32(milli_from_float(result.impedance.z_ohms.re));
    write_text("\r\nz_imag_mohm=");
    write_i32(milli_from_float(result.impedance.z_ohms.im));
    write_text("\r\nz_mag_mohm=");
    write_i32(milli_from_float(result.derived.magnitude_ohms));
    write_text("\r\nphase_mrad=");
    write_i32(milli_from_float(result.derived.phase_rad));
    write_text("\r\ninterpretation=");
    write_text(measurement_interpretation_string(result.derived.interpretation));
    write_text("\r\nDSP_END\r\n");
}

static void lab_dump_metrology_header(const hw_metrology_block_t *block)
{
    write_text("METROLOGY_RAW_BEGIN v=1\r\n");
    write_text("mode=");
    write_text(lab_metrology_mode_string(block->mode));
    write_text("\r\nfrequency_hz=");
    write_u32(block->excitation_frequency_hz);
    write_text("\r\namplitude_mvrms=");
    write_u32(block->requested_amplitude_mvrms);
    write_text("\r\nrange=");
    write_text(lab_range_dump_token(block->range_id));
    write_text("\r\nsample_rate_hz=");
    write_u32(block->sample_rate_hz);
    write_text("\r\nsamples=");
    write_u32(block->sample_count);
    write_text("\r\nwords_per_sample=");
    write_u32(block->words_per_sample);
    if (block->dut_measure)
    {
        write_text("\r\npermit_issue_ms=");
        write_u32(block->permit_issue_ms);
        write_text("\r\npermit_validate_ms=");
        write_u32(block->permit_validate_ms);
        write_text("\r\nk1_operate_guard_ms=");
        write_u32(block->k1_operate_guard_ms);
        write_text("\r\nk1_release_guard_ms=");
        write_u32(block->k1_release_guard_ms);
    }
    write_text("\r\n");
    lab_dump_metrology_dsp_summary(block);
    write_text("index,vexc1,ret1x,vexc2,rethg,vmid_adc1,vmid_adc2\r\n");
}

static void lab_dump_raw_row(const hw_metrology_block_t *block, uint16_t row)
{
    hw_metrology_sample_t sample;
    if (hw_metrology_unpack_sample(block->raw_words, row, &sample) != BSP_STATUS_OK)
    {
        return;
    }

    write_u32(row);
    write_text(",");
    write_u32(sample.vexc_1);
    write_text(",");
    write_u32(sample.ret_1x);
    write_text(",");
    write_u32(sample.vexc_2);
    write_text(",");
    write_u32(sample.ret_hg);
    write_text(",");
    write_u32(sample.vmid_adc1);
    write_text(",");
    write_u32(sample.vmid_adc2);
    write_text("\r\n");
}

static const hw_metrology_block_t *lab_active_dump_block(const app_lab_console_t *console)
{
    if (console->dump_source == APP_LAB_METROLOGY_DUMP_MEASURE)
    {
        return hw_metrology_measure_block(&console->measure);
    }
    if (console->dump_source == APP_LAB_METROLOGY_DUMP_CAPTURE)
    {
        return hw_metrology_session_block(&console->session);
    }
    return NULL;
}

static void lab_metrology_dump_acknowledge(app_lab_console_t *console)
{
    if (console->dump_source == APP_LAB_METROLOGY_DUMP_MEASURE)
    {
        hw_metrology_measure_acknowledge(&console->measure);
    }
    else if (console->dump_source == APP_LAB_METROLOGY_DUMP_CAPTURE)
    {
        hw_metrology_session_acknowledge(&console->session);
    }
    console->dump_source = APP_LAB_METROLOGY_DUMP_NONE;
    console->dump_active = false;
}

static void lab_step_metrology_dump(app_lab_console_t *console)
{
    if (!console->dump_active)
    {
        return;
    }

    const hw_metrology_block_t *block = lab_active_dump_block(console);
    if (block == NULL)
    {
        lab_metrology_dump_acknowledge(console);
        return;
    }

    for (uint8_t burst = 0u; burst < 16u; burst++)
    {
        if (console->dump_row >= block->sample_count)
        {
            write_text("METROLOGY_RAW_END status=OK\r\n");
            lab_metrology_dump_acknowledge(console);
            return;
        }
        lab_dump_raw_row(block, console->dump_row);
        console->dump_row++;
    }
}

static void lab_step_metrology_capture(app_lab_console_t *console, uint32_t now_ms)
{
    if (!hw_metrology_session_active(&console->session) &&
        (hw_metrology_session_state(&console->session) != HW_METROLOGY_SESSION_DONE))
    {
        return;
    }

    const bsp_status_t status = hw_metrology_session_step(&console->session, now_ms);
    if (hw_metrology_session_state(&console->session) == HW_METROLOGY_SESSION_DONE)
    {
        if (hw_metrology_session_dumpable(&console->session))
        {
            const hw_metrology_block_t *block = hw_metrology_session_block(&console->session);
            lab_dump_metrology_header(block);
            console->dump_row = 0u;
            console->dump_source = APP_LAB_METROLOGY_DUMP_CAPTURE;
            console->dump_active = true;
        }
        else
        {
            write_text("lab metrology: ERROR ");
            write_text(hw_metrology_session_error_string(hw_metrology_session_error(&console->session)));
            write_text("\r\n");
            hw_metrology_session_acknowledge(&console->session);
        }
        (void)status;
        return;
    }

    if ((status == BSP_STATUS_ERROR) && !hw_metrology_session_active(&console->session))
    {
        write_text("lab metrology: ERROR ");
        write_text(hw_metrology_session_error_string(hw_metrology_session_error(&console->session)));
        write_text("\r\n");
    }
}

static void lab_step_metrology_measure(app_lab_console_t *console, uint32_t now_ms)
{
    if (!hw_metrology_measure_active(&console->measure) &&
        (hw_metrology_measure_state(&console->measure) != HW_METROLOGY_MEASURE_DONE))
    {
        return;
    }

    const bsp_status_t status = hw_metrology_measure_step(&console->measure, now_ms);
    if (hw_metrology_measure_state(&console->measure) == HW_METROLOGY_MEASURE_DONE)
    {
        if (hw_metrology_measure_dumpable(&console->measure))
        {
            const hw_metrology_block_t *block = hw_metrology_measure_block(&console->measure);
            lab_dump_metrology_header(block);
            console->dump_row = 0u;
            console->dump_source = APP_LAB_METROLOGY_DUMP_MEASURE;
            console->dump_active = true;
        }
        else
        {
            write_text("lab metrology: ERROR ");
            write_text(hw_metrology_measure_error_string(hw_metrology_measure_error(&console->measure)));
            write_text("\r\n");
            hw_metrology_measure_acknowledge(&console->measure);
        }
        (void)status;
        return;
    }

    if ((status == BSP_STATUS_ERROR) && !hw_metrology_measure_active(&console->measure))
    {
        write_text("lab metrology: ERROR ");
        write_text(hw_metrology_measure_error_string(hw_metrology_measure_error(&console->measure)));
        write_text("\r\n");
    }
}

static void lab_step_auto_measure(app_lab_console_t *console, uint32_t now_ms)
{
    const app_measurement_event_t event = app_measurement_session_step(&console->auto_measure, now_ms);
    switch (event)
    {
    case APP_MEASUREMENT_EVENT_AUTO_BEGIN:
        write_text("AUTO_BEGIN\r\nsession=");
        write_u32(console->auto_sequence);
        write_text("\r\nmode=CLICK\r\n");
        break;
    case APP_MEASUREMENT_EVENT_ATTEMPT_BEGIN:
        write_text("ATTEMPT_BEGIN\r\n");
        lab_write_attempt_config(app_measurement_session_current_attempt(&console->auto_measure));
        break;
    case APP_MEASUREMENT_EVENT_PARTIAL_RESULT:
        lab_write_auto_result("PARTIAL_RESULT", app_measurement_session_partial(&console->auto_measure));
        break;
    case APP_MEASUREMENT_EVENT_FINAL_RESULT:
    {
        const measurement_session_result_t *final = app_measurement_session_final(&console->auto_measure);
        lab_write_auto_result("FINAL_RESULT", final);
        console->auto_hint = measurement_auto_make_hint(final);
        write_text("AUTO_END\r\n");
        break;
    }
    case APP_MEASUREMENT_EVENT_ERROR:
        write_text("AUTO_ERROR\r\n");
        break;
    case APP_MEASUREMENT_EVENT_NONE:
    default:
        break;
    }
}

static void lab_step_metrology(app_lab_console_t *console, uint32_t now_ms)
{
    if (console->dump_active)
    {
        lab_step_metrology_dump(console);
        return;
    }

    if (app_measurement_session_active(&console->auto_measure))
    {
        lab_step_auto_measure(console, now_ms);
        return;
    }

    lab_step_metrology_capture(console, now_ms);
    lab_step_metrology_measure(console, now_ms);
}

static void run_command(app_lab_console_t *console,
                        w25q_device_t *flash,
                        ili9341_t *display,
                        hw_range_t *range,
                        hw_charger_t *charger,
                        hw_aux_sensors_t *sensors,
                        hw_k1_t *k1,
                        const hw_safety_result_t *safety,
                        app_safety_fault_latch_t *faults,
                        const char *line,
                        uint32_t now_ms)
{
    (void)display;
    hw_range_id_t requested_range = HW_RANGE_ID_INVALID;
    hw_excitation_freq_t capture_freq = HW_EXCITATION_FREQ_INVALID;
    hw_excitation_amp_t capture_amp = HW_EXCITATION_AMP_INVALID;
    hw_range_id_t capture_range = HW_RANGE_ID_INVALID;
    hw_excitation_freq_t measure_freq = HW_EXCITATION_FREQ_INVALID;
    hw_excitation_amp_t measure_amp = HW_EXCITATION_AMP_INVALID;
    hw_range_id_t measure_range = HW_RANGE_ID_INVALID;

    lab_bind_refs(console, range, charger, sensors, k1, faults);

    if (text_equals(line, "lab quiet on"))
    {
        if (app_lab_console_flash_busy(console))
        {
            write_text("lab quiet: BUSY\r\n");
            return;
        }
        hw_peripherals_request_quiet(true);
        write_text("lab quiet: ON\r\n");
    }
    else if (text_equals(line, "lab quiet off"))
    {
        hw_peripherals_request_quiet(false);
        write_text("lab quiet: OFF\r\n");
    }
    else if (text_starts_with(line, "lab buzzer "))
    {
        const char *frequency_text = &line[11];
        const char *duration_text = frequency_text;
        while ((*duration_text != '\0') && (*duration_text != ' '))
        {
            duration_text++;
        }
        if (*duration_text != ' ')
        {
            write_text("lab buzzer: INVALID_ARG\r\n");
            return;
        }
        char frequency_buffer[8] = {0};
        const size_t frequency_len = (size_t)(duration_text - frequency_text);
        if (frequency_len >= sizeof(frequency_buffer))
        {
            write_text("lab buzzer: INVALID_ARG\r\n");
            return;
        }
        for (size_t i = 0u; i < frequency_len; i++)
        {
            frequency_buffer[i] = frequency_text[i];
        }

        uint16_t frequency_hz = 0u;
        uint16_t duration_ms = 0u;
        if (!parse_u16(frequency_buffer, &frequency_hz) || !parse_u16(duration_text + 1, &duration_ms))
        {
            write_text("lab buzzer: INVALID_ARG\r\n");
            return;
        }

        const bsp_status_t status = hw_buzzer_play_tone(frequency_hz, duration_ms, now_ms);
        write_text("lab buzzer: ");
        write_text(status == BSP_STATUS_OK ? "OK" : (status == BSP_STATUS_BUSY ? "BUSY" : "ERROR"));
        write_text("\r\n");
    }
    else if (text_equals(line, "lab flash info"))
    {
        write_flash_info(flash);
    }
    else if (text_equals(line, "lab flash selftest"))
    {
        start_flash_selftest(console, flash);
    }
    else if (parse_range_id(line, &requested_range))
    {
        if (range == NULL)
        {
            write_text("lab range: ERROR\r\n");
            return;
        }
        const bsp_status_t status = hw_range_request(range, requested_range, now_ms);
        if ((status != BSP_STATUS_OK) && (status != BSP_STATUS_BUSY))
        {
            app_safety_fault_latch(faults, APP_SAFETY_FAULT_RANGE_IO);
        }
        write_text("lab range: ");
        write_text(status == BSP_STATUS_BUSY ? "START" : bsp_status_string(status));
        write_text("\r\n");
    }
    else if (text_equals(line, "lab range off"))
    {
        if (range != NULL)
        {
            const bsp_status_t status = hw_range_force_disabled(range);
            if (status != BSP_STATUS_OK)
            {
                app_safety_fault_latch(faults, APP_SAFETY_FAULT_RANGE_IO);
            }
            write_text("lab range: ");
            write_text(bsp_status_string(status));
            write_text("\r\n");
            return;
        }
        write_text("lab range: OFF\r\n");
    }
    else if (text_equals(line, "lab range status"))
    {
        write_range_status(range);
    }
    else if (text_equals(line, "lab safety status"))
    {
        write_safety_status(safety);
    }
    else if (text_equals(line, "lab charger status"))
    {
        const hw_charger_state_t state = hw_charger_get_state(charger);
        write_text("charger: ");
        write_text(hw_charger_state_string(state));
        write_text("\r\n");
    }
    else if (text_equals(line, "lab sensors status"))
    {
        write_sensors_status(sensors, now_ms);
    }
    else if (text_equals(line, "lab adc status"))
    {
        write_adc_status(sensors);
    }
    else if (text_equals(line, "lab fault status"))
    {
        write_text("lab fault: ");
        write_hex8(app_safety_fault_mask(faults));
        write_text("\r\n");
    }
    else if (text_equals(line, "lab cal status"))
    {
        write_calibration_status(flash);
    }
    else if (text_equals(line, "lab permit status"))
    {
        write_permit_status(range, charger, sensors, k1, faults, now_ms);
    }
    else if (text_equals(line, "lab auto policy"))
    {
        write_auto_policy();
    }
    else if (text_equals(line, "lab auto measure"))
    {
        if (app_lab_console_flash_busy(console) || lab_metrology_busy(console))
        {
            write_text("lab auto: BUSY\r\n");
            return;
        }
        lab_start_auto_measure(console, now_ms);
    }
    else if (parse_capture_tokens(line, &capture_freq, &capture_amp, &capture_range))
    {
        if (app_lab_console_flash_busy(console) || lab_metrology_busy(console))
        {
            write_text("lab metrology: BUSY\r\n");
            return;
        }
        lab_start_metrology_capture(console, capture_freq, capture_amp, capture_range, now_ms);
    }
    else if (parse_measure_tokens(line, &measure_freq, &measure_amp, &measure_range))
    {
        if (app_lab_console_flash_busy(console) || lab_metrology_busy(console))
        {
            write_text("lab metrology: BUSY\r\n");
            return;
        }
        lab_start_metrology_measure(console, measure_freq, measure_amp, measure_range, now_ms);
    }
    else if (line[0] != '\0')
    {
        write_text("lab: UNKNOWN_COMMAND\r\n");
    }
}

#endif

void app_lab_console_init(app_lab_console_t *console)
{
    if (console == NULL)
    {
        return;
    }

#if WTK_ENABLE_LAB_DIAGNOSTICS
    console->line_length = 0u;
    console->flash_test_state = APP_LAB_FLASH_TEST_IDLE;
    console->test_sector_address = 0u;
    console->busy_observed = false;
    console->dump_active = false;
    console->dump_row = 0u;
    console->dump_source = APP_LAB_METROLOGY_DUMP_NONE;
    console->auto_hint = (measurement_auto_hint_t){0};
    console->auto_sequence = 0u;
    console->range_ref = NULL;
    console->k1_ref = NULL;
    console->sensors_ref = NULL;
    console->charger_ref = NULL;
    console->faults_ref = NULL;
    for (size_t i = 0u; i < sizeof(console->line); i++)
    {
        console->line[i] = '\0';
    }
    (void)lab_init_metrology_session(console);
    (void)lab_init_metrology_measure(console);
    (void)lab_init_auto_measure(console);
#else
    console->unused = 0u;
#endif
}

void app_lab_console_step(app_lab_console_t *console,
                          w25q_device_t *flash,
                          ili9341_t *display,
                          hw_range_t *range,
                          hw_charger_t *charger,
                          hw_aux_sensors_t *sensors,
                          hw_k1_t *k1,
                          const hw_safety_result_t *safety,
                          app_safety_fault_latch_t *faults,
                          uint32_t now_ms)
{
#if WTK_ENABLE_LAB_DIAGNOSTICS
    if (console == NULL)
    {
        return;
    }

    lab_bind_refs(console, range, charger, sensors, k1, faults);
    lab_step_metrology(console, now_ms);
    step_flash_selftest(console, flash, now_ms);

    uint8_t byte = 0u;
    while (bsp_uart_try_read_byte(&byte) == BSP_STATUS_OK)
    {
        if ((byte == '\r') || (byte == '\n'))
        {
            console->line[console->line_length] = '\0';
            run_command(console, flash, display, range, charger, sensors, k1, safety, faults, console->line, now_ms);
            console->line_length = 0u;
        }
        else if (console->line_length < (APP_LAB_CONSOLE_LINE_CAPACITY - 1u))
        {
            console->line[console->line_length] = (char)byte;
            console->line_length++;
        }
        else
        {
            console->line_length = 0u;
            write_text("lab: LINE_TOO_LONG\r\n");
        }
    }
#else
    (void)console;
    (void)flash;
    (void)display;
    (void)range;
    (void)charger;
    (void)sensors;
    (void)k1;
    (void)safety;
    (void)faults;
    (void)now_ms;
#endif
}

bool app_lab_console_flash_busy(const app_lab_console_t *console)
{
#if WTK_ENABLE_LAB_DIAGNOSTICS
    if (console == NULL)
    {
        return false;
    }

    return (console->flash_test_state != APP_LAB_FLASH_TEST_IDLE) &&
           (console->flash_test_state != APP_LAB_FLASH_TEST_COMPLETE) &&
           (console->flash_test_state != APP_LAB_FLASH_TEST_ERROR);
#else
    (void)console;
    return false;
#endif
}

bool app_lab_console_capture_busy(const app_lab_console_t *console)
{
#if WTK_ENABLE_LAB_DIAGNOSTICS
    if (console == NULL)
    {
        return false;
    }

    return lab_metrology_busy(console);
#else
    (void)console;
    return false;
#endif
}
