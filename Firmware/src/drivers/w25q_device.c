#include "drivers/w25q.h"

#include <stddef.h>
#include <stdint.h>

#include "drivers/spi_bus.h"

enum
{
    W25Q_CMD_READ_JEDEC_ID = 0x9Fu,
    W25Q_CMD_READ_STATUS1 = 0x05u,
    W25Q_CMD_WRITE_ENABLE = 0x06u,
    W25Q_CMD_READ_DATA = 0x03u,
    W25Q_CMD_FAST_READ = 0x0Bu,
    W25Q_CMD_PAGE_PROGRAM = 0x02u,
    W25Q_CMD_SECTOR_ERASE = 0x20u,
    W25Q_STATUS1_BUSY = 0x01u,
    W25Q_SPI_TIMEOUT_MS = 5u,
    W25Q_PAGE_PROGRAM_TIMEOUT_MS = 20u,
    W25Q_SECTOR_ERASE_TIMEOUT_MS = 600u,
};

static w25q_status_t from_bsp_status(bsp_status_t status)
{
    switch (status)
    {
    case BSP_STATUS_OK:
        return W25Q_STATUS_OK;
    case BSP_STATUS_TIMEOUT:
        return W25Q_STATUS_TIMEOUT;
    case BSP_STATUS_INVALID_ARG:
        return W25Q_STATUS_INVALID_ARG;
    case BSP_STATUS_BUSY:
        return W25Q_STATUS_BUSY;
    default:
        return W25Q_STATUS_ERROR;
    }
}

static void put_addr(uint8_t *dst, uint32_t address)
{
    dst[0] = (uint8_t)((address >> 16u) & 0xFFu);
    dst[1] = (uint8_t)((address >> 8u) & 0xFFu);
    dst[2] = (uint8_t)(address & 0xFFu);
}

static w25q_status_t w25q_command_only(uint8_t command)
{
    w25q_status_t status = from_bsp_status(spi_bus_acquire(SPI_BUS_DEVICE_W25Q));
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }

    status = from_bsp_status(spi_bus_transfer(&command, NULL, 1u, W25Q_SPI_TIMEOUT_MS));
    const w25q_status_t release_status = from_bsp_status(spi_bus_release(SPI_BUS_DEVICE_W25Q));

    return (status == W25Q_STATUS_OK) ? release_status : status;
}

static w25q_status_t w25q_write_enable(void)
{
    return w25q_command_only(W25Q_CMD_WRITE_ENABLE);
}

void w25q_device_init(w25q_device_t *device)
{
    if (device == NULL)
    {
        return;
    }

    device->detected = false;
    device->part.jedec.manufacturer_id = 0u;
    device->part.jedec.memory_type = 0u;
    device->part.jedec.capacity_code = 0u;
    device->part.capacity_bytes = 0u;
    device->part.name = "unknown";
    device->operation = W25Q_OPERATION_NONE;
    device->operation_address = 0u;
    device->operation_started_ms = 0u;
    device->operation_timeout_ms = 0u;
}

w25q_status_t w25q_device_probe(w25q_device_t *device)
{
    if (device == NULL)
    {
        return W25Q_STATUS_INVALID_ARG;
    }

    const uint8_t tx[4] = {W25Q_CMD_READ_JEDEC_ID, 0xFFu, 0xFFu, 0xFFu};
    uint8_t rx[4] = {0u, 0u, 0u, 0u};

    w25q_status_t status = from_bsp_status(spi_bus_acquire(SPI_BUS_DEVICE_W25Q));
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }

    status = from_bsp_status(spi_bus_transfer(tx, rx, sizeof(tx), W25Q_SPI_TIMEOUT_MS));
    const w25q_status_t release_status = from_bsp_status(spi_bus_release(SPI_BUS_DEVICE_W25Q));
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }
    if (release_status != W25Q_STATUS_OK)
    {
        return release_status;
    }

    const w25q_jedec_id_t jedec = {
        .manufacturer_id = rx[1],
        .memory_type = rx[2],
        .capacity_code = rx[3],
    };

    status = w25q_decode_jedec(jedec, &device->part);
    device->detected = (status == W25Q_STATUS_OK);
    return status;
}

w25q_status_t w25q_device_read_status(uint8_t *status_reg)
{
    if (status_reg == NULL)
    {
        return W25Q_STATUS_INVALID_ARG;
    }

    const uint8_t tx[2] = {W25Q_CMD_READ_STATUS1, 0xFFu};
    uint8_t rx[2] = {0u, 0u};

    w25q_status_t status = from_bsp_status(spi_bus_acquire(SPI_BUS_DEVICE_W25Q));
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }

    status = from_bsp_status(spi_bus_transfer(tx, rx, sizeof(tx), W25Q_SPI_TIMEOUT_MS));
    const w25q_status_t release_status = from_bsp_status(spi_bus_release(SPI_BUS_DEVICE_W25Q));
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }
    if (release_status != W25Q_STATUS_OK)
    {
        return release_status;
    }

    *status_reg = rx[1];
    return W25Q_STATUS_OK;
}

