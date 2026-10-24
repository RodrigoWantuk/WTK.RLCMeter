#include "drivers/ili9341.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_gpio.h"
#include "drivers/ili9341_color.h"
#include "drivers/spi_bus.h"

#ifndef WTK_ILI9341_DIAGNOSTIC_SUPPORT
#define WTK_ILI9341_DIAGNOSTIC_SUPPORT 0
#endif

enum
{
    ILI9341_CMD_SWRESET = 0x01u,
    ILI9341_CMD_SLPOUT = 0x11u,
    ILI9341_CMD_DISPON = 0x29u,
    ILI9341_CMD_CASET = 0x2Au,
    ILI9341_CMD_PASET = 0x2Bu,
    ILI9341_CMD_RAMWR = 0x2Cu,
    ILI9341_CMD_MADCTL = 0x36u,
    ILI9341_CMD_PIXFMT = 0x3Au,
    ILI9341_PIXFMT_RGB565 = 0x55u,
    ILI9341_SPI_TIMEOUT_MS = 5u,
};

typedef struct
{
    uint8_t command;
    uint8_t data[4];
    uint8_t data_len;
    uint16_t wait_after_ms;
} ili9341_init_command_t;

static const ili9341_init_command_t g_init_commands[] = {
    {ILI9341_CMD_SWRESET, {0u, 0u, 0u, 0u}, 0u, 120u},
    {ILI9341_CMD_SLPOUT, {0u, 0u, 0u, 0u}, 0u, 120u},
    {ILI9341_CMD_PIXFMT, {ILI9341_PIXFMT_RGB565, 0u, 0u, 0u}, 1u, 10u},
    {ILI9341_CMD_MADCTL, {0x48u, 0u, 0u, 0u}, 1u, 10u},
    {ILI9341_CMD_DISPON, {0u, 0u, 0u, 0u}, 0u, 20u},
};

/* ILI9341/ST7789 common DCS subset; no controller-specific analog settings. */
#if WTK_ILI9341_DIAGNOSTIC_SUPPORT
static const ili9341_init_command_t g_diagnostic_commands[] = {
    {ILI9341_CMD_SWRESET, {0u}, 0u, 150u},
    {0x28u, {0u}, 0u, 20u}, /* Display off while configuring. */
    {ILI9341_CMD_SLPOUT, {0u}, 0u, 150u},
    {ILI9341_CMD_PIXFMT, {ILI9341_PIXFMT_RGB565}, 1u, 10u},
    {ILI9341_CMD_MADCTL, {0x48u}, 1u, 10u},
    {0x20u, {0u}, 0u, 20u}, /* Inversion off. */
    {0x13u, {0u}, 0u, 20u}, /* Normal/full display mode. */
    {ILI9341_CMD_DISPON, {0u}, 0u, 150u},
};
#endif

static bsp_status_t ili9341_write_command(uint8_t command)
{
    bsp_status_t status = spi_bus_acquire(SPI_BUS_DEVICE_ILI9341);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }

    (void)bsp_gpio_write_output(BSP_GPIO_OUTPUT_TFT_DC, false);
    status = spi_bus_transfer(&command, NULL, 1u, ILI9341_SPI_TIMEOUT_MS);
    const bsp_status_t release_status = spi_bus_release(SPI_BUS_DEVICE_ILI9341);

    return (status == BSP_STATUS_OK) ? release_status : status;
}

static bsp_status_t ili9341_write_data(const uint8_t *data, size_t size)
{
    bsp_status_t status = spi_bus_acquire(SPI_BUS_DEVICE_ILI9341);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }

    (void)bsp_gpio_write_output(BSP_GPIO_OUTPUT_TFT_DC, true);
    status = spi_bus_transfer(data, NULL, size, ILI9341_SPI_TIMEOUT_MS);
    const bsp_status_t release_status = spi_bus_release(SPI_BUS_DEVICE_ILI9341);

    return (status == BSP_STATUS_OK) ? release_status : status;
}

static bsp_status_t ili9341_write_command_data(uint8_t command, const uint8_t *data, size_t size)
{
    bsp_status_t status = ili9341_write_command(command);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }

    if (size == 0u)
    {
        return BSP_STATUS_OK;
    }

    return ili9341_write_data(data, size);
}

