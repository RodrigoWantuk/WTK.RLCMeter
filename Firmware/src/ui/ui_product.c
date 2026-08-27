#include "ui/ui_product.h"

#include <stddef.h>

#include "ui/ui_fallback_renderer.h"
#include "ui/ui_format.h"

enum
{
    UI_COLOR_BLACK = 0x0000u,
    UI_COLOR_WHITE = 0xFFFFu,
    UI_COLOR_AMBER = 0xFFE0u,
    UI_COLOR_RED = 0xF800u,
    UI_COLOR_CYAN = 0x07FFu,
    UI_COLOR_GREEN = 0x07E0u,
};

static const char *freq_token(hw_excitation_freq_t frequency)
{
    switch (frequency)
    {
    case HW_EXCITATION_FREQ_100HZ:
        return "100HZ";
    case HW_EXCITATION_FREQ_1KHZ:
        return "1KHZ";
    case HW_EXCITATION_FREQ_10KHZ:
        return "10KHZ";
    case HW_EXCITATION_FREQ_INVALID:
    default:
        return "FREQ?";
    }
}

static const char *amp_token(hw_excitation_amp_t amplitude)
{
    switch (amplitude)
    {
    case HW_EXCITATION_AMP_100MVRMS:
        return "100MV";
    case HW_EXCITATION_AMP_500MVRMS:
        return "500MV";
    case HW_EXCITATION_AMP_INVALID:
    default:
        return "AMP?";
    }
}

static bool append_char(char *dst, size_t capacity, size_t *used, char ch)
{
    if ((dst == NULL) || (used == NULL) || ((*used + 1u) >= capacity))
    {
        return false;
    }
    dst[*used] = ch;
    (*used)++;
    dst[*used] = '\0';
    return true;
}

static bool append_text(char *dst, size_t capacity, size_t *used, const char *text)
{
    if (text == NULL)
    {
        return false;
    }
    for (size_t i = 0u; text[i] != '\0'; i++)
    {
        if (!append_char(dst, capacity, used, text[i]))
        {
            return false;
        }
    }
    return true;
}

static bool append_hex8(char *dst, size_t capacity, size_t *used, uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4)
    {
        if (!append_char(dst, capacity, used, hex[(value >> (uint32_t)shift) & 0x0Fu]))
        {
            return false;
        }
    }
    return true;
}

static const char *blocker_text(ui_product_blocker_t blocker)
{
    switch (blocker)
    {
    case UI_PRODUCT_BLOCK_CHARGER:
        return "REMOVE CHARGER";
    case UI_PRODUCT_BLOCK_RESIDUAL:
        return "VOLTAGE DETECTED";
    case UI_PRODUCT_BLOCK_SENSOR:
        return "SENSOR ERROR";
    case UI_PRODUCT_BLOCK_SUPPLY:
        return "SUPPLY ERROR";
    case UI_PRODUCT_BLOCK_RANGE:
        return "RANGE ERROR";
    case UI_PRODUCT_BLOCK_FAULT:
        return "FAULT";
    case UI_PRODUCT_BLOCK_NONE:
    default:
        return "SAFETY";
    }
}

static bsp_status_t draw_line(const ili9341_t *display,
                              uint16_t x,
                              uint16_t y,
                              const char *text,
                              uint8_t scale,
                              uint16_t color)
{
    return ui_fallback_draw_text_scaled(display, x, y, text, scale, color, UI_COLOR_BLACK);
}

static bsp_status_t draw_result_primary(const ili9341_t *display, const measurement_session_result_t *result)
{
    char value[24] = {0};
    (void)ui_format_primary_value(result, value, sizeof(value));
    bsp_status_t status = draw_line(display,
                                    8u,
                                    24u,
                                    ui_format_interpretation_token(result->classification.interpretation),
                                    2u,
                                    UI_COLOR_CYAN);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    status = draw_line(display, 8u, 64u, value, 3u, UI_COLOR_WHITE);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    char footer[32] = {0};
    size_t used = 0u;
    if (!append_text(footer, sizeof(footer), &used, amp_token(result->primary_attempt.config.amplitude)) ||
        !append_char(footer, sizeof(footer), &used, ' ') ||
        !append_text(footer, sizeof(footer), &used, freq_token(result->primary_attempt.config.frequency)))
    {
        return BSP_STATUS_ERROR;
    }
    return draw_line(display, 8u, 118u, footer, 1u, UI_COLOR_GREEN);
}

static bsp_status_t draw_result_details(const ili9341_t *display, const measurement_session_result_t *result)
{
    char text[32] = {0};
    bsp_status_t status = draw_line(display, 8u, 12u, "DETAILS", 2u, UI_COLOR_CYAN);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    (void)ui_format_resistance(result->primary_attempt.derived.resistance_ohms, text, sizeof(text));
    status = draw_line(display, 8u, 48u, "R", 1u, UI_COLOR_AMBER);
    if (status == BSP_STATUS_OK)
    {
        status = draw_line(display, 38u, 48u, text, 1u, UI_COLOR_WHITE);
    }
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    (void)ui_format_reactance(result->primary_attempt.derived.reactance_ohms, text, sizeof(text));
    status = draw_line(display, 8u, 66u, "X", 1u, UI_COLOR_AMBER);
    if (status == BSP_STATUS_OK)
    {
        status = draw_line(display, 38u, 66u, text, 1u, UI_COLOR_WHITE);
    }
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    (void)ui_format_phase_rad(result->primary_attempt.derived.phase_rad, text, sizeof(text));
    status = draw_line(display, 8u, 84u, "PHASE", 1u, UI_COLOR_AMBER);
    if (status == BSP_STATUS_OK)
    {
        status = draw_line(display, 56u, 84u, text, 1u, UI_COLOR_WHITE);
    }
    return status;
}

