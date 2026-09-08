#include "storage/resource_store.h"
#include "storage/storage_crc32.h"
#include "ui/ui_font.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum
{
    PACK_BYTES = 1024u,
    FONT_GLYPH_COUNT = 5u,
};

static uint8_t g_pack[PACK_BYTES];
static bool g_defer_next_read;
static bool g_fail_next_read;
static int g_failures;

static void expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        g_failures++;
    }
}

bsp_status_t ili9341_set_window(const ili9341_t *display,
                                uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height)
{
    (void)display;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return BSP_STATUS_OK;
}

bsp_status_t ili9341_write_pixels_rgb565(const uint16_t *pixels, size_t count)
{
    (void)pixels;
    (void)count;
    return BSP_STATUS_OK;
}

static bsp_status_t mem_read(uint32_t address, void *dst, size_t size, void *user)
{
    const uint8_t *bytes = (const uint8_t *)user;
    if (g_defer_next_read)
    {
        g_defer_next_read = false;
        return BSP_STATUS_BUSY;
    }
    if (g_fail_next_read)
    {
        g_fail_next_read = false;
        return BSP_STATUS_ERROR;
    }
    if ((bytes == NULL) || (dst == NULL) || (size > PACK_BYTES) || (address > (PACK_BYTES - size)))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    (void)memcpy(dst, &bytes[address], size);
    return BSP_STATUS_OK;
}

static void write_font_record(uint8_t *index,
                              uint16_t slot,
                              uint32_t codepoint,
                              uint32_t bitmap_rel_offset,
                              uint32_t bitmap_abs_offset,
                              uint8_t bitmap)
{
    const bool blank = codepoint == 0x20u;
    const resource_font_a1_record_t record = {
        .codepoint = codepoint,
        .bitmap_offset = blank ? 0u : bitmap_rel_offset,
        .bitmap_size = blank ? 0u : 1u,
        .width = blank ? 0u : 1u,
        .height = blank ? 0u : 1u,
        .advance_x = 2,
        .bearing_x = 0,
        .bearing_y = blank ? 0 : 1,
        .row_stride = blank ? 0u : 1u,
    };
    resource_store_encode_font_a1_record(&index[(uint32_t)slot * RESOURCE_FONT_A1_RECORD_SIZE], &record);
    if (!blank)
    {
        g_pack[bitmap_abs_offset] = bitmap;
    }
}

static uint32_t write_font_payload(uint32_t offset, bool bad_index_crc, bool out_of_order, bool bad_bitmap_bounds)
{
    const uint32_t index_offset = RESOURCE_FONT_A1_HEADER_SIZE;
    const uint32_t bitmap_offset =
        index_offset + (FONT_GLYPH_COUNT * RESOURCE_FONT_A1_RECORD_SIZE);
    uint8_t index[FONT_GLYPH_COUNT * RESOURCE_FONT_A1_RECORD_SIZE];
    (void)memset(index, 0, sizeof(index));
    const uint32_t bitmap_base = offset + bitmap_offset;
    write_font_record(index, 0u, 0x20u, 0u, 0u, 0u);
    write_font_record(index, 1u, out_of_order ? 0x41u : 0x3Fu, 0u, bitmap_base + 0u, 0x80u);
    write_font_record(index, 2u, out_of_order ? 0x3Fu : 0x41u, 1u, bitmap_base + 1u, 0x80u);
    write_font_record(index, 3u, 0x00B5u, 2u, bitmap_base + 2u, 0x80u);
    write_font_record(index, 4u, 0x03A9u, 3u, bitmap_base + 3u, 0x80u);
    if (bad_bitmap_bounds)
    {
        index[RESOURCE_FONT_A1_RECORD_SIZE + 4u] = 0xF0u;
    }
    resource_font_a1_header_t header = {
        .magic = RESOURCE_FONT_A1_MAGIC,
        .version = RESOURCE_FONT_A1_VERSION,
        .header_size = RESOURCE_FONT_A1_HEADER_SIZE,
        .glyph_count = FONT_GLYPH_COUNT,
        .glyph_record_size = RESOURCE_FONT_A1_RECORD_SIZE,
        .ascent = 1,
        .descent = 0,
        .line_height = 2u,
        .index_offset = index_offset,
        .bitmap_offset = bitmap_offset,
    };
    header.index_crc32 = storage_crc32(index, sizeof(index)) ^ (bad_index_crc ? 1u : 0u);
    resource_store_encode_font_a1_header(&g_pack[offset], &header);
    (void)memcpy(&g_pack[offset + index_offset], index, sizeof(index));
    return bitmap_offset + 4u;
}

static void finalize_pack(uint32_t total, uint16_t entry_count)
{
    const uint32_t table_offset = RESOURCE_PACK_HEADER_SIZE;
    resource_pack_header_t header = {
        .magic = RESOURCE_PACK_MAGIC,
        .schema_version = RESOURCE_PACK_SCHEMA_VERSION,
        .header_size = RESOURCE_PACK_HEADER_SIZE,
        .resource_api_version = RESOURCE_PACK_API_VERSION,
        .total_pack_size = total,
        .entry_count = entry_count,
        .entry_wire_size = RESOURCE_PACK_ENTRY_WIRE_SIZE,
        .entry_table_offset = table_offset,
        .data_offset = table_offset + ((uint32_t)entry_count * RESOURCE_PACK_ENTRY_WIRE_SIZE),
        .entry_table_crc32 = storage_crc32(&g_pack[table_offset],
                                           (uint32_t)entry_count * RESOURCE_PACK_ENTRY_WIRE_SIZE),
    };
    uint8_t header_bytes[RESOURCE_PACK_HEADER_SIZE];
    resource_store_encode_header(header_bytes, &header, false);
    header.header_crc32 = storage_crc32(header_bytes, sizeof(header_bytes));
    resource_store_encode_header(g_pack, &header, true);
}

