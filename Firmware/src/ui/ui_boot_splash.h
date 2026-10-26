#ifndef WTK_UI_BOOT_SPLASH_H
#define WTK_UI_BOOT_SPLASH_H

#include "ui/ui_image.h"

enum { UI_BOOT_SPLASH_HOLD_MS = 3000u, UI_BOOT_SPLASH_PROGRESS_TIMEOUT_MS = 15000u };

typedef enum
{
    UI_BOOT_SPLASH_DONE = 0,
    UI_BOOT_SPLASH_WAIT_DISPLAY,
    UI_BOOT_SPLASH_VALIDATE,
    UI_BOOT_SPLASH_DRAW,
    UI_BOOT_SPLASH_HOLD_START,
    UI_BOOT_SPLASH_HOLD,
    UI_BOOT_SPLASH_CLEAR
} ui_boot_splash_state_t;

typedef struct
{
    resource_catalog_t resources;
    ui_image_catalog_t images;
    ui_image_rle_op_t image;
    ui_image_validation_t validation;
    ili9341_fill_t clear;
    uint32_t started_ms;
    uint32_t visible_ms;
    uint32_t partition_start;
    uint32_t partition_size;
    resource_catalog_io_t io;
    ui_boot_splash_state_t state;
} ui_boot_splash_t;

void ui_boot_splash_start(ui_boot_splash_t *splash, const resource_catalog_io_t *io,
                          uint32_t partition_start, uint32_t partition_size, uint32_t now_ms);
/* True holds normal application work. Safety/sensors/watchdog remain caller-owned. */
bool ui_boot_splash_step(ui_boot_splash_t *splash, const ili9341_t *display,
                         uint32_t now_ms, bool abort_requested);

#endif
