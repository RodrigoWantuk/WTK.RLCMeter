#include "drivers/spi_bus.h"
#include "bsp/bsp_spi.h"
#include "bsp/bsp_gpio.h"
#include <stdio.h>

static bool quiet, flash_selected, tft_selected;
static bsp_spi_prescaler_t prescaler;
static unsigned configurations;
static int failures;
static void check(bool condition, const char *message)
{
    if (!condition) { (void)fprintf(stderr,"FAIL: %s\n", message); failures++; }
}
bool bsp_quiet_requested(void) { return quiet; }
bsp_status_t bsp_gpio_write_output(bsp_gpio_output_t output, bool high)
{
    if (output == BSP_GPIO_OUTPUT_FLASH_CS) flash_selected = !high;
    else if (output == BSP_GPIO_OUTPUT_TFT_CS) tft_selected = !high;
    else check(false,"only CS outputs controlled");
    check(!flash_selected || !tft_selected,"mutually exclusive CS");
    return BSP_STATUS_OK;
}
bsp_status_t bsp_spi2_configure(const bsp_spi_config_t *config)
{
    check(!flash_selected && !tft_selected,"change clock only with all CS inactive");
    check(!config->cpol && !config->cpha,"mode0 retained");
    prescaler = config->prescaler; configurations++; return BSP_STATUS_OK;
}
bsp_status_t bsp_spi2_init(const bsp_spi_config_t *config) { return bsp_spi2_configure(config); }
bsp_status_t bsp_spi2_transfer(const uint8_t *tx, uint8_t *rx, size_t n, uint32_t timeout)
{
    (void)tx; (void)rx; (void)n; (void)timeout; return BSP_STATUS_OK;
}
int main(void)
{
    check(spi_bus_init() == BSP_STATUS_OK,"init");
    check(spi_bus_set_display_slow(true) == BSP_STATUS_OK,"select slow diagnostic");
    check(spi_bus_acquire(SPI_BUS_DEVICE_ILI9341) == BSP_STATUS_OK && prescaler == BSP_SPI_PRESCALER_DIV64,
          "TFT uses divider64");
    check(spi_bus_set_display_slow(false) == BSP_STATUS_BUSY,"clock policy cannot change during transaction");
    check(spi_bus_release(SPI_BUS_DEVICE_ILI9341) == BSP_STATUS_OK,"release TFT");
    check(spi_bus_acquire(SPI_BUS_DEVICE_W25Q) == BSP_STATUS_OK && prescaler == BSP_SPI_PRESCALER_DIV8,
          "Flash clock unchanged");
    check(spi_bus_release(SPI_BUS_DEVICE_W25Q) == BSP_STATUS_OK,"release Flash");
    quiet = true;
    const unsigned before = configurations;
    check(spi_bus_acquire(SPI_BUS_DEVICE_ILI9341) == BSP_STATUS_BUSY && configurations == before,
          "quiet blocks transfer/configuration");
    quiet = false;
    check(spi_bus_init() == BSP_STATUS_OK,"reinit restores default clock");
    check(spi_bus_acquire(SPI_BUS_DEVICE_ILI9341) == BSP_STATUS_OK && prescaler == BSP_SPI_PRESCALER_DIV4,
          "normal TFT writes use divider4 (9 MHz)");
    check(spi_bus_release(SPI_BUS_DEVICE_ILI9341) == BSP_STATUS_OK,"release default TFT");
    check(spi_bus_acquire(SPI_BUS_DEVICE_ILI9341_READ)==BSP_STATUS_OK && prescaler==BSP_SPI_PRESCALER_DIV8,
          "TFT reads stay at 4.5 MHz independently of writes");
    check(spi_bus_release(SPI_BUS_DEVICE_ILI9341_READ)==BSP_STATUS_OK,"release read owner");
    return failures == 0 ? 0 : 1;
}
