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

/* Quantized offline round(63 * (x/63)^gamma); never evaluate pow on MCU. */
static const uint8_t picture_gamma[2][64] = {
    {0u,2u,4u,6u,7u,8u,10u,11u,12u,13u,14u,16u,17u,18u,19u,20u,21u,22u,23u,24u,25u,26u,27u,28u,29u,30u,31u,32u,33u,34u,35u,36u,37u,38u,38u,39u,40u,41u,42u,43u,44u,45u,46u,46u,47u,48u,49u,50u,51u,52u,52u,53u,54u,55u,56u,57u,57u,58u,59u,60u,61u,61u,62u,63u},
    {0u,0u,1u,2u,2u,3u,4u,5u,5u,6u,7u,8u,9u,9u,10u,11u,12u,13u,14u,15u,16u,17u,18u,19u,20u,21u,22u,23u,24u,25u,26u,27u,28u,29u,30u,31u,32u,33u,34u,35u,37u,38u,39u,40u,41u,42u,43u,44u,45u,47u,48u,49u,50u,51u,52u,54u,55u,56u,57u,58u,59u,61u,62u,63u},
};

bool ili9341_picture_valid(const ili9341_picture_t *p)
{
    return p != NULL && p->gamma < 3u && p->contrast < 5u && p->saturation < 5u;
}
static int32_t picture_clip(int32_t value)
{
    return value < 0 ? 0 : value > 63 ? 63 : value;
}
uint16_t ili9341_picture_pixel(uint16_t pixel, const ili9341_picture_t *p)
{
    if (!ili9341_picture_valid(p) || (p->gamma==0u && p->contrast==2u && p->saturation==2u)) return pixel;
    int32_t r=(int32_t)((((uint32_t)pixel >> 11u)*63u+15u)/31u);
    int32_t g=(int32_t)(((uint32_t)pixel >> 5u)&63u);
    int32_t b=(int32_t)((((uint32_t)pixel&31u)*63u+15u)/31u);
    if (p->gamma!=0u) { r=picture_gamma[p->gamma-1u][r];g=picture_gamma[p->gamma-1u][g];b=picture_gamma[p->gamma-1u][b]; }
    const int32_t contrast=50+(int32_t)p->contrast*25;
    r=picture_clip(32+((r-32)*contrast)/100);
    g=picture_clip(32+((g-32)*contrast)/100);
    b=picture_clip(32+((b-32)*contrast)/100);
    const int32_t y=(77*r+150*g+29*b+128)/256;
    const int32_t saturation=(int32_t)p->saturation*50;
    r=picture_clip(y+((r-y)*saturation)/100);
    g=picture_clip(y+((g-y)*saturation)/100);
    b=picture_clip(y+((b-y)*saturation)/100);
    return (uint16_t)((((uint32_t)r*31u+31u)/63u)<<11u |
                      (uint32_t)g<<5u | (((uint32_t)b*31u+31u)/63u));
}
