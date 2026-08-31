#include "storage/resource_w25q_adapter.h"

static bsp_status_t read_resource(uint32_t address, void *dst, size_t size, void *user)
{
    const resource_w25q_reader_t *reader = (const resource_w25q_reader_t *)user;
    if ((reader == NULL) || (reader->flash == NULL) || (dst == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (reader->policy_snapshot != NULL)
    {
        const app_flash_access_snapshot_t snapshot = reader->policy_snapshot(reader->policy_user);
        if (!app_flash_access_allowed(&snapshot, APP_FLASH_ACCESS_RESOURCE_READ))
        {
            return BSP_STATUS_BUSY;
        }
    }
    switch (w25q_device_read(reader->flash, address, dst, size))
    {
    case W25Q_STATUS_OK:
        return BSP_STATUS_OK;
    case W25Q_STATUS_BUSY:
        return BSP_STATUS_BUSY;
    case W25Q_STATUS_INVALID_ARG:
    case W25Q_STATUS_OUT_OF_RANGE:
        return BSP_STATUS_INVALID_ARG;
    case W25Q_STATUS_UNSUPPORTED_DEVICE:
        return BSP_STATUS_NOT_SUPPORTED;
    case W25Q_STATUS_TIMEOUT:
        return BSP_STATUS_TIMEOUT;
    case W25Q_STATUS_ERROR:
    default:
        return BSP_STATUS_ERROR;
    }
}

resource_catalog_io_t resource_w25q_catalog_io(resource_w25q_reader_t *reader)
{
    const resource_catalog_io_t io = {
        .read = read_resource,
        .user = reader,
    };
    return io;
}
