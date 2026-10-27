#include "storage/app_settings_w25q_adapter.h"

#include "storage/measurement_cal_w25q_adapter.h"

static bsp_status_t adapter_read(uint32_t address, void *dst, size_t size, void *user)
{
    return measurement_cal_w25q_status_to_bsp(
        w25q_device_read((const w25q_device_t *)user, address, dst, size));
}

static bsp_status_t adapter_erase(uint32_t address, uint32_t now_ms, void *user)
{
    w25q_device_t *device = user;
    if (device == NULL) return BSP_STATUS_INVALID_ARG;
    if (device->operation != W25Q_OPERATION_NONE) return BSP_STATUS_BUSY;
    const w25q_status_t status = w25q_device_sector_erase_start(device, address, now_ms);
    /* Settings start returns OK only when this call acquired the operation.
     * Transport BUSY without a newly started operation must remain retryable. */
    if (status == W25Q_STATUS_BUSY && device->operation == W25Q_OPERATION_SECTOR_ERASE &&
        device->operation_address == address) return BSP_STATUS_OK;
    return measurement_cal_w25q_status_to_bsp(status);
}

static bsp_status_t adapter_program(uint32_t address,
                                    const void *src,
                                    size_t size,
                                    uint32_t now_ms,
                                    void *user)
{
    w25q_device_t *device = user;
    if (device == NULL) return BSP_STATUS_INVALID_ARG;
    if (device->operation != W25Q_OPERATION_NONE) return BSP_STATUS_BUSY;
    const w25q_status_t status = w25q_device_page_program_start(device, address, src, size, now_ms);
    if (status == W25Q_STATUS_BUSY && device->operation == W25Q_OPERATION_PAGE_PROGRAM &&
        device->operation_address == address) return BSP_STATUS_OK;
    return measurement_cal_w25q_status_to_bsp(status);
}

static bsp_status_t adapter_poll(uint32_t now_ms, void *user)
{
    return measurement_cal_w25q_status_to_bsp(w25q_device_poll((w25q_device_t *)user, now_ms));
}

app_settings_store_io_t app_settings_w25q_store_io(w25q_device_t *flash)
{
    const app_settings_store_io_t io = {
        .read = adapter_read,
        .erase_sector_start = adapter_erase,
        .program_start = adapter_program,
        .poll = adapter_poll,
        .user = flash,
    };
    return io;
}
