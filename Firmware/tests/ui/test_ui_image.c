#include "ui/ui_image.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "storage/storage_crc32.h"

enum
{
    PACK_BYTES = 512u,
};

static int g_failures;
static uint8_t g_pack[PACK_BYTES];
static uint32_t g_pixels_written;
static uint16_t g_window_x;
static uint16_t g_window_y;
static uint16_t g_window_w;
static uint16_t g_window_h;

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

bsp_status_t ili9341_set_window(const ili9341_t *display,
                                uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height)
{
    (void)display;
    g_window_x = x;
    g_window_y = y;
    g_window_w = width;
    g_window_h = height;
    return BSP_STATUS_OK;
}

bsp_status_t ili9341_write_pixels_rgb565(const uint16_t *pixels, size_t count)
{
    if ((pixels == NULL) && (count != 0u))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    g_pixels_written += (uint32_t)count;
    return BSP_STATUS_OK;
}

bsp_status_t mem_read(uint32_t address, void *dst, size_t size, void *user)
{
    const uint8_t *const bytes = (const uint8_t *)user;
    if ((bytes == NULL) || (dst == NULL) || (size > PACK_BYTES) || (address > (PACK_BYTES - size)))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    (void)memcpy(dst, &bytes[address], size);
    return BSP_STATUS_OK;
}

static void put_u16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8u) & 0xFFu);
}

static void write_image_payload(uint32_t offset, bool corrupt_command_crc, uint32_t *size_out)
{
    uint8_t commands[3u * RESOURCE_IMAGE_RGB565_RLE_RECORD_SIZE] = {0};
    put_u16(&commands[0], 2u);
    put_u16(&commands[2], 0xF800u);
    put_u16(&commands[4], 4u);
    put_u16(&commands[6], 0x07E0u);
    put_u16(&commands[8], 2u);
    put_u16(&commands[10], 0x001Fu);
    resource_image_rgb565_rle_header_t header = {
        .magic = RESOURCE_IMAGE_RGB565_RLE_MAGIC,
        .version = RESOURCE_IMAGE_RGB565_RLE_VERSION,
        .header_size = RESOURCE_IMAGE_RGB565_RLE_HEADER_SIZE,
        .width = 4u,
        .height = 2u,
        .command_offset = RESOURCE_IMAGE_RGB565_RLE_HEADER_SIZE,
        .command_size = sizeof(commands),
        .decoded_pixel_count = 8u,
        .command_crc32 = storage_crc32(commands, sizeof(commands)) ^ (corrupt_command_crc ? 1u : 0u),
    };
    resource_store_encode_image_rgb565_rle_header(&g_pack[offset], &header);
    (void)memcpy(&g_pack[offset + RESOURCE_IMAGE_RGB565_RLE_HEADER_SIZE], commands, sizeof(commands));
    *size_out = RESOURCE_IMAGE_RGB565_RLE_HEADER_SIZE + (uint32_t)sizeof(commands);
}

