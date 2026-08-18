#include "bsp/bsp_diagnostics.h"

#include <stdint.h>

#include "app/app_version.h"
#include "bsp/bsp_clock.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_reset.h"
#include "bsp/bsp_status.h"
#include "bsp/bsp_time.h"
#include "bsp/bsp_uart.h"
#include "bsp/bsp_watchdog.h"
#include "wtk_build_config.h"

static bsp_log_level_t g_log_level = (bsp_log_level_t)WTK_DIAGNOSTIC_LOG_LEVEL_DEFAULT;

static void write_text(const char *text)
{
    (void)bsp_uart_write_cstr(text);
}

static void write_u32(uint32_t value)
{
    char buffer[11];
    uint32_t index = 0u;

    if (value == 0u)
    {
        write_text("0");
        return;
    }

    while ((value > 0u) && (index < (uint32_t)sizeof(buffer)))
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

static void write_hex8(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    write_text("0x");
    for (int32_t shift = 28; shift >= 0; shift -= 4)
    {
        const uint32_t nibble = (value >> (uint32_t)shift) & 0xFu;
        (void)bsp_uart_write(&digits[nibble], 1u);
    }
}

void bsp_diagnostics_write_key_value_text(const char *key, const char *value)
{
    write_text(key);
    write_text(": ");
    write_text(value);
    write_text("\r\n");
}

void bsp_diagnostics_write_key_value_u32(const char *key, unsigned long value)
{
    write_text(key);
    write_text(": ");
    write_u32((uint32_t)value);
    write_text("\r\n");
}

void bsp_diagnostics_write_key_value_hex8(const char *key, unsigned int value)
{
    write_text(key);
    write_text(": ");
    write_hex8((uint32_t)value);
    write_text("\r\n");
}

void bsp_diagnostics_set_level(bsp_log_level_t level)
{
    g_log_level = level;
}

void bsp_diagnostics_write(bsp_log_level_t level, const char *message)
{
    if ((uint32_t)level <= (uint32_t)g_log_level)
    {
        write_text("[");
        write_u32(bsp_time_now_ms());
        write_text("] ");
        write_text(message);
        write_text("\r\n");
    }
}

void bsp_diagnostics_boot_banner(bsp_reset_reason_t reset_reason, bsp_status_t clock_status)
{
    const wtk_app_version_info_t *const version = wtk_app_version_get();
    const bsp_clock_summary_t *const clock = bsp_clock_get_summary();

    write_text("\r\nWTK.RLCMeter\r\n");
    bsp_diagnostics_write_key_value_text("firmware", version->project_version);
    bsp_diagnostics_write_key_value_text("git", version->git_commit);
    bsp_diagnostics_write_key_value_text("build", version->build_type);
    bsp_diagnostics_write_key_value_text("hardware", version->hardware_compatibility);
    bsp_diagnostics_write_key_value_text("reset", bsp_reset_reason_string(reset_reason));
    bsp_diagnostics_write_key_value_text("clock_status", bsp_status_string(clock_status));
    bsp_diagnostics_write_key_value_text("clock_source", bsp_clock_source_string(clock->source));
    bsp_diagnostics_write_key_value_u32("sysclk_hz", clock->sysclk_hz);
    bsp_diagnostics_write_key_value_u32("hclk_hz", clock->hclk_hz);
    bsp_diagnostics_write_key_value_u32("pclk1_hz", clock->pclk1_hz);
    bsp_diagnostics_write_key_value_u32("pclk2_hz", clock->pclk2_hz);
    bsp_diagnostics_write_key_value_u32("adc_hz", clock->adc_hz);
    bsp_diagnostics_write_key_value_text("swd", bsp_gpio_swd_preserved() ? "PRESERVED" : "UNKNOWN");
    bsp_diagnostics_write_key_value_text("boot_state", "SAFE_BOOT");
    bsp_diagnostics_write_key_value_text("watchdog_policy", "IWDG_START_AFTER_UART_BANNER");
}

void bsp_diagnostics_step(void)
{
    if (bsp_watchdog_is_started())
    {
        return;
    }
}
