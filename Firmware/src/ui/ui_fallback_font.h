#ifndef WTK_UI_FALLBACK_FONT_H
#define WTK_UI_FALLBACK_FONT_H

#include <stdbool.h>
#include <stdint.h>

enum
{
    UI_FALLBACK_GLYPH_WIDTH = 5u,
    UI_FALLBACK_GLYPH_HEIGHT = 7u,
};

bool ui_fallback_font_get_glyph(char ch, uint8_t rows[UI_FALLBACK_GLYPH_HEIGHT]);

#endif
