#include "ui/ui_font.h"

#include <stddef.h>
#include <string.h>

#include "storage/storage_crc32.h"
#include "ui/ui_utf8.h"

enum
{
    UI_FONT_REQUIRED_SPACE = 0x20u,
    UI_FONT_REQUIRED_UNKNOWN = 0x3Fu,
};

static uint32_t role_resource_id(ui_font_role_t role)
{
    switch (role)
    {
    case UI_FONT_ROLE_SMALL:
        return RESOURCE_ID_FONT_UI_SMALL;
    case UI_FONT_ROLE_MEDIUM:
        return RESOURCE_ID_FONT_UI_MEDIUM;
    case UI_FONT_ROLE_LARGE:
        return RESOURCE_ID_FONT_UI_LARGE;
    default:
        return 0u;
    }
}

static bsp_status_t bsp_from_resource(resource_status_t status)
{
    switch (status)
    {
    case RESOURCE_STATUS_OK:
        return BSP_STATUS_OK;
    case RESOURCE_STATUS_DEFERRED:
        return BSP_STATUS_BUSY;
    case RESOURCE_STATUS_INVALID_ARG:
    case RESOURCE_STATUS_OUT_OF_RANGE:
        return BSP_STATUS_INVALID_ARG;
    case RESOURCE_STATUS_MISSING:
    case RESOURCE_STATUS_NOT_FOUND:
    case RESOURCE_STATUS_CORRUPT:
    case RESOURCE_STATUS_INCOMPATIBLE_API:
    case RESOURCE_STATUS_INCOMPATIBLE_SCHEMA:
    case RESOURCE_STATUS_INVALID_UTF8:
    default:
        return BSP_STATUS_ERROR;
    }
}

static resource_status_t read_face(const ui_font_catalog_t *catalog,
                                   const ui_font_face_t *face,
                                   uint32_t offset,
                                   void *dst,
                                   size_t size)
{
    if ((catalog == NULL) || (face == NULL) || (dst == NULL))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    return resource_catalog_read(catalog->resource_catalog, &face->entry, offset, dst, size);
}

static resource_status_t read_record(const ui_font_catalog_t *catalog,
                                     const ui_font_face_t *face,
                                     uint16_t index,
                                     resource_font_a1_record_t *record,
                                     uint8_t bytes[RESOURCE_FONT_A1_RECORD_SIZE])
{
    const uint32_t offset = face->index_offset +
                            ((uint32_t)index * RESOURCE_FONT_A1_RECORD_SIZE);
    resource_status_t status = read_face(catalog, face, offset, bytes, RESOURCE_FONT_A1_RECORD_SIZE);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    return resource_store_decode_font_a1_record(bytes,
                                                &(resource_font_a1_header_t){
                                                    .bitmap_offset = face->bitmap_offset,
                                                },
                                                face->entry.payload_size,
                                                record);
}

static bool scalar_valid(uint32_t codepoint)
{
    return (codepoint <= 0x10FFFFu) &&
           !((codepoint >= 0xD800u) && (codepoint <= 0xDFFFu));
}

static void glyph_from_record(const resource_font_a1_record_t *record, ui_font_glyph_t *glyph)
{
    *glyph = (ui_font_glyph_t){
        .codepoint = record->codepoint,
        .bitmap_offset = record->bitmap_offset,
        .bitmap_size = record->bitmap_size,
        .width = record->width,
        .height = record->height,
        .advance_x = record->advance_x,
        .bearing_x = record->bearing_x,
        .bearing_y = record->bearing_y,
        .row_stride = record->row_stride,
    };
}

static resource_status_t validate_face_index(ui_font_catalog_t *catalog,
                                             ui_font_face_t *face,
                                             const resource_font_a1_header_t *header)
{
    uint8_t bytes[RESOURCE_FONT_A1_RECORD_SIZE];
    uint32_t crc = STORAGE_CRC32_INIT;
    uint32_t previous = 0u;
    uint32_t bitmap_cursor = 0u;
    bool have_space = false;
    bool have_unknown = false;
    for (uint16_t i = 0u; i < header->glyph_count; i++)
    {
        const uint32_t offset = header->index_offset +
                                ((uint32_t)i * RESOURCE_FONT_A1_RECORD_SIZE);
        resource_status_t status = read_face(catalog, face, offset, bytes, sizeof(bytes));
        if (status != RESOURCE_STATUS_OK)
        {
            return status;
        }
        crc = storage_crc32_update(crc, bytes, sizeof(bytes));
        if ((bytes[18] != 0u) || (bytes[19] != 0u))
        {
            return RESOURCE_STATUS_CORRUPT;
        }
        resource_font_a1_record_t record;
        status = resource_store_decode_font_a1_record(bytes, header, face->entry.payload_size, &record);
        if (status != RESOURCE_STATUS_OK)
        {
            return status;
        }
        if (!scalar_valid(record.codepoint) ||
            ((i != 0u) && (record.codepoint <= previous)))
        {
            return RESOURCE_STATUS_CORRUPT;
        }
        if ((record.bearing_y < 0) || (record.bearing_y > header->ascent))
        {
            return RESOURCE_STATUS_CORRUPT;
        }
        previous = record.codepoint;
        have_space = have_space || (record.codepoint == UI_FONT_REQUIRED_SPACE);
        have_unknown = have_unknown || (record.codepoint == UI_FONT_REQUIRED_UNKNOWN);
        if (record.bitmap_size != 0u)
        {
            if (record.bitmap_offset != bitmap_cursor)
            {
                return RESOURCE_STATUS_CORRUPT;
            }
            bitmap_cursor += record.bitmap_size;
        }
    }
    if (((crc ^ STORAGE_CRC32_XOR_OUT) != header->index_crc32) ||
        !have_space ||
        !have_unknown ||
        ((header->bitmap_offset + bitmap_cursor) != face->entry.payload_size))
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    return RESOURCE_STATUS_OK;
}

