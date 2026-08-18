#include "drivers/ili9341_color.h"

void ili9341_rgb565_to_wire(uint16_t pixel, uint8_t wire[2])
{
    wire[0] = (uint8_t)(pixel >> 8u);
    wire[1] = (uint8_t)(pixel & 0xFFu);
}

void ili9341_rgb565_array_to_wire(const uint16_t *pixels, uint8_t *wire, size_t count)
{
    for (size_t i = 0u; i < count; i++)
    {
        ili9341_rgb565_to_wire(pixels[i], &wire[i * 2u]);
    }
}
