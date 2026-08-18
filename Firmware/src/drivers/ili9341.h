#ifndef WTK_ILI9341_H
#define WTK_ILI9341_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "drivers/ili9341_geometry.h"

enum
{
    ILI9341_WIDTH = ILI9341_NATIVE_WIDTH,
    ILI9341_HEIGHT = ILI9341_NATIVE_HEIGHT,
    ILI9341_FILL_CHUNK_PIXELS = 64u,
};

typedef enum
{
    ILI9341_INIT_IDLE = 0,
    ILI9341_INIT_RESET_ASSERTED,
    ILI9341_INIT_RESET_RELEASED,
    ILI9341_INIT_COMMANDS,
    ILI9341_INIT_READY,
    ILI9341_INIT_ERROR,
} ili9341_init_state_t;

typedef struct
{
    ili9341_init_state_t init_state;
    uint8_t init_index;
    uint32_t wait_until_ms;
    uint8_t rotation;
    bool ready;
} ili9341_t;

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t color_rgb565;
    uint32_t remaining_pixels;
    bool window_sent;
    bool active;
} ili9341_fill_t;

void ili9341_init_context(ili9341_t *display);
void ili9341_init_start(ili9341_t *display, uint32_t now_ms);
bsp_status_t ili9341_init_step(ili9341_t *display, uint32_t now_ms);
bsp_status_t ili9341_set_rotation(ili9341_t *display, uint8_t rotation);
bsp_status_t ili9341_set_window(const ili9341_t *display,
                                uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height);
bsp_status_t ili9341_write_pixels_rgb565(const uint16_t *pixels, size_t count);
void ili9341_fill_start(ili9341_fill_t *fill,
                        uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t height,
                        uint16_t color_rgb565);
bsp_status_t ili9341_fill_step(const ili9341_t *display, ili9341_fill_t *fill, uint16_t max_pixels);

#endif
