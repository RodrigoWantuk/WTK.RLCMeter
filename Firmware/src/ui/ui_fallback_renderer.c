#include "ui/ui_fallback_renderer.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "drivers/ili9341.h"
#include "ui/ui_fallback_font.h"

enum
{
    UI_FALLBACK_GLYPH_SPACING = 1u,
};

static bsp_status_t draw_scaled_char(const ili9341_t *display,
                                     uint16_t x,
                                     uint16_t y,
                                     char ch,
                                     uint8_t scale,
                                     uint16_t fg_rgb565,
                                     uint16_t bg_rgb565)
{
    uint16_t pixels[UI_FALLBACK_GLYPH_WIDTH * 3u];
    uint8_t rows[UI_FALLBACK_GLYPH_HEIGHT] = {0u};
    (void)ui_fallback_font_get_glyph(ch, rows);
    for (uint16_t row = 0u; row < UI_FALLBACK_GLYPH_HEIGHT; row++)
    {
        uint16_t width = 0u;
        for (uint16_t col = 0u; col < UI_FALLBACK_GLYPH_WIDTH; col++)
        {
            const uint8_t mask = (uint8_t)(1u << (UI_FALLBACK_GLYPH_WIDTH - 1u - col));
            const uint16_t color = ((rows[row] & mask) != 0u) ? fg_rgb565 : bg_rgb565;
            for (uint8_t sx = 0u; sx < scale; sx++)
            {
                pixels[width++] = color;
            }
        }
        for (uint8_t sy = 0u; sy < scale; sy++)
        {
            bsp_status_t status =
                ili9341_set_window(display,
                                   x,
                                   (uint16_t)(y + (row * scale) + sy),
                                   width,
                                   1u);
            if (status != BSP_STATUS_OK)
            {
                return status;
            }
            status = ili9341_write_pixels_rgb565(pixels, width);
            if (status != BSP_STATUS_OK)
            {
                return status;
            }
        }
    }
    return BSP_STATUS_OK;
}

bsp_status_t ui_fallback_draw_char(const ili9341_t *display,
                                   uint16_t x,
                                   uint16_t y,
                                   char ch,
                                   uint16_t fg_rgb565,
                                   uint16_t bg_rgb565)
{
    uint8_t rows[UI_FALLBACK_GLYPH_HEIGHT] = {0u};
    uint16_t pixels[UI_FALLBACK_GLYPH_WIDTH];

    (void)ui_fallback_font_get_glyph(ch, rows);

    for (uint16_t row = 0u; row < UI_FALLBACK_GLYPH_HEIGHT; row++)
    {
        for (uint16_t col = 0u; col < UI_FALLBACK_GLYPH_WIDTH; col++)
        {
            const uint8_t mask = (uint8_t)(1u << (UI_FALLBACK_GLYPH_WIDTH - 1u - col));
            pixels[col] = ((rows[row] & mask) != 0u) ? fg_rgb565 : bg_rgb565;
        }

        bsp_status_t status =
            ili9341_set_window(display, x, (uint16_t)(y + row), UI_FALLBACK_GLYPH_WIDTH, 1u);
        if (status != BSP_STATUS_OK)
        {
            return status;
        }

        status = ili9341_write_pixels_rgb565(pixels, UI_FALLBACK_GLYPH_WIDTH);
        if (status != BSP_STATUS_OK)
        {
            return status;
        }
    }

    return BSP_STATUS_OK;
}

bsp_status_t ui_fallback_draw_text(const ili9341_t *display,
                                   uint16_t x,
                                   uint16_t y,
                                   const char *text,
                                   uint16_t fg_rgb565,
                                   uint16_t bg_rgb565)
{
    if (text == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    uint16_t cursor_x = x;
    for (size_t i = 0u; text[i] != '\0'; i++)
    {
        const bsp_status_t status = ui_fallback_draw_char(display, cursor_x, y, text[i], fg_rgb565, bg_rgb565);
        if (status != BSP_STATUS_OK)
        {
            return status;
        }

        cursor_x = (uint16_t)(cursor_x + UI_FALLBACK_GLYPH_WIDTH + UI_FALLBACK_GLYPH_SPACING);
    }

    return BSP_STATUS_OK;
}

bsp_status_t ui_fallback_draw_text_scaled(const ili9341_t *display,
                                          uint16_t x,
                                          uint16_t y,
                                          const char *text,
                                          uint8_t scale,
                                          uint16_t fg_rgb565,
                                          uint16_t bg_rgb565)
{
    if ((text == NULL) || (scale == 0u) || (scale > 3u))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    uint16_t cursor_x = x;
    for (size_t i = 0u; text[i] != '\0'; i++)
    {
        const bsp_status_t status =
            draw_scaled_char(display, cursor_x, y, text[i], scale, fg_rgb565, bg_rgb565);
        if (status != BSP_STATUS_OK)
        {
            return status;
        }
        cursor_x = (uint16_t)(cursor_x + ((UI_FALLBACK_GLYPH_WIDTH + UI_FALLBACK_GLYPH_SPACING) * scale));
    }
    return BSP_STATUS_OK;
}

void ui_fallback_text_scaled_start(ui_fallback_text_op_t *op,
                                   uint16_t x,
                                   uint16_t y,
                                   const char *text,
                                   uint8_t scale,
                                   uint16_t fg_rgb565,
                                   uint16_t bg_rgb565)
{
    if (op == NULL)
    {
        return;
    }
    *op = (ui_fallback_text_op_t){0};
    if ((text == NULL) || (scale == 0u) || (scale > 3u))
    {
        return;
    }
    (void)strncpy(op->text, text, sizeof(op->text) - 1u);
    op->x = x;
    op->y = y;
    op->fg_rgb565 = fg_rgb565;
    op->bg_rgb565 = bg_rgb565;
    op->scale = scale;
    op->active = op->text[0] != '\0';
}

bsp_status_t ui_fallback_text_scaled_step(const ili9341_t *display,
                                          ui_fallback_text_op_t *op)
{
    if ((display == NULL) || (op == NULL) || !op->active)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    const char ch = op->text[op->index];
    if (ch == '\0')
    {
        op->active = false;
        return BSP_STATUS_OK;
    }
    const uint16_t cursor_x =
        (uint16_t)(op->x + ((uint16_t)op->index *
                            (uint16_t)((UI_FALLBACK_GLYPH_WIDTH + UI_FALLBACK_GLYPH_SPACING) * op->scale)));
    const bsp_status_t status =
        draw_scaled_char(display, cursor_x, op->y, ch, op->scale, op->fg_rgb565, op->bg_rgb565);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    op->index++;
    if ((op->index >= (sizeof(op->text) - 1u)) || (op->text[op->index] == '\0'))
    {
        op->active = false;
        return BSP_STATUS_OK;
    }
    return BSP_STATUS_BUSY;
}
