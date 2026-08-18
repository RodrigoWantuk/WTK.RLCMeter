#ifndef WTK_BSP_WATCHDOG_H
#define WTK_BSP_WATCHDOG_H

#include <stdbool.h>

#include "bsp/bsp_status.h"

bsp_status_t bsp_watchdog_start(void);
void bsp_watchdog_service(void);
bool bsp_watchdog_is_started(void);

#endif
