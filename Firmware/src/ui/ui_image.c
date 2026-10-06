#include "ui/ui_image.h"

#include <stddef.h>

#include "storage/storage_crc32.h"

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

static uint16_t get_u16(const uint8_t bytes[2])
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
}

static resource_status_t read_image(const ui_image_catalog_t *catalog,
                                    const ui_image_resource_t *image,
                                    uint32_t offset,
                                    void *dst,
                                    size_t size)
{
    if ((catalog == NULL) || (image == NULL) || (dst == NULL))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    return resource_catalog_read(catalog->resource_catalog, &image->entry, offset, dst, size);
}

static resource_status_t validate_image_commands(ui_image_catalog_t *catalog,
                                                 ui_image_resource_t *image)
{
    uint8_t bytes[RESOURCE_IMAGE_RGB565_RLE_RECORD_SIZE];
    uint32_t crc = STORAGE_CRC32_INIT;
    uint32_t pixels = 0u;
    uint32_t cursor = 0u;
    while (cursor < image->header.command_size)
    {
        resource_status_t status = read_image(catalog,
                                              image,
                                              image->header.command_offset + cursor,
                                              bytes,
                                              sizeof(bytes));
        if (status != RESOURCE_STATUS_OK)
        {
            return status;
        }
        crc = storage_crc32_update(crc, bytes, sizeof(bytes));
        const uint16_t count = get_u16(&bytes[0]);
        if (count == 0u)
        {
            return RESOURCE_STATUS_CORRUPT;
        }
        pixels += count;
        if (pixels > image->header.decoded_pixel_count)
        {
            return RESOURCE_STATUS_CORRUPT;
        }
        cursor += RESOURCE_IMAGE_RGB565_RLE_RECORD_SIZE;
    }
    if ((pixels != image->header.decoded_pixel_count) ||
        ((crc ^ STORAGE_CRC32_XOR_OUT) != image->header.command_crc32))
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    return RESOURCE_STATUS_OK;
}

static resource_status_t mount_image(ui_image_catalog_t *catalog,
                                     uint32_t resource_id,
                                     ui_image_resource_t *image)
{
    if ((catalog == NULL) || (image == NULL))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    resource_status_t status = resource_catalog_lookup(catalog->resource_catalog, resource_id, &image->entry);
    if (status == RESOURCE_STATUS_NOT_FOUND)
    {
        *image = (ui_image_resource_t){0};
        return RESOURCE_STATUS_OK;
    }
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    if ((image->entry.resource_type != RESOURCE_TYPE_RGB565_IMAGE) ||
        (image->entry.format != RESOURCE_FORMAT_IMAGE_RGB565_RLE_V1))
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    status = resource_catalog_verify_payload(catalog->resource_catalog, &image->entry);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    uint8_t header_bytes[RESOURCE_IMAGE_RGB565_RLE_HEADER_SIZE];
    status = resource_catalog_read(catalog->resource_catalog,
                                   &image->entry,
                                   0u,
                                   header_bytes,
                                   sizeof(header_bytes));
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    status = resource_store_decode_image_rgb565_rle_header(header_bytes,
                                                          image->entry.payload_size,
                                                          &image->header);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    status = validate_image_commands(catalog, image);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    image->ready = true;
    return RESOURCE_STATUS_OK;
}

static const ui_image_resource_t *lookup_image(const ui_image_catalog_t *catalog,
                                               uint32_t resource_id)
{
    if (catalog == NULL)
    {
        return NULL;
    }
    if ((resource_id == RESOURCE_ID_IMAGE_SPLASH) && catalog->splash.ready)
    {
        return &catalog->splash;
    }
    return NULL;
}

void ui_image_catalog_init(ui_image_catalog_t *catalog)
{
    if (catalog != NULL)
    {
        *catalog = (ui_image_catalog_t){0};
    }
}

resource_status_t ui_image_catalog_mount(ui_image_catalog_t *catalog,
                                         resource_catalog_t *resource_catalog)
{
    if ((catalog == NULL) || (resource_catalog == NULL))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    *catalog = (ui_image_catalog_t){
        .resource_catalog = resource_catalog,
    };
    const resource_status_t status = mount_image(catalog, RESOURCE_ID_IMAGE_SPLASH, &catalog->splash);
    if (status != RESOURCE_STATUS_OK)
    {
        catalog->ready = false;
        return status;
    }
    catalog->ready = catalog->splash.ready;
    return RESOURCE_STATUS_OK;
}

