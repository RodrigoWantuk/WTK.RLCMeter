#include "drivers/ili9341_geometry.h"

uint8_t ili9341_normalize_rotation(uint8_t rotation)
{
    return (uint8_t)(rotation & 0x03u);
}

ili9341_size_t ili9341_logical_size(uint8_t rotation)
{
    const uint8_t normalized = ili9341_normalize_rotation(rotation);
    if ((normalized == 1u) || (normalized == 3u))
    {
        return (ili9341_size_t){
            .width = ILI9341_NATIVE_HEIGHT,
            .height = ILI9341_NATIVE_WIDTH,
        };
    }

    return (ili9341_size_t){
        .width = ILI9341_NATIVE_WIDTH,
        .height = ILI9341_NATIVE_HEIGHT,
    };
}

bool ili9341_rect_is_valid(uint8_t rotation, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    if ((width == 0u) || (height == 0u))
    {
        return false;
    }

    const ili9341_size_t size = ili9341_logical_size(rotation);
    if ((x >= size.width) || (y >= size.height))
    {
        return false;
    }

    return ((uint32_t)width <= ((uint32_t)size.width - (uint32_t)x)) &&
           ((uint32_t)height <= ((uint32_t)size.height - (uint32_t)y));
}
