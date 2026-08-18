#ifndef WTK_UI_FONT_H
#define WTK_UI_FONT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint32_t glyph_id;
    uint16_t width;
    uint16_t height;
    int16_t advance_x;
    int16_t offset_x;
    int16_t offset_y;
    uint32_t bitmap_offset;
    uint32_t bitmap_size;
    uint16_t format;
} ui_font_glyph_t;

typedef struct
{
    bool (*lookup_glyph)(void *context, uint32_t codepoint, ui_font_glyph_t *glyph);
    bool (*read_glyph)(void *context, const ui_font_glyph_t *glyph, uint32_t offset, uint8_t *dst, size_t size);
    void *context;
} ui_font_backend_t;

#endif
