#ifndef WTK_UI_FALLBACK_RENDERER_H
#define WTK_UI_FALLBACK_RENDERER_H

#include <stdint.h>

#include "bsp/bsp_status.h"

bsp_status_t ui_fallback_draw_char(uint16_t x,
                                   uint16_t y,
                                   char ch,
                                   uint16_t fg_rgb565,
                                   uint16_t bg_rgb565);
bsp_status_t ui_fallback_draw_text(uint16_t x,
                                   uint16_t y,
                                   const char *text,
                                   uint16_t fg_rgb565,
                                   uint16_t bg_rgb565);

#endif
