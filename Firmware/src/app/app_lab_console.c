#include "app/app_lab_console.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
#include "hardware/hw_safety.h"
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

static void run_command(app_lab_console_t *console,
                        w25q_device_t *flash,
                        ili9341_t *display,
                        hw_range_t *range,
                        hw_charger_t *charger,
                        hw_aux_sensors_t *sensors,
                        const hw_k1_t *k1,
                        const hw_safety_result_t *safety,
                        app_safety_fault_latch_t *faults,
                        const char *line,
                        uint32_t now_ms)
{
    (void)display;
    hw_range_id_t requested_range = HW_RANGE_ID_INVALID;

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
    else if (text_equals(line, "lab permit status"))
    {
        write_permit_status(range, charger, sensors, k1, faults, now_ms);
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
    for (size_t i = 0u; i < sizeof(console->line); i++)
    {
        console->line[i] = '\0';
    }
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
                          const hw_k1_t *k1,
                          const hw_safety_result_t *safety,
                          app_safety_fault_latch_t *faults,
                          uint32_t now_ms)
{
#if WTK_ENABLE_LAB_DIAGNOSTICS
    if (console == NULL)
    {
        return;
    }

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
