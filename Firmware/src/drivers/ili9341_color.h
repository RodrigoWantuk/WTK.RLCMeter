#ifndef WTK_ILI9341_COLOR_H
#define WTK_ILI9341_COLOR_H

#include <stddef.h>
#include <stdint.h>

void ili9341_rgb565_to_wire(uint16_t pixel, uint8_t wire[2]);
void ili9341_rgb565_array_to_wire(const uint16_t *pixels, uint8_t *wire, size_t count);

#endif
