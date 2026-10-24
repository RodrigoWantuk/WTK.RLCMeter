#include "drivers/ili9341.h"
#include "drivers/spi_bus.h"
#include "bsp/bsp_gpio.h"
#include <stdio.h>
#include <string.h>

static unsigned acquired, released, transfers;
static bool selected, dc;
static bsp_status_t acquire_result, transfer_result, release_result;
static int failures;

static void check(bool condition, const char *message)
{
    if (!condition) { (void)fprintf(stderr, "FAIL: %s\n", message); failures++; }
}
bsp_status_t spi_bus_acquire(spi_bus_device_t device)
{
    check(device == SPI_BUS_DEVICE_ILI9341, "only TFT selected");
    acquired++;
    selected = acquire_result == BSP_STATUS_OK;
    return acquire_result;
}
bsp_status_t spi_bus_release(spi_bus_device_t device)
{
    check(selected && device == SPI_BUS_DEVICE_ILI9341, "release selected TFT");
    released++; selected = false; return release_result;
}
bsp_status_t bsp_gpio_write_output(bsp_gpio_output_t output, bool high)
{
    check(selected && output == BSP_GPIO_OUTPUT_TFT_DC, "only DC changes inside transaction");
    dc = high; return BSP_STATUS_OK;
}
bsp_status_t spi_bus_transfer(const uint8_t *tx, uint8_t *rx, size_t length, uint32_t timeout_ms)
{
    transfers++;
    check(selected && timeout_ms == 5u, "bounded transfer with CS held");
    if (tx != NULL)
        check(!dc && rx == NULL && length == 1u && *tx == 0xD3u, "command phase");
    else
    {
        check(dc && rx != NULL && length == 4u, "response phase");
        const uint8_t response[] = {0xA5u, 0u, 0x93u, 0x41u};
        memcpy(rx, response, sizeof(response));
    }
    return transfer_result;
}
int main(void)
{
    uint8_t response[4] = {0u};
    check(ili9341_read_diagnostic_raw(0xD3u, response, 4u) == BSP_STATUS_OK, "successful read");
    check(acquired == 1u && released == 1u && transfers == 2u && !selected, "single CS interval");
    check(response[0] == 0xA5u && response[2] == 0x93u && response[3] == 0x41u,
          "raw response preserves dummy and identity bytes");
    const unsigned before = acquired;
    check(ili9341_read_diagnostic_raw(0x29u, response, 4u) == BSP_STATUS_INVALID_ARG, "reject write command");
    check(ili9341_read_diagnostic_raw(0xD3u, NULL, 4u) == BSP_STATUS_INVALID_ARG, "reject null");
    check(ili9341_read_diagnostic_raw(0xD3u, response, 0u) == BSP_STATUS_INVALID_ARG, "reject empty");
    check(ili9341_read_diagnostic_raw(0xD3u, response, 5u) == BSP_STATUS_INVALID_ARG, "reject oversized");
    check(acquired == before, "invalid arguments perform no IO");
    acquire_result = BSP_STATUS_BUSY;
    check(ili9341_read_diagnostic_raw(0xD3u, response, 4u) == BSP_STATUS_BUSY && released == 1u,
          "quiet/busy performs no transfer or release");
    acquire_result = BSP_STATUS_OK;
    transfer_result = BSP_STATUS_TIMEOUT;
    const unsigned previous_transfers = transfers;
    check(ili9341_read_diagnostic_raw(0xD3u, response, 4u) == BSP_STATUS_TIMEOUT && !selected &&
          transfers == previous_transfers + 1u && released == 2u, "command failure releases bus and stops");
    transfer_result = BSP_STATUS_OK;
    release_result = BSP_STATUS_ERROR;
    check(ili9341_read_diagnostic_raw(0xD3u, response, 4u) == BSP_STATUS_ERROR && !selected,
          "release failure propagated");
    return failures == 0 ? 0 : 1;
}
