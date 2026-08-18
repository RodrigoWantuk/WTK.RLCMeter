#ifndef WTK_BSP_DIAGNOSTICS_H
#define WTK_BSP_DIAGNOSTICS_H

#include "bsp/bsp_reset.h"
#include "bsp/bsp_status.h"

typedef enum
{
    BSP_LOG_LEVEL_ERROR = 1,
    BSP_LOG_LEVEL_WARN = 2,
    BSP_LOG_LEVEL_INFO = 3,
    BSP_LOG_LEVEL_DEBUG = 4,
    BSP_LOG_LEVEL_TRACE = 5,
} bsp_log_level_t;

void bsp_diagnostics_set_level(bsp_log_level_t level);
void bsp_diagnostics_write(bsp_log_level_t level, const char *message);
void bsp_diagnostics_boot_banner(bsp_reset_reason_t reset_reason, bsp_status_t clock_status);
void bsp_diagnostics_step(void);

#endif
