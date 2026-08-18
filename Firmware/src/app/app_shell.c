#include "app/app_shell.h"

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
#include "ui/ui_fallback_renderer.h"
#include "wtk_build_config.h"

static buttons_t g_buttons;
static ili9341_t g_display;
static w25q_device_t g_flash;
static bool g_flash_fallback_drawn = false;

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

static void app_step(void)
{
    const uint32_t now_ms = bsp_time_now_ms();
    buttons_update(&g_buttons, app_read_button_mask(), now_ms);

    button_event_t event;
    while (buttons_pop_event(&g_buttons, &event))
    {
        app_log_button_event(&event);
    }

    (void)ili9341_init_step(&g_display, now_ms);
    if (g_display.ready && !g_flash.detected && !g_flash_fallback_drawn)
    {
        if (ui_fallback_draw_text(&g_display, 8u, 8u, "FLASH ERROR", 0xFFFFu, 0x0000u) == BSP_STATUS_OK)
        {
            g_flash_fallback_drawn = true;
        }
    }

    (void)w25q_device_poll(&g_flash, now_ms);
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