static void build_pack(bool include_image, bool corrupt_command_crc)
{
    (void)memset(g_pack, 0xFF, sizeof(g_pack));
    const uint32_t table_offset = RESOURCE_PACK_HEADER_SIZE;
    const uint32_t data_offset = table_offset + RESOURCE_PACK_ENTRY_WIRE_SIZE;
    uint32_t payload_size = 1u;
    if (include_image)
    {
        write_image_payload(data_offset, corrupt_command_crc, &payload_size);
    }
    else
    {
        g_pack[data_offset] = 0u;
    }
    const resource_entry_t entry = {
        .resource_id = include_image ? RESOURCE_ID_IMAGE_SPLASH : RESOURCE_ID_TEXT_EN,
        .resource_type = include_image ? RESOURCE_TYPE_RGB565_IMAGE : RESOURCE_TYPE_TEXT_TABLE,
        .format = include_image ? RESOURCE_FORMAT_IMAGE_RGB565_RLE_V1 : RESOURCE_FORMAT_TEXT_TABLE_UTF8_V1,
        .payload_offset = data_offset,
        .payload_size = payload_size,
        .payload_crc32 = storage_crc32(&g_pack[data_offset], payload_size),
    };
    resource_store_encode_entry(&g_pack[table_offset], &entry);
    resource_pack_header_t header = {
        .magic = RESOURCE_PACK_MAGIC,
        .schema_version = RESOURCE_PACK_SCHEMA_VERSION,
        .header_size = RESOURCE_PACK_HEADER_SIZE,
        .resource_api_version = RESOURCE_PACK_API_VERSION,
        .total_pack_size = data_offset + payload_size,
        .entry_count = 1u,
        .entry_wire_size = RESOURCE_PACK_ENTRY_WIRE_SIZE,
        .entry_table_offset = table_offset,
        .data_offset = data_offset,
        .entry_table_crc32 = storage_crc32(&g_pack[table_offset], RESOURCE_PACK_ENTRY_WIRE_SIZE),
    };
    uint8_t header_bytes[RESOURCE_PACK_HEADER_SIZE];
    resource_store_encode_header(header_bytes, &header, false);
    header.header_crc32 = storage_crc32(header_bytes, sizeof(header_bytes));
    resource_store_encode_header(g_pack, &header, true);
}

static resource_status_t mount_image_catalog(ui_image_catalog_t *images)
{
    resource_catalog_t catalog;
    static resource_catalog_t stable_catalog;
    const resource_catalog_io_t io = {.read = mem_read, .user = g_pack};
    resource_status_t status = resource_catalog_mount(&catalog, &io, 0u, PACK_BYTES);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    stable_catalog = catalog;
    ui_image_catalog_init(images);
    return ui_image_catalog_mount(images, &stable_catalog);
}

static int test_mount_and_render_splash(void)
{
    int failures = 0;
    build_pack(true, false);
    ui_image_catalog_t images;
    failures += expect_true(mount_image_catalog(&images) == RESOURCE_STATUS_OK, "image catalog mounts");
    failures += expect_true(ui_image_catalog_ready(&images), "image catalog reports ready");
    failures += expect_true(ui_image_catalog_splash_ready(&images), "splash reports ready");

    ui_image_rle_op_t op;
    ui_image_rle_start(&op, &images, RESOURCE_ID_IMAGE_SPLASH, 10u, 20u);
    ili9341_t display = {.ready = true};
    uint32_t guard = 0u;
    while (op.active && (guard < 16u))
    {
        const bsp_status_t status = ui_image_rle_step(&display, &op, 3u);
        failures += expect_true((status == BSP_STATUS_BUSY) || (status == BSP_STATUS_OK),
                                "image step returns busy/ok");
        guard++;
    }
    failures += expect_true(!op.active, "image render drains");
    failures += expect_true(g_pixels_written == 8u, "image render writes decoded pixels");
    failures += expect_true((g_window_x == 10u) && (g_window_y == 20u) &&
                                (g_window_w == 4u) && (g_window_h == 2u),
                            "image render sets expected display window");
    return failures;
}

static int test_missing_image_is_optional(void)
{
    int failures = 0;
    build_pack(false, false);
    ui_image_catalog_t images;
    failures += expect_true(mount_image_catalog(&images) == RESOURCE_STATUS_OK,
                            "missing image catalog is accepted");
    failures += expect_true(!ui_image_catalog_ready(&images), "missing optional image is not ready");
    ui_image_rle_op_t op;
    ui_image_rle_start(&op, &images, RESOURCE_ID_IMAGE_SPLASH, 0u, 0u);
    failures += expect_true(!op.active, "missing optional image does not start render");
    return failures;
}

static int test_corrupt_image_rejected(void)
{
    int failures = 0;
    build_pack(true, true);
    ui_image_catalog_t images;
    failures += expect_true(mount_image_catalog(&images) == RESOURCE_STATUS_CORRUPT,
                            "bad image command CRC is rejected");
    return failures;
}

int main(void)
{
    g_failures += test_mount_and_render_splash();
    g_failures += test_missing_image_is_optional();
    g_failures += test_corrupt_image_rejected();
    return (g_failures == 0) ? 0 : 1;
}
