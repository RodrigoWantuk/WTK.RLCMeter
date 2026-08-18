#ifndef WTK_ILI9341_GEOMETRY_H
#define WTK_ILI9341_GEOMETRY_H

#include <stdbool.h>
#include <stdint.h>

enum
{
    ILI9341_NATIVE_WIDTH = 240u,
    ILI9341_NATIVE_HEIGHT = 320u,
};

typedef struct
{
    uint16_t width;
    uint16_t height;
} ili9341_size_t;

uint8_t ili9341_normalize_rotation(uint8_t rotation);
ili9341_size_t ili9341_logical_size(uint8_t rotation);
bool ili9341_rect_is_valid(uint8_t rotation, uint16_t x, uint16_t y, uint16_t width, uint16_t height);

#endif