static resource_status_t mount_face(ui_font_catalog_t *catalog, ui_font_role_t role)
{
    ui_font_face_t *const face = &catalog->faces[(uint8_t)role];
    const uint32_t resource_id = role_resource_id(role);
    resource_status_t status = resource_catalog_lookup(catalog->resource_catalog, resource_id, &face->entry);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    if ((face->entry.resource_type != RESOURCE_TYPE_FONT_BITMAP_A1) ||
        (face->entry.format != RESOURCE_FORMAT_FONT_BITMAP_A1_V1))
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    status = resource_catalog_verify_payload(catalog->resource_catalog, &face->entry);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    uint8_t header_bytes[RESOURCE_FONT_A1_HEADER_SIZE];
    status = read_face(catalog, face, 0u, header_bytes, sizeof(header_bytes));
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    resource_font_a1_header_t header;
    status = resource_store_decode_font_a1_header(header_bytes, face->entry.payload_size, &header);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    face->glyph_count = header.glyph_count;
    face->ascent = (uint8_t)header.ascent;
    face->descent = (uint8_t)header.descent;
    face->line_height = header.line_height;
    face->index_offset = header.index_offset;
    face->bitmap_offset = header.bitmap_offset;
    status = validate_face_index(catalog, face, &header);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    face->ready = true;
    return RESOURCE_STATUS_OK;
}

void ui_font_catalog_init(ui_font_catalog_t *catalog)
{
    if (catalog != NULL)
    {
        *catalog = (ui_font_catalog_t){0};
    }
}

resource_status_t ui_font_catalog_mount(ui_font_catalog_t *catalog,
                                        resource_catalog_t *resource_catalog)
{
    if ((catalog == NULL) || (resource_catalog == NULL))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    *catalog = (ui_font_catalog_t){
        .resource_catalog = resource_catalog,
    };
    for (uint8_t role = 0u; role < (uint8_t)UI_FONT_ROLE_COUNT; role++)
    {
        const resource_status_t status = mount_face(catalog, (ui_font_role_t)role);
        if (status != RESOURCE_STATUS_OK)
        {
            catalog->ready = false;
            return status;
        }
    }
    catalog->ready = true;
    return RESOURCE_STATUS_OK;
}

bool ui_font_catalog_ready(const ui_font_catalog_t *catalog)
{
    return (catalog != NULL) && catalog->ready;
}

resource_status_t ui_font_lookup_glyph(const ui_font_catalog_t *catalog,
                                       ui_font_role_t role,
                                       uint32_t codepoint,
                                       ui_font_glyph_t *glyph)
{
    if ((catalog == NULL) || !catalog->ready || (glyph == NULL) ||
        (role >= UI_FONT_ROLE_COUNT) || !catalog->faces[(uint8_t)role].ready ||
        !scalar_valid(codepoint))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    const ui_font_face_t *const face = &catalog->faces[(uint8_t)role];
    uint16_t lo = 0u;
    uint16_t hi = face->glyph_count;
    uint8_t bytes[RESOURCE_FONT_A1_RECORD_SIZE];
    while (lo < hi)
    {
        const uint16_t mid = (uint16_t)(lo + ((hi - lo) / 2u));
        resource_font_a1_record_t record;
        const resource_status_t status = read_record(catalog, face, mid, &record, bytes);
        if (status != RESOURCE_STATUS_OK)
        {
            return status;
        }
        if (record.codepoint == codepoint)
        {
            glyph_from_record(&record, glyph);
            return RESOURCE_STATUS_OK;
        }
        if (record.codepoint < codepoint)
        {
            lo = (uint16_t)(mid + 1u);
        }
        else
        {
            hi = mid;
        }
    }
    return RESOURCE_STATUS_NOT_FOUND;
}

resource_status_t ui_font_read_glyph_bitmap(const ui_font_catalog_t *catalog,
                                            ui_font_role_t role,
                                            const ui_font_glyph_t *glyph,
                                            uint8_t *dst,
                                            size_t capacity)
{
    if ((catalog == NULL) || !catalog->ready || (role >= UI_FONT_ROLE_COUNT) ||
        (glyph == NULL) || (dst == NULL))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    if (glyph->bitmap_size == 0u)
    {
        return RESOURCE_STATUS_OK;
    }
    if ((size_t)glyph->bitmap_size > capacity)
    {
        return RESOURCE_STATUS_OUT_OF_RANGE;
    }
    const ui_font_face_t *const face = &catalog->faces[(uint8_t)role];
    return read_face(catalog,
                     face,
                     face->bitmap_offset + glyph->bitmap_offset,
                     dst,
                     glyph->bitmap_size);
}

