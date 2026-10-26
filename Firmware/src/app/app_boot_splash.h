#ifndef WTK_APP_BOOT_SPLASH_H
#define WTK_APP_BOOT_SPLASH_H

#include "drivers/ili9341.h"
#include "drivers/w25q.h"

void app_boot_splash_start(const w25q_device_t *flash, uint32_t now_ms);
bool app_boot_splash_step(const ili9341_t *display, uint32_t now_ms, bool abort_requested);

#endif
