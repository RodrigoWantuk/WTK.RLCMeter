#include "app/app_boot_splash.h"

#include <stddef.h>
#include "storage/resource_w25q_adapter.h"
#include "storage/storage_layout.h"
#include "ui/ui_boot_splash.h"

/* Boot-only context is shared by all profiles and independent of the runtime UI. */
static ui_boot_splash_t g_boot_splash;
static resource_w25q_reader_t g_boot_reader;

void app_boot_splash_start(const w25q_device_t *flash, uint32_t now_ms)
{
    storage_partition_t partition;
    g_boot_splash.state = UI_BOOT_SPLASH_DONE;
    if (flash == NULL || !flash->detected ||
        !storage_layout_partition(flash->part.capacity_bytes, STORAGE_PARTITION_RESOURCE_PACK, &partition)) return;
    g_boot_reader = (resource_w25q_reader_t){.flash = flash};
    const resource_catalog_io_t io = resource_w25q_catalog_io(&g_boot_reader);
    ui_boot_splash_start(&g_boot_splash, &io, partition.start, partition.size, now_ms);
}

bool app_boot_splash_step(const ili9341_t *display, uint32_t now_ms, bool abort_requested)
{
    return ui_boot_splash_step(&g_boot_splash, display, now_ms, abort_requested);
}
