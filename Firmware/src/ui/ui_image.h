#ifndef WTK_UI_IMAGE_H
#define WTK_UI_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "drivers/ili9341.h"
#include "storage/resource_store.h"

typedef struct
{
    resource_entry_t entry;
    resource_image_rgb565_rle_header_t header;
    bool ready;
} ui_image_resource_t;

typedef struct
{
    resource_catalog_t *resource_catalog;
    ui_image_resource_t splash;
    bool ready;
} ui_image_catalog_t;

typedef struct
{
    resource_catalog_t *resource_catalog;
    resource_entry_t entry;
    resource_image_rgb565_rle_header_t header;
    uint16_t x;
    uint16_t y;
    uint32_t command_cursor;
    uint32_t pixels_remaining;
    uint16_t run_remaining;
    uint16_t run_color_rgb565;
    bool window_sent;
    bool active;
} ui_image_rle_op_t;

void ui_image_catalog_init(ui_image_catalog_t *catalog);
resource_status_t ui_image_catalog_mount(ui_image_catalog_t *catalog,
                                         resource_catalog_t *resource_catalog);
bool ui_image_catalog_ready(const ui_image_catalog_t *catalog);
bool ui_image_catalog_splash_ready(const ui_image_catalog_t *catalog);
void ui_image_rle_start(ui_image_rle_op_t *op,
                        const ui_image_catalog_t *catalog,
                        uint32_t resource_id,
                        uint16_t x,
                        uint16_t y);
bsp_status_t ui_image_rle_step(const ili9341_t *display,
                               ui_image_rle_op_t *op,
                               uint16_t max_pixels);
uint32_t ui_image_catalog_context_size_bytes(void);
uint32_t ui_image_rle_op_size_bytes(void);

#endif
