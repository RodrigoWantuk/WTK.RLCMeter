#ifndef WTK_W25Q_H
#define WTK_W25Q_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_status.h"

enum
{
    W25Q_PAGE_SIZE = 256u,
    W25Q_SECTOR_SIZE = 4096u,
    W25Q_BRINGUP_TEST_SECTOR_OFFSET_FROM_END = W25Q_SECTOR_SIZE,
};

typedef enum
{
    W25Q_STATUS_OK = 0,
    W25Q_STATUS_BUSY,
    W25Q_STATUS_ERROR,
    W25Q_STATUS_TIMEOUT,
    W25Q_STATUS_INVALID_ARG,
    W25Q_STATUS_UNSUPPORTED_DEVICE,
    W25Q_STATUS_OUT_OF_RANGE,
} w25q_status_t;

typedef struct
{
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity_code;
} w25q_jedec_id_t;

typedef struct
{
    w25q_jedec_id_t jedec;
    uint32_t capacity_bytes;
    const char *name;
} w25q_part_info_t;

typedef enum
{
    W25Q_OPERATION_NONE = 0,
    W25Q_OPERATION_PAGE_PROGRAM,
    W25Q_OPERATION_SECTOR_ERASE,
} w25q_operation_t;

typedef struct
{
    bool detected;
    w25q_part_info_t part;
    w25q_operation_t operation;
    uint32_t operation_address;
    uint32_t operation_started_ms;
    uint32_t operation_timeout_ms;
} w25q_device_t;

w25q_status_t w25q_decode_jedec(w25q_jedec_id_t jedec, w25q_part_info_t *part);
bool w25q_range_valid(uint32_t capacity_bytes, uint32_t address, size_t size);
size_t w25q_page_program_span(uint32_t address, size_t requested);
uint32_t w25q_reserved_test_sector_address(uint32_t capacity_bytes);
const char *w25q_status_string(w25q_status_t status);

void w25q_device_init(w25q_device_t *device);
w25q_status_t w25q_device_probe(w25q_device_t *device);
w25q_status_t w25q_device_read_status(uint8_t *status);
w25q_status_t w25q_device_read(const w25q_device_t *device, uint32_t address, void *dst, size_t size);
w25q_status_t w25q_device_fast_read(const w25q_device_t *device, uint32_t address, void *dst, size_t size);
w25q_status_t w25q_device_page_program_start(w25q_device_t *device,
                                             uint32_t address,
                                             const void *src,
                                             size_t size,
                                             uint32_t now_ms);
w25q_status_t w25q_device_sector_erase_start(w25q_device_t *device, uint32_t address, uint32_t now_ms);
w25q_status_t w25q_device_poll(w25q_device_t *device, uint32_t now_ms);

#endif