static bsp_status_t draw_view(const ili9341_t *display, const ui_product_view_t *view)
{
    switch (view->state)
    {
    case UI_PRODUCT_STATE_STARTUP:
        return draw_line(display, 8u, 24u, "WTK RLC", 3u, UI_COLOR_WHITE);
    case UI_PRODUCT_STATE_SELF_TEST:
        return draw_line(display, 8u, 24u, "STARTING", 2u, UI_COLOR_WHITE);
    case UI_PRODUCT_STATE_CALIBRATION_CHECK:
        return draw_line(display, 8u, 24u, "CAL CHECK", 2u, UI_COLOR_WHITE);
    case UI_PRODUCT_STATE_CALIBRATION_REQUIRED:
        if (view->storage_unavailable || (view->calibration_status == UI_PRODUCT_CAL_STORAGE_ERROR))
        {
            return draw_line(display, 8u, 24u, "STORAGE ERROR", 2u, UI_COLOR_RED);
        }
        return draw_line(display, 8u, 24u, "CALIBRATION REQUIRED", 1u, UI_COLOR_AMBER);
    case UI_PRODUCT_STATE_READY:
        return draw_line(display, 8u, 24u, "READY", 3u, UI_COLOR_GREEN);
    case UI_PRODUCT_STATE_MEASURING:
    {
        bsp_status_t status = draw_line(display, 8u, 24u, "MEASURING", 2u, UI_COLOR_WHITE);
        if ((status == BSP_STATUS_OK) && view->has_measurement_result)
        {
            status = draw_result_primary(display, &view->measurement_result);
        }
        return status;
    }
    case UI_PRODUCT_STATE_RESULT:
        if (!view->has_measurement_result)
        {
            return draw_line(display, 8u, 24u, "READY", 3u, UI_COLOR_GREEN);
        }
        return (view->page == UI_PRODUCT_PAGE_DETAILS) ?
                   draw_result_details(display, &view->measurement_result) :
                   draw_result_primary(display, &view->measurement_result);
    case UI_PRODUCT_STATE_SAFETY_BLOCKED:
        return draw_line(display, 8u, 24u, blocker_text(view->safety_blocker), 1u, UI_COLOR_RED);
    case UI_PRODUCT_STATE_FAULT:
    default:
    {
        char fault[28] = {0};
        size_t used = 0u;
        if (!append_text(fault, sizeof(fault), &used, "FAULT ") ||
            !append_hex8(fault, sizeof(fault), &used, view->safety_fault_mask))
        {
            return BSP_STATUS_ERROR;
        }
        return draw_line(display, 8u, 24u, fault, 1u, UI_COLOR_RED);
    }
    }
}

void ui_product_init(ui_product_t *ui)
{
    if (ui != NULL)
    {
        *ui = (ui_product_t){0};
    }
}

void ui_product_request(ui_product_t *ui, const ui_product_view_t *view)
{
    if ((ui == NULL) || (view == NULL) || (ui->rendered_generation == view->generation))
    {
        return;
    }
    ui->pending = *view;
    ui->active = true;
    ui->clear_started = false;
}

bsp_status_t ui_product_step(ui_product_t *ui, const ili9341_t *display, bool quiet)
{
    if ((ui == NULL) || (display == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!ui->active || !display->ready)
    {
        return BSP_STATUS_OK;
    }
    if (quiet)
    {
        return BSP_STATUS_BUSY;
    }
    if (!ui->clear_started)
    {
        ili9341_fill_start(&ui->clear_fill, 0u, 0u, ILI9341_WIDTH, ILI9341_HEIGHT, UI_COLOR_BLACK);
        ui->clear_started = true;
    }
    const bsp_status_t clear_status = ili9341_fill_step(display, &ui->clear_fill, ILI9341_FILL_CHUNK_PIXELS);
    if (clear_status == BSP_STATUS_BUSY)
    {
        return BSP_STATUS_BUSY;
    }
    if (clear_status != BSP_STATUS_OK)
    {
        ui->active = false;
        return clear_status;
    }
    const bsp_status_t draw_status = draw_view(display, &ui->pending);
    if (draw_status == BSP_STATUS_OK)
    {
        ui->rendered_generation = ui->pending.generation;
        ui->active = false;
    }
    return draw_status;
}

uint32_t ui_product_context_size_bytes(void)
{
    return (uint32_t)sizeof(ui_product_t);
}

const char *ui_product_state_string(ui_product_state_t state)
{
    switch (state)
    {
    case UI_PRODUCT_STATE_STARTUP:
        return "STARTUP";
    case UI_PRODUCT_STATE_SELF_TEST:
        return "SELF_TEST";
    case UI_PRODUCT_STATE_CALIBRATION_CHECK:
        return "CALIBRATION_CHECK";
    case UI_PRODUCT_STATE_CALIBRATION_REQUIRED:
        return "CALIBRATION_REQUIRED";
    case UI_PRODUCT_STATE_READY:
        return "READY";
    case UI_PRODUCT_STATE_MEASURING:
        return "MEASURING";
    case UI_PRODUCT_STATE_RESULT:
        return "RESULT";
    case UI_PRODUCT_STATE_SAFETY_BLOCKED:
        return "SAFETY_BLOCKED";
    case UI_PRODUCT_STATE_FAULT:
    default:
        return "FAULT";
    }
}
