#ifndef WTK_UI_FALLBACK_RENDERER_H
#define WTK_UI_FALLBACK_RENDERER_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "drivers/ili9341.h"

enum
{
    UI_FALLBACK_TEXT_MAX_CHARS = 32u,
};

typedef struct
{
    char text[UI_FALLBACK_TEXT_MAX_CHARS];
    uint16_t x;
    uint16_t y;
    uint16_t fg_rgb565;
    uint16_t bg_rgb565;
    uint8_t scale;
    uint8_t index;
    bool active;
} ui_fallback_text_op_t;

bsp_status_t ui_fallback_draw_char(const ili9341_t *display,
                                   uint16_t x,
                                   uint16_t y,
                                   char ch,
                                   uint16_t fg_rgb565,
                                   uint16_t bg_rgb565);
bsp_status_t ui_fallback_draw_text(const ili9341_t *display,
                                   uint16_t x,
                                   uint16_t y,
                                   const char *text,
                                   uint16_t fg_rgb565,
                                   uint16_t bg_rgb565);
bsp_status_t ui_fallback_draw_text_scaled(const ili9341_t *display,
                                          uint16_t x,
                                          uint16_t y,
                                          const char *text,
                                          uint8_t scale,
                                          uint16_t fg_rgb565,
                                          uint16_t bg_rgb565);
void ui_fallback_text_scaled_start(ui_fallback_text_op_t *op,
                                   uint16_t x,
                                   uint16_t y,
                                   const char *text,
                                   uint8_t scale,
                                   uint16_t fg_rgb565,
                                   uint16_t bg_rgb565);
bsp_status_t ui_fallback_text_scaled_step(const ili9341_t *display,
                                          ui_fallback_text_op_t *op);

#endif
