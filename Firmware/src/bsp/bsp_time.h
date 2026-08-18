#ifndef WTK_BSP_TIME_H
#define WTK_BSP_TIME_H

#include <stdint.h>

#include "bsp/bsp_status.h"

bsp_status_t bsp_time_init(void);
uint32_t bsp_time_now_ms(void);

#endif
