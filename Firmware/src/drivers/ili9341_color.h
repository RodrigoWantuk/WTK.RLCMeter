#ifndef WTK_ILI9341_COLOR_H
#define WTK_ILI9341_COLOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Indices: gamma 0=identity, 1=0.8, 2=1.2; contrast 50..150% in
 * 25% steps; saturation 0..200% in 50% steps. No panel analog registers. */
typedef struct { uint8_t gamma, contrast, saturation; bool inverted; } ili9341_picture_t;
bool ili9341_picture_valid(const ili9341_picture_t *picture);
uint16_t ili9341_picture_pixel(uint16_t pixel, const ili9341_picture_t *picture);

void ili9341_rgb565_to_wire(uint16_t pixel, uint8_t wire[2]);
void ili9341_rgb565_array_to_wire(const uint16_t *pixels, uint8_t *wire, size_t count);

#endif
