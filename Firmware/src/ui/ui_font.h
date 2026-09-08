#ifndef WTK_UI_FONT_H
#define WTK_UI_FONT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "drivers/ili9341.h"
#include "storage/resource_store.h"

typedef enum
{
    UI_FONT_ROLE_SMALL = 0,
    UI_FONT_ROLE_MEDIUM,
    UI_FONT_ROLE_LARGE,
    UI_FONT_ROLE_COUNT,
} ui_font_role_t;

enum
{
    UI_FONT_TEXT_MAX_CHARS = 32u,
};

typedef struct
{
    uint32_t codepoint;
    uint32_t bitmap_offset;
    uint16_t bitmap_size;
    uint8_t width;
    uint8_t height;
    int8_t advance_x;
    int8_t bearing_x;
    int8_t bearing_y;
    uint8_t row_stride;
} ui_font_glyph_t;

typedef struct
{
    resource_entry_t entry;
    uint16_t glyph_count;
    uint8_t ascent;
    uint8_t descent;
    uint8_t line_height;
    uint32_t index_offset;
    uint32_t bitmap_offset;
    bool ready;
} ui_font_face_t;

typedef struct
{
    resource_catalog_t *resource_catalog;
    ui_font_face_t faces[UI_FONT_ROLE_COUNT];
    bool ready;
} ui_font_catalog_t;

typedef struct
{
    char text[UI_FONT_TEXT_MAX_CHARS];
    ui_font_catalog_t *catalog;
    uint16_t x;
    uint16_t y;
    uint16_t cursor_x;
    uint16_t fg_rgb565;
    uint16_t bg_rgb565;
    uint8_t byte_index;
    ui_font_role_t role;
    bool active;
} ui_font_text_op_t;

void ui_font_catalog_init(ui_font_catalog_t *catalog);
resource_status_t ui_font_catalog_mount(ui_font_catalog_t *catalog,
                                        resource_catalog_t *resource_catalog);
bool ui_font_catalog_ready(const ui_font_catalog_t *catalog);
resource_status_t ui_font_lookup_glyph(const ui_font_catalog_t *catalog,
                                       ui_font_role_t role,
                                       uint32_t codepoint,
                                       ui_font_glyph_t *glyph);
resource_status_t ui_font_read_glyph_bitmap(const ui_font_catalog_t *catalog,
                                            ui_font_role_t role,
                                            const ui_font_glyph_t *glyph,
                                            uint8_t *dst,
                                            size_t capacity);
void ui_font_text_start(ui_font_text_op_t *op,
                        ui_font_catalog_t *catalog,
                        ui_font_role_t role,
                        uint16_t x,
                        uint16_t y,
                        const char *text,
                        uint16_t fg_rgb565,
                        uint16_t bg_rgb565);
bsp_status_t ui_font_text_step(const ili9341_t *display, ui_font_text_op_t *op);
uint32_t ui_font_catalog_context_size_bytes(void);
uint32_t ui_font_face_size_bytes(void);
uint32_t ui_font_glyph_size_bytes(void);
uint32_t ui_font_text_op_size_bytes(void);

#endif
