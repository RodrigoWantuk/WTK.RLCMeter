#include "drivers/ili9341.h"
#include "drivers/spi_bus.h"
#include "bsp/bsp_gpio.h"
#include <stdio.h>
#include <string.h>

static bool selected, dc, quiet;
static unsigned acquires, releases, transfers, gpio_calls, fail_transfer, fail_gpio;
static bsp_status_t release_result;
static uint8_t bytes[256];
static bool data_phase[256];
static size_t byte_count;
static int failures;
static void check(bool value, const char *message)
{
    if (!value) { (void)fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}
static void reset_trace(void)
{
    selected = false; dc = false; quiet = false;
    acquires = releases = transfers = gpio_calls = fail_transfer = fail_gpio = 0u;
    release_result = BSP_STATUS_OK; byte_count = 0u;
}
bsp_status_t spi_bus_acquire(spi_bus_device_t device)
{
    check(device == SPI_BUS_DEVICE_ILI9341 && !selected, "TFT exclusive owner");
    ++acquires;
    if (quiet) return BSP_STATUS_BUSY;
    selected = true;
    return BSP_STATUS_OK;
}
bsp_status_t spi_bus_release(spi_bus_device_t device)
{
    check(selected && device == SPI_BUS_DEVICE_ILI9341, "release actual owner");
    ++releases; selected = false;
    return release_result;
}
bsp_status_t bsp_gpio_write_output(bsp_gpio_output_t output, bool high)
{
    check(selected && output == BSP_GPIO_OUTPUT_TFT_DC, "DC stays inside CS interval");
    ++gpio_calls;
    if (gpio_calls == fail_gpio) return BSP_STATUS_ERROR;
    dc = high;
    return BSP_STATUS_OK;
}
bsp_status_t spi_bus_transfer(const uint8_t *tx, uint8_t *rx, size_t size, uint32_t timeout)
{
    check(selected && tx != NULL && rx == NULL && timeout == 5u, "bounded selected write");
    ++transfers;
    if (transfers == fail_transfer) return BSP_STATUS_TIMEOUT;
    check(byte_count + size <= sizeof(bytes), "bounded trace");
    if (byte_count + size > sizeof(bytes)) return BSP_STATUS_ERROR;
    for (size_t i = 0u; i < size; ++i)
    {
        bytes[byte_count] = tx[i]; data_phase[byte_count++] = dc;
    }
    return BSP_STATUS_OK;
}
int main(void)
{
    const ili9341_t display = {.ready = true, .rotation = 2u};
    reset_trace();
    check(ili9341_set_window(&display, 3u, 4u, 5u, 6u) == BSP_STATUS_OK, "window succeeds");
    const uint8_t expected[] = {0x2Au,0u,3u,0u,7u,0x2Bu,0u,4u,0u,9u,0x2Cu};
    check(byte_count == sizeof(expected) && memcmp(bytes, expected, sizeof(expected)) == 0,
          "exact CASET/PASET/RAMWR coordinates and byte order");
    for (size_t i = 0u; i < sizeof(expected); ++i)
        check(data_phase[i] == (i != 0u && i != 5u && i != 10u), "command/data framing");
    check(acquires == 1u && releases == 1u && transfers == 5u && !selected,
          "one CS interval replaces five without extending ownership");
    reset_trace(); quiet = true;
    check(ili9341_set_window(&display, 0u, 0u, 1u, 1u) == BSP_STATUS_BUSY &&
          transfers == 0u && releases == 0u, "quiet blocks before any byte");
    for (unsigned step = 1u; step <= 5u; ++step)
    {
        reset_trace(); fail_transfer = step;
        check(ili9341_set_window(&display, 0u, 0u, 1u, 1u) == BSP_STATUS_TIMEOUT &&
              transfers == step && releases == 1u && !selected, "each failed SPI stage releases and stops");
        reset_trace(); fail_gpio = step;
        check(ili9341_set_window(&display, 0u, 0u, 1u, 1u) == BSP_STATUS_ERROR &&
              gpio_calls == step && releases == 1u && !selected, "each failed DC stage releases and stops");
    }
    reset_trace(); release_result = BSP_STATUS_ERROR;
    check(ili9341_set_window(&display, 0u, 0u, 1u, 1u) == BSP_STATUS_ERROR && !selected,
          "release failure is visible");
    reset_trace();
    check(ili9341_set_window(&display, 239u, 0u, 2u, 1u) == BSP_STATUS_INVALID_ARG &&
          acquires == 0u, "invalid rectangle performs no IO");
    const uint16_t pixels[] = {0xF800u,0x001Fu,0xFFFFu};
    check(ili9341_write_pixels_rgb565(pixels, 3u) == BSP_STATUS_OK, "pixel stream succeeds");
    const uint8_t wire[] = {0xF8u,0u,0u,0x1Fu,0xFFu,0xFFu};
    check(byte_count == sizeof(wire) && memcmp(bytes, wire, sizeof(wire)) == 0,
          "RGB565 wire order is unchanged");
    return failures == 0 ? 0 : 1;
}