bsp_status_t ili9341_read_diagnostic_raw(uint8_t command, uint8_t *response, size_t size)
{
    if (response == NULL || size == 0u || size > 5u ||
        !(command == 0x04u || (command >= 0x09u && command <= 0x0Fu) || command == 0xD3u))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    bsp_status_t status = spi_bus_acquire(SPI_BUS_DEVICE_ILI9341);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    status = bsp_gpio_write_output(BSP_GPIO_OUTPUT_TFT_DC, false);
    if (status == BSP_STATUS_OK)
        status = spi_bus_transfer(&command, NULL, 1u, ILI9341_SPI_TIMEOUT_MS);
    if (status == BSP_STATUS_OK)
        status = bsp_gpio_write_output(BSP_GPIO_OUTPUT_TFT_DC, true);
    if (status == BSP_STATUS_OK)
        status = spi_bus_transfer(NULL, response, size, ILI9341_SPI_TIMEOUT_MS);
    const bsp_status_t release_status = spi_bus_release(SPI_BUS_DEVICE_ILI9341);
    return status == BSP_STATUS_OK ? release_status : status;
}

void ili9341_init_context(ili9341_t *display)
{
    if (display == NULL)
    {
        return;
    }

    display->init_state = ILI9341_INIT_IDLE;
    display->init_index = 0u;
    display->wait_until_ms = 0u;
    display->rotation = 0u;
    display->ready = false;
    display->conservative_diagnostic = false;
}

void ili9341_init_start(ili9341_t *display, uint32_t now_ms)
{
    if (display == NULL)
    {
        return;
    }

    display->init_state = ILI9341_INIT_RESET_ASSERTED;
    display->init_index = 0u;
    display->wait_until_ms = now_ms + 10u;
    display->ready = false;
    display->conservative_diagnostic = false;
    (void)bsp_gpio_write_output(BSP_GPIO_OUTPUT_TFT_RST, false);
}

void ili9341_init_diagnostic_start(ili9341_t *display, uint32_t now_ms)
{
    ili9341_init_start(display, now_ms);
#if WTK_ILI9341_DIAGNOSTIC_SUPPORT
    if (display != NULL)
    {
        display->conservative_diagnostic = true;
        display->wait_until_ms = now_ms + 50u;
    }
#endif
}

