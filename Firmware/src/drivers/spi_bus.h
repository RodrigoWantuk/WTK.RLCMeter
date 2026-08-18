#ifndef WTK_SPI_BUS_H
#define WTK_SPI_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_status.h"

typedef enum
{
    SPI_BUS_DEVICE_W25Q = 0,
    SPI_BUS_DEVICE_ILI9341,
} spi_bus_device_t;

bsp_status_t spi_bus_init(void);
bool spi_bus_quiet_requested(void);
bsp_status_t spi_bus_acquire(spi_bus_device_t device);
bsp_status_t spi_bus_transfer(const uint8_t *tx, uint8_t *rx, size_t length, uint32_t timeout_ms);
bsp_status_t spi_bus_release(spi_bus_device_t device);

#endif
