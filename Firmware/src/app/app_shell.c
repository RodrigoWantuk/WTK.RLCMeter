#include "app/app_shell.h"

#include "app/app_lab_console.h"
#include "bsp/bsp_clock.h"
#include "bsp/bsp_diagnostics.h"
#include "bsp/bsp_gpio.h"
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
#include "hardware/hw_buzzer.h"
#include "hardware/hw_charger.h"
#include "hardware/hw_k1.h"
#include "hardware/hw_k2.h"
#include "hardware/hw_range.h"
#include "hardware/hw_safety.h"
#include "ui/ui_fallback_renderer.h"
#include "wtk_build_config.h"

static buttons_t g_buttons;
static ili9341_t g_display;
static w25q_device_t g_flash;
static hw_k1_t g_k1;
static hw_k2_t g_k2;
static hw_range_t g_range;
static hw_charger_t g_charger;
static hw_safety_result_t g_safety_result;
#if WTK_ENABLE_LAB_DIAGNOSTICS
static app_lab_console_t g_lab_console;
static bool g_display_ready_reported = false;
#endif
static bool g_flash_fallback_drawn = false;
static bool g_safety_state_reported = false;

typedef enum
{
    APP_SAFETY_SAFE_CHECK = 0,
    APP_SAFETY_WAIT_SAFE,
    APP_SAFETY_READY,
} app_safety_state_t;

static app_safety_state_t g_safety_state = APP_SAFETY_SAFE_CHECK;

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

#if WTK_ENABLE_LAB_DIAGNOSTICS
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
    const hw_safety_input_t safety_input = {
        .charger = hw_charger_get_state(&g_charger),
        .residual = HW_RESIDUAL_UNKNOWN,
        .battery = HW_BATTERY_UNKNOWN,
        .range = hw_range_safety_state(&g_range),
        .application_fault = false,
    };

    g_safety_result = hw_safety_evaluate(&safety_input);
    if (g_safety_result.measure_allowed)
    {
        g_safety_state = APP_SAFETY_READY;
    }
    else
    {
        g_safety_state = APP_SAFETY_WAIT_SAFE;
        (void)hw_k1_force_safe(&g_k1);
    }

    if (!g_safety_state_reported)
    {
        bsp_diagnostics_write_key_value_text("safety_state", app_safety_state_string(g_safety_state));
        bsp_diagnostics_write_key_value_text("safety_block",
                                             hw_safety_primary_blocker_string(g_safety_result.primary_blocker));
        g_safety_state_reported = true;
    }
}

static void app_step(void)
{
    const uint32_t now_ms = bsp_time_now_ms();
    hw_range_step(&g_range, now_ms);
    app_update_safety_state();

    buttons_update(&g_buttons, app_read_button_mask(), now_ms);

    button_event_t event;
    while (buttons_pop_event(&g_buttons, &event))
    {
        app_log_button_event(&event);
    }

    (void)ili9341_init_step(&g_display, now_ms);
#if WTK_ENABLE_LAB_DIAGNOSTICS
    if (g_display.ready && !g_display_ready_reported)
    {
        bsp_uart_write_cstr("display: READY\r\n");
        g_display_ready_reported = true;
    }
#endif
    if (g_display.ready && !g_flash.detected && !g_flash_fallback_drawn)
    {
        if (ui_fallback_draw_text(&g_display, 8u, 8u, "FLASH ERROR", 0xFFFFu, 0x0000u) == BSP_STATUS_OK)
        {
            g_flash_fallback_drawn = true;
#if WTK_ENABLE_LAB_DIAGNOSTICS
            bsp_uart_write_cstr("fallback_ui: FLASH_ERROR_DRAWN\r\n");
#endif
        }
    }

#if WTK_ENABLE_LAB_DIAGNOSTICS
    app_lab_console_step(&g_lab_console, &g_flash, &g_display, &g_range, &g_charger, &g_safety_result, now_ms);
    if (!app_lab_console_flash_busy(&g_lab_console))
    {
        (void)w25q_device_poll(&g_flash, now_ms);
    }
#else
    (void)w25q_device_poll(&g_flash, now_ms);
#endif
    hw_buzzer_step(now_ms);
}

void app_shell_run(void)
{
    const bsp_reset_reason_t reset_reason = bsp_reset_capture_reason();

    (void)bsp_gpio_init_safe();
    const bsp_status_t clock_status = bsp_clock_init();
    (void)bsp_time_init();
    (void)bsp_uart_init(115200u);

    bsp_diagnostics_boot_banner(reset_reason, clock_status);
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
    bsp_diagnostics_write_key_value_text("k1", bsp_status_string(k1_status));
    const bsp_status_t k2_status = hw_k2_init(&g_k2, &k2_io);
    bsp_diagnostics_write_key_value_text("k2", bsp_status_string(k2_status));
    const bsp_status_t range_status = hw_range_init(&g_range, &range_io);
    bsp_diagnostics_write_key_value_text("range", bsp_status_string(range_status));
    const bsp_status_t charger_init_status = hw_charger_init(&g_charger, &charger_io);
    bsp_diagnostics_write_key_value_text("charger_init", bsp_status_string(charger_init_status));
    bsp_diagnostics_write_key_value_text("charger", hw_charger_state_string(hw_charger_get_state(&g_charger)));
    bsp_diagnostics_write_key_value_text("k2_topology", hw_lowz_bank_mode_string(hw_k2_topology(&g_k2).lowz_bank_mode));
    g_safety_result = hw_safety_evaluate(NULL);
    g_safety_state = APP_SAFETY_SAFE_CHECK;
    g_safety_state_reported = false;
#if WTK_ENABLE_LAB_DIAGNOSTICS
    app_lab_console_init(&g_lab_console);
    g_display_ready_reported = false;
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
        const unsigned int jedec = ((unsigned int)g_flash.part.jedec.manufacturer_id << 16u) |
                                   ((unsigned int)g_flash.part.jedec.memory_type << 8u) |
                                   (unsigned int)g_flash.part.jedec.capacity_code;
        bsp_diagnostics_write_key_value_text("w25q_part", g_flash.part.name);
        bsp_diagnostics_write_key_value_hex8("w25q_jedec", jedec);
        bsp_diagnostics_write_key_value_u32("w25q_capacity", g_flash.part.capacity_bytes);
        bsp_diagnostics_write_key_value_u32("w25q_test_sector",
                                            w25q_reserved_test_sector_address(g_flash.part.capacity_bytes));
    }

    ili9341_init_start(&g_display, bsp_time_now_ms());

    for (;;)
    {
        app_step();
        bsp_diagnostics_step();
        bsp_watchdog_service();
    }
}
