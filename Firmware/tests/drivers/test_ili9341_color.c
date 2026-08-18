#include "drivers/ili9341_color.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static int g_failures = 0;

static void expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        g_failures++;
    }
}

static void expect_pixel(uint16_t pixel, uint8_t high, uint8_t low)
{
    uint8_t wire[2] = {0u, 0u};

    ili9341_rgb565_to_wire(pixel, wire);
    expect_true(wire[0] == high, "RGB565 high byte");
    expect_true(wire[1] == low, "RGB565 low byte");
}

static void test_rgb565_wire_order(void)
{
    expect_pixel(0xF800u, 0xF8u, 0x00u);
    expect_pixel(0x07E0u, 0x07u, 0xE0u);
    expect_pixel(0x001Fu, 0x00u, 0x1Fu);
    expect_pixel(0xFFFFu, 0xFFu, 0xFFu);
}

int main(void)
{
    test_rgb565_wire_order();
    return (g_failures == 0) ? 0 : 1;
}
