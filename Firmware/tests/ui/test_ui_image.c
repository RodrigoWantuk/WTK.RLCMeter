#include "ui/ui_image.h"
#include "ui/ui_boot_splash.h"

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
static uint32_t g_write_calls;
static uint16_t g_window_x;
static uint16_t g_window_y;
static uint16_t g_window_w;
static uint16_t g_window_h;
static bool g_defer_command_read;
static bool g_defer_pixel_write;
static bool g_splash_fixture;
static uint32_t g_clear_calls;
static size_t g_max_read;
static bool g_defer_all_reads;

void ili9341_fill_start(ili9341_fill_t *fill, uint16_t x, uint16_t y,
                        uint16_t width, uint16_t height, uint16_t color)
{
    *fill = (ili9341_fill_t){.x=x, .y=y, .width=width, .height=height,
        .color_rgb565=color, .remaining_pixels=(uint32_t)width*height, .active=true};
}

bsp_status_t ili9341_fill_step(const ili9341_t *display, ili9341_fill_t *fill, uint16_t max_pixels)
{
    (void)display;
    g_clear_calls++;
    const uint32_t count = fill->remaining_pixels < max_pixels ? fill->remaining_pixels : max_pixels;
    fill->remaining_pixels -= count;
    fill->active = fill->remaining_pixels != 0u;
    return fill->active ? BSP_STATUS_BUSY : BSP_STATUS_OK;
}

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
    if (g_defer_pixel_write)
    {
        g_defer_pixel_write = false;
        return BSP_STATUS_BUSY;
    }
    if ((pixels == NULL) && (count != 0u))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    g_pixels_written += (uint32_t)count;
    g_write_calls++;
    return BSP_STATUS_OK;
}

