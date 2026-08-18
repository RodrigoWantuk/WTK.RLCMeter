#include "drivers/spi_bus.h"

#include "bsp/bsp_gpio.h"
#include "bsp/bsp_quiet.h"
#include "bsp/bsp_spi.h"

static bool g_acquired = false;
static spi_bus_device_t g_owner = SPI_BUS_DEVICE_W25Q;

static const bsp_spi_config_t g_mode0_conservative = {
    .prescaler = BSP_SPI_PRESCALER_DIV8,
    .cpol = false,
    .cpha = false,
};

static void deselect_all(void)
{
    (void)bsp_gpio_write_output(BSP_GPIO_OUTPUT_FLASH_CS, true);
    (void)bsp_gpio_write_output(BSP_GPIO_OUTPUT_TFT_CS, true);
}

bsp_status_t spi_bus_init(void)
{
    deselect_all();
    return bsp_spi2_init(&g_mode0_conservative);
}

bool spi_bus_quiet_requested(void)
{
    return bsp_quiet_requested();
}

bsp_status_t spi_bus_acquire(spi_bus_device_t device)
{
    if (bsp_quiet_requested() || g_acquired)
    {
        return BSP_STATUS_BUSY;
    }

    deselect_all();
    (void)bsp_spi2_configure(&g_mode0_conservative);

    switch (device)
    {
    case SPI_BUS_DEVICE_W25Q:
        (void)bsp_gpio_write_output(BSP_GPIO_OUTPUT_FLASH_CS, false);
        break;
    case SPI_BUS_DEVICE_ILI9341:
        (void)bsp_gpio_write_output(BSP_GPIO_OUTPUT_TFT_CS, false);
        break;
    default:
        return BSP_STATUS_INVALID_ARG;
    }

    g_owner = device;
    g_acquired = true;
    return BSP_STATUS_OK;
}

bsp_status_t spi_bus_transfer(const uint8_t *tx, uint8_t *rx, size_t length, uint32_t timeout_ms)
{
    if (!g_acquired)
    {
        return BSP_STATUS_ERROR;
    }

    return bsp_spi2_transfer(tx, rx, length, timeout_ms);
}

bsp_status_t spi_bus_release(spi_bus_device_t device)
{
    if (!g_acquired || (device != g_owner))
    {
        deselect_all();
        g_acquired = false;
        return BSP_STATUS_ERROR;
    }

    deselect_all();
    g_acquired = false;
    return BSP_STATUS_OK;
}
