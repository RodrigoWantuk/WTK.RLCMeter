#ifndef WTK_BSP_RESET_H
#define WTK_BSP_RESET_H

#include <stdint.h>

typedef enum
{
    BSP_RESET_REASON_UNKNOWN = 0,
    BSP_RESET_REASON_POWER_ON,
    BSP_RESET_REASON_PIN,
    BSP_RESET_REASON_SOFTWARE,
    BSP_RESET_REASON_WATCHDOG,
    BSP_RESET_REASON_BROWNOUT_OR_LOW_POWER,
} bsp_reset_reason_t;

bsp_reset_reason_t bsp_reset_capture_reason(void);
bsp_reset_reason_t bsp_reset_get_reason(void);
uint32_t bsp_reset_get_raw_flags(void);
const char *bsp_reset_reason_string(bsp_reset_reason_t reason);

#endif