bsp_status_t mem_read(uint32_t address, void *dst, size_t size, void *user)
{
    if (size > g_max_read) g_max_read = size;
    if (g_defer_all_reads) return BSP_STATUS_BUSY;
    if (g_defer_command_read &&
        (address >= RESOURCE_PACK_HEADER_SIZE + RESOURCE_PACK_ENTRY_WIRE_SIZE +
                        RESOURCE_IMAGE_RGB565_RLE_HEADER_SIZE))
    {
        g_defer_command_read = false;
        return BSP_STATUS_BUSY;
    }
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
    put_u16(&commands[0], g_splash_fixture ? 30000u : 2u);
    put_u16(&commands[2], 0xF800u);
    put_u16(&commands[4], g_splash_fixture ? 30000u : 4u);
    put_u16(&commands[6], 0x07E0u);
    put_u16(&commands[8], g_splash_fixture ? 16800u : 2u);
    put_u16(&commands[10], 0x001Fu);
    resource_image_rgb565_rle_header_t header = {
        .magic = RESOURCE_IMAGE_RGB565_RLE_MAGIC,
        .version = RESOURCE_IMAGE_RGB565_RLE_VERSION,
        .header_size = RESOURCE_IMAGE_RGB565_RLE_HEADER_SIZE,
        .width = g_splash_fixture ? 240u : 4u,
        .height = g_splash_fixture ? 320u : 2u,
        .command_offset = RESOURCE_IMAGE_RGB565_RLE_HEADER_SIZE,
        .command_size = sizeof(commands),
        .decoded_pixel_count = g_splash_fixture ? 76800u : 8u,
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
        .resource_id = include_image ? (g_splash_fixture ? RESOURCE_ID_IMAGE_BOOT_SPLASH : RESOURCE_ID_IMAGE_SCREEN_BOOT) : RESOURCE_ID_TEXT_EN,
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

static int test_mount_and_render_boot(void)
{
    int failures = 0;
    g_pixels_written = 0u;
    g_write_calls = 0u;
    build_pack(true, false);
    ui_image_catalog_t images;
    failures += expect_true(mount_image_catalog(&images) == RESOURCE_STATUS_OK, "image catalog mounts");
    failures += expect_true(ui_image_catalog_ready(&images), "image catalog reports ready");
    failures += expect_true(ui_image_catalog_boot_ready(&images), "boot artwork reports ready");

    ui_image_rle_op_t op;
    ui_image_rle_start(&op, &images, RESOURCE_ID_IMAGE_SCREEN_BOOT, 10u, 20u);
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
    failures += expect_true(g_write_calls == 3u, "adjacent RLE runs share bounded TFT writes");
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
    ui_image_rle_start(&op, &images, RESOURCE_ID_IMAGE_SCREEN_BOOT, 0u, 0u);
    failures += expect_true(!op.active, "missing optional image does not start render");
    return failures;
}

static int test_deferred_transport_does_not_advance_rle(void)
{
    int failures = 0;
    g_pixels_written = 0u;
    g_write_calls = 0u;
    build_pack(true, false);
    ui_image_catalog_t images;
    failures += expect_true(mount_image_catalog(&images) == RESOURCE_STATUS_OK,
                            "deferred image catalog mounts");
    ui_image_rle_op_t op;
    ui_image_rle_start(&op, &images, RESOURCE_ID_IMAGE_SCREEN_BOOT, 0u, 0u);
    ili9341_t display = {.ready = true};
    g_defer_command_read = true;
    failures += expect_true(ui_image_rle_step(&display, &op, 8u) == BSP_STATUS_BUSY,
                            "deferred W25Q read yields busy");
    failures += expect_true((op.pixels_remaining == 8u) && (g_pixels_written == 0u),
                            "deferred W25Q read preserves RLE cursor");
    g_defer_pixel_write = true;
    failures += expect_true(ui_image_rle_step(&display, &op, 8u) == BSP_STATUS_BUSY,
                            "deferred TFT write yields busy");
    failures += expect_true((op.pixels_remaining == 8u) && (g_pixels_written == 0u),
                            "deferred TFT write preserves RLE cursor");
    failures += expect_true(ui_image_rle_step(&display, &op, 8u) == BSP_STATUS_OK,
                            "image resumes after transport deferral");
    failures += expect_true(!op.active && (g_pixels_written == 8u) && (g_write_calls == 1u),
                            "image resumes without duplicate or skipped pixels");
    return failures;
}

static int test_corrupt_image_rejected(void)
{
    int failures = 0;
    build_pack(true, true);
    ui_image_catalog_t images;
    failures += expect_true(mount_image_catalog(&images) == RESOURCE_STATUS_OK,
                            "corrupt optional art does not block required resources");
    failures += expect_true(!ui_image_catalog_boot_ready(&images), "bad image command CRC disables art");
    return failures;
}

static int test_shared_splash(void)
{
    int failures = 0;
    const resource_catalog_io_t io = {.read=mem_read, .user=g_pack};
    ili9341_t display = {.ready=true, .init_state=ILI9341_INIT_READY};
    ui_boot_splash_t splash;
    /* Wrap the millisecond counter during the full-image hold. */
    const uint32_t start = UINT32_MAX - 2500u;
    g_splash_fixture = true;
    build_pack(true, false);
    g_pixels_written = 0u;
    g_clear_calls = 0u;
    g_max_read = 0u;
    (void)memset(&splash, 0xA5, sizeof(splash));
    ui_boot_splash_start(&splash, &io, 0u, PACK_BYTES, start);
    failures += expect_true(ui_boot_splash_step(&splash, &display, start, false), "splash mounts");
    failures += expect_true(g_pixels_written == 0u, "no pixels before validation");
    g_defer_all_reads = true;
    failures += expect_true(ui_boot_splash_step(&splash, &display, start+1u, false) &&
        splash.validation.cursor == 0u, "deferred validation preserves cursor and yields");
    g_defer_all_reads = false;
    failures += expect_true(ui_boot_splash_step(&splash, &display, start+1u, false), "splash validates");
    failures += expect_true(g_pixels_written == 0u && splash.state == UI_BOOT_SPLASH_DRAW,
                            "verified splash admitted without early pixels");
    for (uint32_t i=0u; i<1300u && splash.state == UI_BOOT_SPLASH_DRAW; i++)
        (void)ui_boot_splash_step(&splash, &display, start+2u+i, false);
    failures += expect_true(g_pixels_written == 76800u && splash.state == UI_BOOT_SPLASH_HOLD_START,
                            "full portrait streamed in bounded chunks");
    const uint32_t visible = start+1400u;
    (void)ui_boot_splash_step(&splash, &display, visible, false);
    failures += expect_true(ui_boot_splash_step(&splash, &display, visible+2999u, false) &&
                            splash.state == UI_BOOT_SPLASH_HOLD && g_clear_calls == 0u,
                            "complete image retained for at least 3000ms across wrap");
    (void)ui_boot_splash_step(&splash, &display, visible+3000u, false);
    failures += expect_true(splash.state == UI_BOOT_SPLASH_CLEAR, "hold ends at 3000ms");
    for (uint32_t i=0u; i<1300u && splash.state != UI_BOOT_SPLASH_DONE; i++)
        (void)ui_boot_splash_step(&splash, &display, visible+3001u+i, false);
    failures += expect_true(splash.state == UI_BOOT_SPLASH_DONE && g_clear_calls == 1200u,
                            "presentation clears cooperatively then returns control");
    failures += expect_true(g_max_read <= 128u, "resource validation and drawing reads bounded");

    build_pack(true, true);
    g_pixels_written = 0u;
    ui_boot_splash_start(&splash, &io, 0u, PACK_BYTES, 0u);
    (void)ui_boot_splash_step(&splash, &display, 0u, false);
    failures += expect_true(!ui_boot_splash_step(&splash, &display, 1u, false) && g_pixels_written == 0u,
                            "corrupt command CRC skips optional art before pixels");
    build_pack(false, false);
    ui_boot_splash_start(&splash, &io, 0u, PACK_BYTES, 0u);
    failures += expect_true(!ui_boot_splash_step(&splash, &display, 0u, false), "missing splash skips");
    build_pack(true, false);
    ui_boot_splash_start(&splash, &io, 0u, PACK_BYTES, 0u);
    display.ready = false;
    failures += expect_true(ui_boot_splash_step(&splash, &display, 14999u, false), "display wait bounded");
    failures += expect_true(!ui_boot_splash_step(&splash, &display, 15000u, false), "display timeout exits");
    display.ready = true;
    ui_boot_splash_start(&splash, &io, 0u, PACK_BYTES, 0u);
    failures += expect_true(!ui_boot_splash_step(&splash, &display, 0u, true), "safety abort exits immediately");
    ui_boot_splash_start(&splash, &io, 0u, PACK_BYTES, 0u);
    display.init_state = ILI9341_INIT_ERROR;
    failures += expect_true(!ui_boot_splash_step(&splash, &display, 0u, false), "display error exits");
    g_splash_fixture = false;
    return failures;
}

int main(void)
{
    g_failures += test_mount_and_render_boot();
    g_failures += test_missing_image_is_optional();
    g_failures += test_deferred_transport_does_not_advance_rle();
    g_failures += test_corrupt_image_rejected();
    g_failures += test_shared_splash();
    return (g_failures == 0) ? 0 : 1;
}
