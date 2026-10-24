#include "drivers/ili9341.h"
#include "drivers/spi_bus.h"
#include "bsp/bsp_gpio.h"
#include <stdio.h>
#include <string.h>

static unsigned acquired, released, transfers;
static bool selected, dc;
static bsp_status_t acquire_result, transfer_result, release_result;
static int failures;
static uint8_t expected_command = 0xD3u;
static size_t expected_size = 4u;
static bool init_test, reset_high;
static uint8_t commands[16];
static unsigned command_count;
static uint8_t madctl;

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
    if (init_test && output == BSP_GPIO_OUTPUT_TFT_RST)
    {
        reset_high = high;
        return BSP_STATUS_OK;
    }
    check(selected && output == BSP_GPIO_OUTPUT_TFT_DC, "only DC changes inside transaction");
    dc = high; return BSP_STATUS_OK;
}
bsp_status_t spi_bus_transfer(const uint8_t *tx, uint8_t *rx, size_t length, uint32_t timeout_ms)
{
    transfers++;
    check(selected && timeout_ms == 5u, "bounded transfer with CS held");
    if (init_test)
    {
        check(rx == NULL && tx != NULL && length == 1u, "bounded init command/data");
        if (!dc && command_count < sizeof(commands)) commands[command_count++] = *tx;
        else if (dc && command_count > 0u && commands[command_count - 1u] == 0x36u) madctl = *tx;
        return BSP_STATUS_OK;
    }
    if (tx != NULL)
        check(!dc && rx == NULL && length == 1u && *tx == expected_command, "command phase");
    else
    {
        check(dc && rx != NULL && length == expected_size, "response phase");
        const uint8_t response[] = {0xA5u, 0u, 0x93u, 0x41u, 0x5Au};
        memcpy(rx, response, length);
    }
    return transfer_result;
}
int main(void)
{
    uint8_t response[5] = {0u};
    check(ili9341_read_diagnostic_raw(0xD3u, response, 4u) == BSP_STATUS_OK, "successful read");
    check(acquired == 1u && released == 1u && transfers == 2u && !selected, "single CS interval");
    check(response[0] == 0xA5u && response[2] == 0x93u && response[3] == 0x41u,
          "raw response preserves dummy and identity bytes");
    const unsigned before = acquired;
    check(ili9341_read_diagnostic_raw(0x29u, response, 4u) == BSP_STATUS_INVALID_ARG, "reject write command");
    check(ili9341_read_diagnostic_raw(0xD3u, NULL, 4u) == BSP_STATUS_INVALID_ARG, "reject null");
    check(ili9341_read_diagnostic_raw(0xD3u, response, 0u) == BSP_STATUS_INVALID_ARG, "reject empty");
    check(ili9341_read_diagnostic_raw(0xD3u, response, 6u) == BSP_STATUS_INVALID_ARG, "reject oversized");
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
    release_result = BSP_STATUS_OK;
    expected_size = 5u;
    expected_command = 0x04u;
    check(ili9341_read_diagnostic_raw(0x04u, response, 5u) == BSP_STATUS_OK && response[4] == 0x5Au,
          "common display identification preserves all raw bytes");
    expected_command = 0x09u;
    check(ili9341_read_diagnostic_raw(0x09u, response, 5u) == BSP_STATUS_OK,
          "full display status read supported");

    init_test = true;
    ili9341_t display;
    ili9341_init_context(&display);
    const uint32_t start = UINT32_MAX - 30u;
    ili9341_init_start(&display, start);
    check(!reset_high && !display.ready, "diagnostic asserts reset");
    (void)ili9341_init_step(&display, start + 49u);
    check(!reset_high && command_count == 0u, "reset held across time wrap");
    (void)ili9341_init_step(&display, start + 50u);
    check(reset_high && !display.ready, "release at50ms");
    (void)ili9341_init_step(&display, start + 299u);
    check(command_count == 0u, "wait250ms after reset release");
    for (uint32_t elapsed = 300u; elapsed < 1200u; elapsed++)
        (void)ili9341_init_step(&display, start + elapsed);
    const uint8_t expected[] = {0x01u,0x28u,0x11u,0x3Au,0x36u,0x20u,0x13u,0x29u};
    check(display.ready && command_count == sizeof(expected) && memcmp(commands,expected,sizeof(expected)) == 0,
          "only common DCS commands reach ready, no analog voltage commands");
    check(display.rotation == 2u && madctl == 0x88u, "diagnostic portrait rotated180 matches owner mounting");
    command_count = 0u;
    ili9341_init_start(&display, 0u);
    check(display.rotation == ILI9341_DEFAULT_ROTATION, "all profiles restore mounted orientation");
    for (uint32_t now = 0u; now < 1200u; now++) (void)ili9341_init_step(&display, now);
    check(display.ready && command_count == sizeof(expected) && memcmp(commands,expected,sizeof(expected)) == 0,
          "restart uses shared bench-confirmed initialization");
    check(madctl == 0x88u, "shared portrait180 MADCTL");
    return failures == 0 ? 0 : 1;
}
