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
    ili9341_picture_t p={.contrast=2u,.saturation=2u};
    for(uint32_t pixel=0u;pixel<=UINT16_MAX;++pixel)
        expect_true(ili9341_picture_pixel((uint16_t)pixel,&p)==pixel,"all 65536 neutral RGB565 colors are exact");
    p.gamma=1u; const uint16_t lifted=ili9341_picture_pixel(0x7befu,&p);
    p.gamma=2u; expect_true(lifted>0x7befu && ili9341_picture_pixel(0x7befu,&p)<0x7befu,"gamma changes midtones in the requested direction");
    p.gamma=0u;p.saturation=0u; const uint16_t gray=ili9341_picture_pixel(0xf800u,&p);
    const int r=(int)((gray>>11u)&31u)*2,g=(int)((gray>>5u)&63u),b=(int)(gray&31u)*2;
    expect_true(r-g>=-1 && r-g<=1 && b-g>=-1 && b-g<=1,"zero saturation produces quantized gray");
    for(p.gamma=0u;p.gamma<3u;++p.gamma) for(p.saturation=0u;p.saturation<5u;++p.saturation) {
        expect_true(ili9341_picture_pixel(0u,&p)==0u && ili9341_picture_pixel(0xffffu,&p)==0xffffu,"gamma/saturation preserve black/white endpoints");
    }
    p.gamma=3u;expect_true(!ili9341_picture_valid(&p),"invalid gamma rejected");
    return (g_failures == 0) ? 0 : 1;
}
