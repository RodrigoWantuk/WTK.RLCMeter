#ifndef WTK_BSP_GPIO_H
#define WTK_BSP_GPIO_H

#include <stdbool.h>

#include "bsp/bsp_status.h"

bsp_status_t bsp_gpio_init_safe(void);
bool bsp_gpio_swd_preserved(void);

#endif