bool ui_image_catalog_ready(const ui_image_catalog_t *catalog)
{
    return (catalog != NULL) && catalog->ready;
}

bool ui_image_catalog_splash_ready(const ui_image_catalog_t *catalog)
{
    return (catalog != NULL) && catalog->splash.ready;
}

void ui_image_rle_start(ui_image_rle_op_t *op,
                        const ui_image_catalog_t *catalog,
                        uint32_t resource_id,
                        uint16_t x,
                        uint16_t y)
{
    if (op == NULL)
    {
        return;
    }
    *op = (ui_image_rle_op_t){0};
    const ui_image_resource_t *const image = lookup_image(catalog, resource_id);
    if (image == NULL)
    {
        return;
    }
    op->resource_catalog = catalog->resource_catalog;
    op->entry = image->entry;
    op->header = image->header;
    op->x = x;
    op->y = y;
    op->pixels_remaining = image->header.decoded_pixel_count;
    op->active = op->pixels_remaining != 0u;
}

bsp_status_t ui_image_rle_step(const ili9341_t *display,
                               ui_image_rle_op_t *op,
                               uint16_t max_pixels)
{
    static uint16_t pixels[ILI9341_FILL_CHUNK_PIXELS];
    if ((display == NULL) || (op == NULL) || !op->active || (op->resource_catalog == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!op->window_sent)
    {
        const bsp_status_t status = ili9341_set_window(display,
                                                       op->x,
                                                       op->y,
                                                       op->header.width,
                                                       op->header.height);
        if (status != BSP_STATUS_OK)
        {
            return status;
        }
        op->window_sent = true;
    }
    if (op->run_remaining == 0u)
    {
        uint8_t bytes[RESOURCE_IMAGE_RGB565_RLE_RECORD_SIZE];
        if (op->command_cursor >= op->header.command_size)
        {
            return BSP_STATUS_ERROR;
        }
        const resource_status_t read_status =
            resource_catalog_read(op->resource_catalog,
                                  &op->entry,
                                  op->header.command_offset + op->command_cursor,
                                  bytes,
                                  sizeof(bytes));
        if (read_status != RESOURCE_STATUS_OK)
        {
            return bsp_from_resource(read_status);
        }
        op->run_remaining = get_u16(&bytes[0]);
        op->run_color_rgb565 = get_u16(&bytes[2]);
        op->command_cursor += RESOURCE_IMAGE_RGB565_RLE_RECORD_SIZE;
        if ((op->run_remaining == 0u) || ((uint32_t)op->run_remaining > op->pixels_remaining))
        {
            return BSP_STATUS_ERROR;
        }
    }
    uint16_t chunk = (max_pixels == 0u) ? ILI9341_FILL_CHUNK_PIXELS : max_pixels;
    if (chunk > ILI9341_FILL_CHUNK_PIXELS)
    {
        chunk = ILI9341_FILL_CHUNK_PIXELS;
    }
    if (chunk > op->run_remaining)
    {
        chunk = op->run_remaining;
    }
    if ((uint32_t)chunk > op->pixels_remaining)
    {
        chunk = (uint16_t)op->pixels_remaining;
    }
    for (uint16_t i = 0u; i < chunk; i++)
    {
        pixels[i] = op->run_color_rgb565;
    }
    const bsp_status_t status = ili9341_write_pixels_rgb565(pixels, chunk);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    op->run_remaining = (uint16_t)(op->run_remaining - chunk);
    op->pixels_remaining -= chunk;
    if (op->pixels_remaining == 0u)
    {
        op->active = false;
        return BSP_STATUS_OK;
    }
    return BSP_STATUS_BUSY;
}

uint32_t ui_image_catalog_context_size_bytes(void)
{
    return (uint32_t)sizeof(ui_image_catalog_t);
}

uint32_t ui_image_rle_op_size_bytes(void)
{
    return (uint32_t)sizeof(ui_image_rle_op_t);
}
