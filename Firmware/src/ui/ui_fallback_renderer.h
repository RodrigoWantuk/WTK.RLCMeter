#ifndef WTK_UI_FALLBACK_RENDERER_H
#define WTK_UI_FALLBACK_RENDERER_H

#include <stdint.h>

#include "bsp/bsp_status.h"
#include "drivers/ili9341.h"

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

#endif
