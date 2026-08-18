#ifndef WTK_BSP_QUIET_H
#define WTK_BSP_QUIET_H

#include <stdbool.h>

void bsp_quiet_request(bool requested);
bool bsp_quiet_requested(void);

#endif
