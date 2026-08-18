#ifndef WTK_BSP_STATUS_H
#define WTK_BSP_STATUS_H

typedef enum
{
    BSP_STATUS_OK = 0,
    BSP_STATUS_ERROR,
    BSP_STATUS_TIMEOUT,
    BSP_STATUS_INVALID_ARG,
} bsp_status_t;

const char *bsp_status_string(bsp_status_t status);

#endif
