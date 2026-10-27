/* Real W25Q driver + settings adapter, with an explicit host SPI emulator. */
#include <assert.h>
#include <string.h>
#include "drivers/spi_bus.h"
#include "storage/app_settings_w25q_adapter.h"

static uint8_t flash_bytes[32768];
static uint8_t command;
static uint32_t offset;
static bool acquired, contended, wip;
static unsigned polls;

bsp_status_t spi_bus_acquire(spi_bus_device_t device)
{
    assert(device == SPI_BUS_DEVICE_W25Q);
    if (contended || acquired) return BSP_STATUS_BUSY;
    acquired = true;
    command = 0u;
    return BSP_STATUS_OK;
}
bsp_status_t spi_bus_release(spi_bus_device_t device)
{
    assert(device == SPI_BUS_DEVICE_W25Q && acquired);
    acquired = false;
    return BSP_STATUS_OK;
}
bsp_status_t spi_bus_transfer(const uint8_t *tx, uint8_t *rx, size_t size, uint32_t timeout)
{
    (void)timeout;
    assert(acquired);
    if (command == 0u && tx != NULL)
    {
        command = tx[0];
        if (command == 5u)
        {
            assert(rx != NULL && size == 2u);
            rx[0] = 0u;
            rx[1] = wip ? 1u : 0u;
            wip = false;
            polls++;
        }
        if (size >= 4u)
        {
            offset = ((uint32_t)tx[1] << 16u) | ((uint32_t)tx[2] << 8u) | tx[3];
            assert(offset < sizeof(flash_bytes));
            if (command == 0x20u)
            {
                assert(offset <= sizeof(flash_bytes) - 4096u);
                memset(flash_bytes + offset, 255, 4096u);
                wip = true;
            }
        }
    }
    else if (command == 3u)
    {
        assert(rx != NULL && offset <= sizeof(flash_bytes) - size);
        memcpy(rx, flash_bytes + offset, size);
    }
    else if (command == 2u)
    {
        assert(tx != NULL && offset <= sizeof(flash_bytes) - size);
        for (size_t i = 0u; i < size; ++i) flash_bytes[offset + i] &= tx[i];
        wip = true;
    }
    return BSP_STATUS_OK;
}

int main(void)
{
    memset(flash_bytes, 255, sizeof(flash_bytes));
    w25q_device_t device;
    w25q_device_init(&device);
    device.detected = true;
    device.part.capacity_bytes = sizeof(flash_bytes);
    const app_settings_store_io_t io = app_settings_w25q_store_io(&device);
    app_settings_service_t service, loaded;
    assert(app_settings_service_init(&service, &io, sizeof(flash_bytes)) == BSP_STATUS_OK);
    app_settings_t value = app_settings_defaults();
    value.brightness_percent = 60u;
    assert(app_settings_service_set(&service, &value) == BSP_STATUS_OK);
    assert(app_settings_service_save_start(&service, 0u) == BSP_STATUS_OK);
    contended = true;
    assert(app_settings_service_step(&service, 1u) == BSP_STATUS_BUSY);
    assert(service.state == APP_SETTINGS_STORE_ERASE_START && device.operation == W25Q_OPERATION_NONE);
    contended = false;
    assert(app_settings_service_step(&service, 2u) == BSP_STATUS_BUSY);
    assert(service.state == APP_SETTINGS_STORE_ERASE_WAIT && device.operation == W25Q_OPERATION_SECTOR_ERASE);
    assert(io.erase_sector_start(service.slots[1].start, 3u, io.user) == BSP_STATUS_BUSY);
    assert(device.operation_address == service.slots[0].start);
    for (uint32_t now = 3u; now < 100u && app_settings_service_busy(&service); ++now)
        assert(app_settings_service_step(&service, now) == BSP_STATUS_BUSY || !app_settings_service_busy(&service));
    assert(!service.save_failed && service.persisted_valid && polls >= 8u);
    assert(device.operation == W25Q_OPERATION_NONE);
    assert(app_settings_service_init(&loaded, &io, sizeof(flash_bytes)) == BSP_STATUS_OK);
    assert(app_settings_service_load(&loaded, NULL) == BSP_STATUS_OK && loaded.current.brightness_percent == 60u);
    return 0;
}