bsp_status_t ili9341_init_step(ili9341_t *display, uint32_t now_ms)
{
    if (display == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    if (display->init_state == ILI9341_INIT_READY)
    {
        return BSP_STATUS_OK;
    }

    if ((now_ms - display->wait_until_ms) >= 0x80000000u)
    {
        return BSP_STATUS_OK;
    }

    switch (display->init_state)
    {
    case ILI9341_INIT_RESET_ASSERTED:
        (void)bsp_gpio_write_output(BSP_GPIO_OUTPUT_TFT_RST, true);
#if WTK_ILI9341_DIAGNOSTIC_SUPPORT
        display->wait_until_ms = now_ms + (display->conservative_diagnostic ? 250u : 120u);
#else
        display->wait_until_ms = now_ms + 120u;
#endif
        display->init_state = ILI9341_INIT_RESET_RELEASED;
        return BSP_STATUS_OK;
    case ILI9341_INIT_RESET_RELEASED:
        display->init_state = ILI9341_INIT_COMMANDS;
        return BSP_STATUS_OK;
    case ILI9341_INIT_COMMANDS:
        if (display->init_index >= (
#if WTK_ILI9341_DIAGNOSTIC_SUPPORT
            display->conservative_diagnostic ?
            sizeof(g_diagnostic_commands) / sizeof(g_diagnostic_commands[0]) :
#endif
            sizeof(g_init_commands) / sizeof(g_init_commands[0])))
        {
            display->ready = true;
            display->init_state = ILI9341_INIT_READY;
            return BSP_STATUS_OK;
        }

        {
            const ili9341_init_command_t *const command =
#if WTK_ILI9341_DIAGNOSTIC_SUPPORT
                display->conservative_diagnostic ? &g_diagnostic_commands[display->init_index] :
#endif
                &g_init_commands[display->init_index];
            const bsp_status_t status =
                ili9341_write_command_data(command->command, command->data, command->data_len);
            if (status != BSP_STATUS_OK)
            {
                display->init_state = ILI9341_INIT_ERROR;
                return status;
            }
            display->wait_until_ms = now_ms + command->wait_after_ms;
            display->init_index++;
        }
        return BSP_STATUS_OK;
    case ILI9341_INIT_IDLE:
    case ILI9341_INIT_ERROR:
    default:
        return BSP_STATUS_ERROR;
    }
}

bsp_status_t ili9341_set_rotation(ili9341_t *display, uint8_t rotation)
{
    if (display == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    static const uint8_t madctl_values[4] = {0x48u, 0x28u, 0x88u, 0xE8u};
    const uint8_t index = ili9341_normalize_rotation(rotation);
    const bsp_status_t status = ili9341_write_command_data(ILI9341_CMD_MADCTL, &madctl_values[index], 1u);
    if (status == BSP_STATUS_OK)
    {
        display->rotation = index;
    }

    return status;
}

bsp_status_t ili9341_set_window(const ili9341_t *display,
                                uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height)
{
    if ((display == NULL) || !ili9341_rect_is_valid(display->rotation, x, y, width, height))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    const uint16_t x_end = (uint16_t)(x + width - 1u);
    const uint16_t y_end = (uint16_t)(y + height - 1u);
    const uint8_t col_data[4] = {
        (uint8_t)(x >> 8u),
        (uint8_t)(x & 0xFFu),
        (uint8_t)(x_end >> 8u),
        (uint8_t)(x_end & 0xFFu),
    };
    const uint8_t row_data[4] = {
        (uint8_t)(y >> 8u),
        (uint8_t)(y & 0xFFu),
        (uint8_t)(y_end >> 8u),
        (uint8_t)(y_end & 0xFFu),
    };

    bsp_status_t status = ili9341_write_command_data(ILI9341_CMD_CASET, col_data, sizeof(col_data));
    if (status != BSP_STATUS_OK)
    {
        return status;
    }

    status = ili9341_write_command_data(ILI9341_CMD_PASET, row_data, sizeof(row_data));
    if (status != BSP_STATUS_OK)
    {
        return status;
    }

    return ili9341_write_command(ILI9341_CMD_RAMWR);
}

bsp_status_t ili9341_write_pixels_rgb565(const uint16_t *pixels, size_t count)
{
    static uint8_t wire_pixels[ILI9341_FILL_CHUNK_PIXELS * 2u];

    if ((pixels == NULL) && (count > 0u))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    size_t offset = 0u;
    while (offset < count)
    {
        size_t chunk = count - offset;
        if (chunk > ILI9341_FILL_CHUNK_PIXELS)
        {
            chunk = ILI9341_FILL_CHUNK_PIXELS;
        }

        ili9341_rgb565_array_to_wire(&pixels[offset], wire_pixels, chunk);
        const bsp_status_t status = ili9341_write_data(wire_pixels, chunk * 2u);
        if (status != BSP_STATUS_OK)
        {
            return status;
        }
        offset += chunk;
    }

    return BSP_STATUS_OK;
}

void ili9341_fill_start(ili9341_fill_t *fill,
                        uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t height,
                        uint16_t color_rgb565)
{
    if (fill == NULL)
    {
        return;
    }

    fill->x = x;
    fill->y = y;
    fill->width = width;
    fill->height = height;
    fill->color_rgb565 = color_rgb565;
    fill->remaining_pixels = (uint32_t)width * (uint32_t)height;
    fill->window_sent = false;
    fill->active = (fill->remaining_pixels > 0u);
}

bsp_status_t ili9341_fill_step(const ili9341_t *display, ili9341_fill_t *fill, uint16_t max_pixels)
{
    static uint16_t pixels[ILI9341_FILL_CHUNK_PIXELS];

    if ((display == NULL) || (fill == NULL) || !fill->active)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    if (!fill->window_sent)
    {
        const bsp_status_t status = ili9341_set_window(display, fill->x, fill->y, fill->width, fill->height);
        if (status != BSP_STATUS_OK)
        {
            return status;
        }
        fill->window_sent = true;
    }

    uint16_t chunk = (max_pixels == 0u) ? ILI9341_FILL_CHUNK_PIXELS : max_pixels;
    if (chunk > ILI9341_FILL_CHUNK_PIXELS)
    {
        chunk = ILI9341_FILL_CHUNK_PIXELS;
    }
    if (chunk > fill->remaining_pixels)
    {
        chunk = (uint16_t)fill->remaining_pixels;
    }

    for (uint16_t i = 0u; i < chunk; i++)
    {
        pixels[i] = fill->color_rgb565;
    }

    const bsp_status_t status = ili9341_write_pixels_rgb565(pixels, chunk);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }

    fill->remaining_pixels -= chunk;
    if (fill->remaining_pixels == 0u)
    {
        fill->active = false;
    }

    return BSP_STATUS_OK;
}