static w25q_status_t w25q_read_common(const w25q_device_t *device,
                                      uint8_t command,
                                      uint32_t address,
                                      void *dst,
                                      size_t size)
{
    if ((device == NULL) || ((dst == NULL) && (size > 0u)) || !device->detected)
    {
        return W25Q_STATUS_INVALID_ARG;
    }

    if (!w25q_range_valid(device->part.capacity_bytes, address, size))
    {
        return W25Q_STATUS_OUT_OF_RANGE;
    }

    uint8_t header[5] = {command, 0u, 0u, 0u, 0u};
    put_addr(&header[1], address);
    const size_t header_size = (command == W25Q_CMD_FAST_READ) ? sizeof(header) : 4u;

    w25q_status_t status = from_bsp_status(spi_bus_acquire(SPI_BUS_DEVICE_W25Q));
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }

    status = from_bsp_status(spi_bus_transfer(header, NULL, header_size, W25Q_SPI_TIMEOUT_MS));
    if (status == W25Q_STATUS_OK)
    {
        status = from_bsp_status(spi_bus_transfer(NULL, (uint8_t *)dst, size, W25Q_SPI_TIMEOUT_MS));
    }

    const w25q_status_t release_status = from_bsp_status(spi_bus_release(SPI_BUS_DEVICE_W25Q));
    return (status == W25Q_STATUS_OK) ? release_status : status;
}

w25q_status_t w25q_device_read(const w25q_device_t *device, uint32_t address, void *dst, size_t size)
{
    return w25q_read_common(device, W25Q_CMD_READ_DATA, address, dst, size);
}

w25q_status_t w25q_device_fast_read(const w25q_device_t *device, uint32_t address, void *dst, size_t size)
{
    return w25q_read_common(device, W25Q_CMD_FAST_READ, address, dst, size);
}

w25q_status_t w25q_device_page_program_start(w25q_device_t *device,
                                             uint32_t address,
                                             const void *src,
                                             size_t size,
                                             uint32_t now_ms)
{
    if ((device == NULL) || ((src == NULL) && (size > 0u)) || !device->detected)
    {
        return W25Q_STATUS_INVALID_ARG;
    }

    if ((device->operation != W25Q_OPERATION_NONE) || (size == 0u))
    {
        return W25Q_STATUS_BUSY;
    }

    if (!w25q_range_valid(device->part.capacity_bytes, address, size) ||
        (size > w25q_page_program_span(address, size)))
    {
        return W25Q_STATUS_OUT_OF_RANGE;
    }

    w25q_status_t status = w25q_write_enable();
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }

    uint8_t header[4] = {W25Q_CMD_PAGE_PROGRAM, 0u, 0u, 0u};
    put_addr(&header[1], address);

    status = from_bsp_status(spi_bus_acquire(SPI_BUS_DEVICE_W25Q));
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }

    status = from_bsp_status(spi_bus_transfer(header, NULL, sizeof(header), W25Q_SPI_TIMEOUT_MS));
    if (status == W25Q_STATUS_OK)
    {
        status = from_bsp_status(spi_bus_transfer((const uint8_t *)src, NULL, size, W25Q_SPI_TIMEOUT_MS));
    }

    const w25q_status_t release_status = from_bsp_status(spi_bus_release(SPI_BUS_DEVICE_W25Q));
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }
    if (release_status != W25Q_STATUS_OK)
    {
        return release_status;
    }

    device->operation = W25Q_OPERATION_PAGE_PROGRAM;
    device->operation_address = address;
    device->operation_started_ms = now_ms;
    device->operation_timeout_ms = W25Q_PAGE_PROGRAM_TIMEOUT_MS;
    return W25Q_STATUS_BUSY;
}

w25q_status_t w25q_device_sector_erase_start(w25q_device_t *device, uint32_t address, uint32_t now_ms)
{
    if ((device == NULL) || !device->detected)
    {
        return W25Q_STATUS_INVALID_ARG;
    }

    if (device->operation != W25Q_OPERATION_NONE)
    {
        return W25Q_STATUS_BUSY;
    }

    if (((address % W25Q_SECTOR_SIZE) != 0u) ||
        !w25q_range_valid(device->part.capacity_bytes, address, W25Q_SECTOR_SIZE))
    {
        return W25Q_STATUS_OUT_OF_RANGE;
    }

    w25q_status_t status = w25q_write_enable();
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }

    uint8_t command[4] = {W25Q_CMD_SECTOR_ERASE, 0u, 0u, 0u};
    put_addr(&command[1], address);

    status = from_bsp_status(spi_bus_acquire(SPI_BUS_DEVICE_W25Q));
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }

    status = from_bsp_status(spi_bus_transfer(command, NULL, sizeof(command), W25Q_SPI_TIMEOUT_MS));
    const w25q_status_t release_status = from_bsp_status(spi_bus_release(SPI_BUS_DEVICE_W25Q));
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }
    if (release_status != W25Q_STATUS_OK)
    {
        return release_status;
    }

    device->operation = W25Q_OPERATION_SECTOR_ERASE;
    device->operation_address = address;
    device->operation_started_ms = now_ms;
    device->operation_timeout_ms = W25Q_SECTOR_ERASE_TIMEOUT_MS;
    return W25Q_STATUS_BUSY;
}

w25q_status_t w25q_device_poll(w25q_device_t *device, uint32_t now_ms)
{
    if ((device == NULL) || !device->detected)
    {
        return W25Q_STATUS_INVALID_ARG;
    }

    if (device->operation == W25Q_OPERATION_NONE)
    {
        return W25Q_STATUS_OK;
    }

    uint8_t status_reg = 0u;
    const w25q_status_t status = w25q_device_read_status(&status_reg);
    if (status != W25Q_STATUS_OK)
    {
        return status;
    }

    if ((status_reg & W25Q_STATUS1_BUSY) == 0u)
    {
        device->operation = W25Q_OPERATION_NONE;
        return W25Q_STATUS_OK;
    }

    if ((now_ms - device->operation_started_ms) >= device->operation_timeout_ms)
    {
        device->operation = W25Q_OPERATION_NONE;
        return W25Q_STATUS_TIMEOUT;
    }

    return W25Q_STATUS_BUSY;
}
