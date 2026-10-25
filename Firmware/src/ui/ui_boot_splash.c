#include "ui/ui_boot_splash.h"

#include <stddef.h>

void ui_boot_splash_start(ui_boot_splash_t *splash, const resource_catalog_io_t *io,
                          uint32_t partition_start, uint32_t partition_size, uint32_t now_ms)
{
    if (splash == NULL) return;
    /* Each owned operation initializes its context before admission. */
    splash->state = UI_BOOT_SPLASH_DONE;
    if (io == NULL) return;
    /* resource_catalog_mount validates the callback and partition before I/O. */
    splash->io = *io;
    splash->partition_start = partition_start;
    splash->partition_size = partition_size;
    splash->started_ms = now_ms;
    splash->state = UI_BOOT_SPLASH_WAIT_DISPLAY;
}

bool ui_boot_splash_step(ui_boot_splash_t *splash, const ili9341_t *display,
                         uint32_t now_ms, bool abort_requested)
{
    if (splash == NULL || splash->state == UI_BOOT_SPLASH_DONE) return false;
    if (abort_requested || display == NULL || display->init_state == ILI9341_INIT_ERROR ||
        ((uint32_t)(now_ms - splash->started_ms) >= UI_BOOT_SPLASH_PROGRESS_TIMEOUT_MS &&
         splash->state != UI_BOOT_SPLASH_HOLD))
    {
        splash->state = UI_BOOT_SPLASH_DONE;
        return false;
    }
    bsp_status_t status = BSP_STATUS_BUSY;
    switch (splash->state)
    {
    case UI_BOOT_SPLASH_WAIT_DISPLAY:
    {
        if (!display->ready) return true;
        resource_status_t mounted = resource_catalog_mount(&splash->resources, &splash->io,
                splash->partition_start, splash->partition_size);
        if (mounted == RESOURCE_STATUS_DEFERRED) return true;
        if (mounted == RESOURCE_STATUS_OK)
            mounted = ui_image_catalog_prepare(&splash->images, &splash->resources,
                RESOURCE_ID_IMAGE_BOOT_SPLASH, &splash->validation);
        if (mounted == RESOURCE_STATUS_DEFERRED) return true;
        if (mounted != RESOURCE_STATUS_OK || splash->images.boot.header.width != ILI9341_WIDTH ||
            splash->images.boot.header.height != ILI9341_HEIGHT)
        {
            splash->state = UI_BOOT_SPLASH_DONE;
            return false;
        }
        splash->state = UI_BOOT_SPLASH_VALIDATE;
        return true;
    }
    case UI_BOOT_SPLASH_VALIDATE:
    {
        const resource_status_t checked = ui_image_catalog_validate_step(&splash->images, &splash->validation);
        if (checked == RESOURCE_STATUS_OK)
        {
            ui_image_rle_start(&splash->image, &splash->images, RESOURCE_ID_IMAGE_BOOT_SPLASH, 0u, 0u);
            splash->state = UI_BOOT_SPLASH_DRAW;
        }
        else if (checked != RESOURCE_STATUS_DEFERRED)
        {
            splash->state = UI_BOOT_SPLASH_DONE;
        }
        return splash->state != UI_BOOT_SPLASH_DONE;
    }
    case UI_BOOT_SPLASH_DRAW:
        status = ui_image_rle_step(display, &splash->image, ILI9341_FILL_CHUNK_PIXELS);
        if (status == BSP_STATUS_OK) splash->state = UI_BOOT_SPLASH_HOLD_START;
        break;
    case UI_BOOT_SPLASH_HOLD_START:
        /* Timestamp on the following loop: the last SPI pixel is already sent. */
        splash->visible_ms = now_ms;
        splash->state = UI_BOOT_SPLASH_HOLD;
        return true;
    case UI_BOOT_SPLASH_HOLD:
        if ((uint32_t)(now_ms - splash->visible_ms) < UI_BOOT_SPLASH_HOLD_MS) return true;
        ili9341_fill_start(&splash->clear, 0u, 0u, ILI9341_WIDTH, ILI9341_HEIGHT, 0u);
        splash->started_ms = now_ms;
        splash->state = UI_BOOT_SPLASH_CLEAR;
        return true;
    case UI_BOOT_SPLASH_CLEAR:
        status = ili9341_fill_step(display, &splash->clear, ILI9341_FILL_CHUNK_PIXELS);
        /* OK acknowledges one chunk; the driver's active flag owns completion. */
        if (!splash->clear.active) splash->state = UI_BOOT_SPLASH_DONE;
        break;
    default:
        splash->state = UI_BOOT_SPLASH_DONE;
        return false;
    }
    if (status != BSP_STATUS_OK && status != BSP_STATUS_BUSY) splash->state = UI_BOOT_SPLASH_DONE;
    return splash->state != UI_BOOT_SPLASH_DONE;
}
