#ifndef WTK_STORAGE_ASYNC_IO_H
#define WTK_STORAGE_ASYNC_IO_H
#include <stddef.h>
#include <stdint.h>
#include "bsp/bsp_status.h"
/* Explicit accepted-start contract: OK means owned and poll is required;
 * BUSY means not accepted. This differs from the historical OSL-store adapter. */
typedef struct
{
    bsp_status_t (*read)(uint32_t address, void *dst, size_t size, void *user);
    bsp_status_t (*erase_sector_start)(uint32_t address, uint32_t now_ms, void *user);
    bsp_status_t (*program_start)(uint32_t address, const void *src, size_t size, uint32_t now_ms, void *user);
    bsp_status_t (*poll)(uint32_t now_ms, void *user);
    void *user;
} storage_async_io_t;
#endif