void ui_font_text_start(ui_font_text_op_t *op,
                        ui_font_catalog_t *catalog,
                        ui_font_role_t role,
                        uint16_t x,
                        uint16_t y,
                        const char *text,
                        uint16_t fg_rgb565,
                        uint16_t bg_rgb565)
{
    if (op == NULL)
    {
        return;
    }
    *op = (ui_font_text_op_t){0};
    if ((catalog == NULL) || !catalog->ready || (role >= UI_FONT_ROLE_COUNT) || (text == NULL))
    {
        return;
    }
    size_t i = 0u;
    while ((text[i] != '\0') && ((i + 1u) < sizeof(op->text)))
    {
        op->text[i] = text[i];
        i++;
    }
    op->text[i] = '\0';
    op->catalog = catalog;
    op->role = role;
    op->x = x;
    op->y = y;
    op->cursor_x = x;
    op->fg_rgb565 = fg_rgb565;
    op->bg_rgb565 = bg_rgb565;
    op->active = op->text[0] != '\0';
}

bsp_status_t ui_font_text_step(const ili9341_t *display, ui_font_text_op_t *op)
{
    if ((display == NULL) || (op == NULL) || !op->active || (op->catalog == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (op->text[op->byte_index] == '\0')
    {
        op->active = false;
        return BSP_STATUS_OK;
    }
    uint32_t codepoint = 0u;
    size_t offset = op->byte_index;
    const size_t length = strlen(op->text);
    ui_utf8_status_t decode = ui_utf8_decode_next(op->text, length, &offset, &codepoint);
    if (decode != UI_UTF8_STATUS_OK)
    {
        codepoint = UI_FONT_REQUIRED_UNKNOWN;
        offset = (size_t)op->byte_index + 1u;
    }
    ui_font_glyph_t glyph;
    resource_status_t status = ui_font_lookup_glyph(op->catalog, op->role, codepoint, &glyph);
    if (status == RESOURCE_STATUS_NOT_FOUND)
    {
        status = ui_font_lookup_glyph(op->catalog, op->role, UI_FONT_REQUIRED_UNKNOWN, &glyph);
    }
    if (status != RESOURCE_STATUS_OK)
    {
        return bsp_from_resource(status);
    }

    uint8_t bitmap[(RESOURCE_FONT_A1_MAX_GLYPH_WIDTH / 8u) * RESOURCE_FONT_A1_MAX_GLYPH_HEIGHT];
    uint16_t pixels[RESOURCE_FONT_A1_MAX_GLYPH_WIDTH];
    status = ui_font_read_glyph_bitmap(op->catalog, op->role, &glyph, bitmap, sizeof(bitmap));
    if (status != RESOURCE_STATUS_OK)
    {
        return bsp_from_resource(status);
    }
    for (uint8_t row = 0u; row < glyph.height; row++)
    {
        const uint32_t row_base = (uint32_t)row * glyph.row_stride;
        for (uint8_t col = 0u; col < glyph.width; col++)
        {
            const uint8_t byte = bitmap[row_base + ((uint32_t)col / 8u)];
            const uint8_t mask = (uint8_t)(1u << (7u - (col & 7u)));
            pixels[col] = ((byte & mask) != 0u) ? op->fg_rgb565 : op->bg_rgb565;
        }
        bsp_status_t draw_status =
            ili9341_set_window(display,
                               (uint16_t)(op->cursor_x + (uint16_t)glyph.bearing_x),
                               (uint16_t)(op->y + (uint16_t)(op->catalog->faces[(uint8_t)op->role].ascent -
                                                             (uint8_t)glyph.bearing_y) + row),
                               glyph.width,
                               1u);
        if (draw_status != BSP_STATUS_OK)
        {
            return draw_status;
        }
        draw_status = ili9341_write_pixels_rgb565(pixels, glyph.width);
        if (draw_status != BSP_STATUS_OK)
        {
            return draw_status;
        }
    }
    op->cursor_x = (uint16_t)(op->cursor_x + (uint16_t)glyph.advance_x);
    op->byte_index = (uint8_t)offset;
    if ((op->byte_index >= (sizeof(op->text) - 1u)) || (op->text[op->byte_index] == '\0'))
    {
        op->active = false;
        return BSP_STATUS_OK;
    }
    return BSP_STATUS_BUSY;
}

uint32_t ui_font_catalog_context_size_bytes(void)
{
    return (uint32_t)sizeof(ui_font_catalog_t);
}

uint32_t ui_font_face_size_bytes(void)
{
    return (uint32_t)sizeof(ui_font_face_t);
}

uint32_t ui_font_glyph_size_bytes(void)
{
    return (uint32_t)sizeof(ui_font_glyph_t);
}

uint32_t ui_font_text_op_size_bytes(void)
{
    return (uint32_t)sizeof(ui_font_text_op_t);
}