static void write_entry(uint16_t slot,
                        uint32_t resource_id,
                        uint32_t payload_offset,
                        uint32_t payload_size)
{
    const resource_entry_t entry = {
        .resource_id = resource_id,
        .resource_type = RESOURCE_TYPE_FONT_BITMAP_A1,
        .format = RESOURCE_FORMAT_FONT_BITMAP_A1_V1,
        .payload_offset = payload_offset,
        .payload_size = payload_size,
        .payload_crc32 = storage_crc32(&g_pack[payload_offset], payload_size),
    };
    resource_store_encode_entry(&g_pack[RESOURCE_PACK_HEADER_SIZE +
                                        ((uint32_t)slot * RESOURCE_PACK_ENTRY_WIRE_SIZE)],
                                &entry);
}

static void build_font_pack(bool bad_index_crc, bool out_of_order, bool bad_bitmap_bounds)
{
    (void)memset(g_pack, 0xFF, sizeof(g_pack));
    const uint32_t data_offset = RESOURCE_PACK_HEADER_SIZE + (3u * RESOURCE_PACK_ENTRY_WIRE_SIZE);
    uint32_t cursor = data_offset;
    const uint32_t small_offset = cursor;
    const uint32_t small_size = write_font_payload(cursor, bad_index_crc, out_of_order, bad_bitmap_bounds);
    cursor += small_size;
    const uint32_t medium_offset = cursor;
    const uint32_t medium_size = write_font_payload(cursor, false, false, false);
    cursor += medium_size;
    const uint32_t large_offset = cursor;
    const uint32_t large_size = write_font_payload(cursor, false, false, false);
    cursor += large_size;
    write_entry(0u, RESOURCE_ID_FONT_UI_SMALL, small_offset, small_size);
    write_entry(1u, RESOURCE_ID_FONT_UI_MEDIUM, medium_offset, medium_size);
    write_entry(2u, RESOURCE_ID_FONT_UI_LARGE, large_offset, large_size);
    finalize_pack(cursor, 3u);
}

static resource_status_t mount_catalog(resource_catalog_t *catalog, ui_font_catalog_t *font)
{
    const resource_catalog_io_t io = {.read = mem_read, .user = g_pack};
    resource_status_t status = resource_catalog_mount(catalog, &io, 0u, PACK_BYTES);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    ui_font_catalog_init(font);
    return ui_font_catalog_mount(font, catalog);
}

static void test_valid_mount_lookup_and_bitmap(void)
{
    build_font_pack(false, false, false);
    resource_catalog_t catalog;
    ui_font_catalog_t font;
    expect_true(mount_catalog(&catalog, &font) == RESOURCE_STATUS_OK, "valid font catalog mounts");
    expect_true(ui_font_catalog_ready(&font), "font catalog reports ready");
    ui_font_glyph_t glyph;
    expect_true(ui_font_lookup_glyph(&font, UI_FONT_ROLE_SMALL, 0x03A9u, &glyph) == RESOURCE_STATUS_OK,
                "Omega glyph lookup succeeds");
    expect_true((glyph.width == 1u) && (glyph.height == 1u) && (glyph.advance_x == 2),
                "glyph metrics decode");
    uint8_t bitmap[1] = {0};
    expect_true(ui_font_read_glyph_bitmap(&font, UI_FONT_ROLE_SMALL, &glyph, bitmap, sizeof(bitmap)) ==
                    RESOURCE_STATUS_OK,
                "glyph bitmap reads");
    expect_true(bitmap[0] == 0x80u, "glyph bitmap is preserved");
    expect_true(ui_font_lookup_glyph(&font, UI_FONT_ROLE_SMALL, 0x2603u, &glyph) ==
                    RESOURCE_STATUS_NOT_FOUND,
                "unknown glyph reports not found");
}

static void test_corrupt_fonts_fail_admission(void)
{
    resource_catalog_t catalog;
    ui_font_catalog_t font;
    build_font_pack(true, false, false);
    expect_true(mount_catalog(&catalog, &font) == RESOURCE_STATUS_CORRUPT,
                "bad index CRC rejected");
    build_font_pack(false, true, false);
    expect_true(mount_catalog(&catalog, &font) == RESOURCE_STATUS_CORRUPT,
                "out-of-order index rejected");
    build_font_pack(false, false, true);
    expect_true(mount_catalog(&catalog, &font) == RESOURCE_STATUS_CORRUPT,
                "bad bitmap bounds rejected");
}

static void test_deferred_and_fatal_reads_surface(void)
{
    build_font_pack(false, false, false);
    resource_catalog_t catalog;
    ui_font_catalog_t font;
    g_defer_next_read = true;
    expect_true(mount_catalog(&catalog, &font) == RESOURCE_STATUS_DEFERRED,
                "deferred read surfaces during mount");
    g_fail_next_read = true;
    expect_true(mount_catalog(&catalog, &font) == RESOURCE_STATUS_CORRUPT,
                "fatal read surfaces during mount");
}

int main(void)
{
    test_valid_mount_lookup_and_bitmap();
    test_corrupt_fonts_fail_admission();
    test_deferred_and_fatal_reads_surface();
    return (g_failures == 0) ? 0 : 1;
}
