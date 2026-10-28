#include "ui/ui_waveform.h"
#include <math.h>
#include <stddef.h>

static float maximum(float a, float b) { return a > b ? a : b; }
static float minimum(float a, float b) { return a < b ? a : b; }
static int nearest(float v) { return (int)(v + (v < 0.0f ? -0.5f : 0.5f)); }

static float value_at(const measurement_waveform_snapshot_t *s, unsigned ch, float sample)
{
    if (sample < 0.0f) sample = 0.0f;
    if (sample > 255.0f) sample = 255.0f;
    const unsigned index = (unsigned)sample;
    const float a = measurement_waveform_mv(s, ch, index);
    const float b = measurement_waveform_mv(s, ch, index < 255u ? index + 1u : index);
    return a + (sample - (float)index) * (b - a);
}

void ui_waveform_start(ui_waveform_t *op, const measurement_waveform_snapshot_t *s)
{
    *op = (ui_waveform_t){.mv_div = {1.0f, 1.0f}};
    if (s == NULL) return;
    const float left = measurement_waveform_sample_at(s, 0.0f);
    const float right = measurement_waveform_sample_at(s, 1.0f);
    for (unsigned ch = 0u; ch < 2u; ++ch)
    {
        float peak = maximum(fabsf(value_at(s, ch, left)), fabsf(value_at(s, ch, right)));
        for (unsigned i = 0u; i < 256u; ++i)
            if ((float)i >= left && (float)i <= right)
                peak = maximum(peak, fabsf(measurement_waveform_mv(s, ch, i)));
        op->mv_div[ch] = measurement_waveform_division(peak);
    }
}

/* Rasterize each real piecewise-linear segment intersecting this pixel column,
 * including vertices inside it. No reconstructed sinusoid or phase realignment. */
static void column_extent(const ui_waveform_t *op, const measurement_waveform_snapshot_t *s,
                          unsigned ch, int *top, int *bottom)
{
    const float left = measurement_waveform_sample_at(s, maximum(0.0f, ((float)op->column - 0.5f) / 240.0f));
    const float right = measurement_waveform_sample_at(s, minimum(1.0f, ((float)op->column + 0.5f) / 240.0f));
    float lo = minimum(value_at(s, ch, left), value_at(s, ch, right));
    float hi = maximum(value_at(s, ch, left), value_at(s, ch, right));
    unsigned first = (unsigned)maximum(0.0f, left);
    if ((float)first < left) ++first;
    for (unsigned i = first; i < 256u && (float)i <= right; ++i)
    {
        const float v = measurement_waveform_mv(s, ch, i);
        lo = minimum(lo, v); hi = maximum(hi, v);
    }
    *top = nearest(54.0f - 18.0f * hi / op->mv_div[ch]);
    *bottom = nearest(54.0f - 18.0f * lo / op->mv_div[ch]);
}

bsp_status_t ui_waveform_step(ui_waveform_t *op, const ili9341_t *d,
                              const measurement_waveform_snapshot_t *s)
{
    if (op == NULL || d == NULL) return BSP_STATUS_INVALID_ARG;
    if (s == NULL || op->column > UI_WAVEFORM_WIDTH) return BSP_STATUS_OK;
    uint16_t pixels[64];
    int top[2], bottom[2];
    for (unsigned ch = 0u; ch < 2u; ++ch) column_extent(op, s, ch, &top[ch], &bottom[ch]);
    const uint16_t remaining = (uint16_t)(UI_WAVEFORM_HEIGHT + 1u - op->row);
    const uint16_t count = remaining < 64u ? remaining : 64u;
    for (unsigned n = 0u; n < count; ++n)
    {
        const int y = (int)op->row + (int)n;
        uint16_t color = op->column % 30u == 0u || y % 18 == 0 ? 0x2104u : 0u;
        if (y == 54 || op->column == 120u) color = 0x630Cu;
        if (y >= top[0] && y <= bottom[0]) color = 0xFFE0u;
        if (y >= top[1] && y <= bottom[1]) color = 0x07FFu;
        pixels[n] = color;
    }
    bsp_status_t status = ili9341_set_window(d, (uint16_t)(UI_WAVEFORM_X + op->column),
        (uint16_t)(UI_WAVEFORM_Y + op->row), 1u, count);
    if (status == BSP_STATUS_OK) status = ili9341_write_pixels_rgb565(pixels, count);
    if (status != BSP_STATUS_OK) return status;
    op->row = (uint16_t)(op->row + count);
    if (op->row > UI_WAVEFORM_HEIGHT) { op->row = 0u; ++op->column; }
    return op->column > UI_WAVEFORM_WIDTH ? BSP_STATUS_OK : BSP_STATUS_BUSY;
}
