#include "drivers/spi_bus.h"

#include "bsp/bsp_gpio.h"
#include "bsp/bsp_quiet.h"
#include "bsp/bsp_spi.h"

static bool g_acquired = false;
static bool g_display_slow = false;
static spi_bus_device_t g_owner = SPI_BUS_DEVICE_W25Q;

static const bsp_spi_config_t g_mode0_conservative = {
    .prescaler = BSP_SPI_PRESCALER_DIV8,
    .cpol = false,
    .cpha = false,
};

static const bsp_spi_config_t g_mode0_display_write = {
    .prescaler = BSP_SPI_PRESCALER_DIV4, .cpol = false, .cpha = false,
};

static const bsp_spi_config_t g_mode0_display_slow = {
    .prescaler = BSP_SPI_PRESCALER_DIV64,
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
    g_display_slow = false;
    deselect_all();
    return bsp_spi2_init(&g_mode0_conservative);
}

bsp_status_t spi_bus_set_display_slow(bool enabled)
{
    if (g_acquired)
        return BSP_STATUS_BUSY;
    g_display_slow = enabled;
    return BSP_STATUS_OK;
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
    const bool tft=device==SPI_BUS_DEVICE_ILI9341 || device==SPI_BUS_DEVICE_ILI9341_READ;
    const bsp_spi_config_t *config=tft && g_display_slow?&g_mode0_display_slow:
        device==SPI_BUS_DEVICE_ILI9341?&g_mode0_display_write:&g_mode0_conservative;
    const bsp_status_t status=bsp_spi2_configure(config);
    if(status!=BSP_STATUS_OK) return status;

    switch (device)
    {
    case SPI_BUS_DEVICE_W25Q:
        (void)bsp_gpio_write_output(BSP_GPIO_OUTPUT_FLASH_CS, false);
        break;
    case SPI_BUS_DEVICE_ILI9341:
    case SPI_BUS_DEVICE_ILI9341_READ:
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
