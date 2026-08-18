#include "ui/ui_fallback_renderer.h"

#include <stddef.h>

#include "drivers/ili9341.h"
#include "ui/ui_fallback_font.h"

enum
{
    UI_FALLBACK_GLYPH_SPACING = 1u,
};

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
