#include "bsp/bsp_status.h"

const char *bsp_status_string(bsp_status_t status)
{
    switch (status)
    {
    case BSP_STATUS_OK:
        return "OK";
    case BSP_STATUS_ERROR:
        return "ERROR";
    case BSP_STATUS_TIMEOUT:
        return "TIMEOUT";
    case BSP_STATUS_INVALID_ARG:
        return "INVALID_ARG";
    case BSP_STATUS_BUSY:
        return "BUSY";
    default:
        return "UNKNOWN";
    }
}
