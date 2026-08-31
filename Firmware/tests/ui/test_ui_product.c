#include "ui/ui_product.h"

#include <stdbool.h>
#include <stdio.h>

static uint32_t g_pixels_written;
static uint32_t g_text_chars_written;
static uint32_t g_full_clears;
static uint32_t g_partial_clears;

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

bsp_status_t ili9341_set_window(const ili9341_t *display,
                                uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height)
{
    (void)display;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return BSP_STATUS_OK;
}

bsp_status_t ili9341_write_pixels_rgb565(const uint16_t *pixels, size_t count)
{
    (void)pixels;
    g_pixels_written += (uint32_t)count;
    return BSP_STATUS_OK;
}

void ili9341_fill_start(ili9341_fill_t *fill,
                        uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t height,
                        uint16_t color_rgb565)
{
    if (fill == NULL)
    {
        return;
    }
    fill->x = x;
    fill->y = y;
    fill->width = width;
    fill->height = height;
    fill->color_rgb565 = color_rgb565;
    fill->remaining_pixels = (uint32_t)width * (uint32_t)height;
    fill->window_sent = false;
    fill->active = fill->remaining_pixels > 0u;
    if ((x == 0u) && (y == 0u) && (width == ILI9341_WIDTH) && (height == ILI9341_HEIGHT))
    {
        g_full_clears++;
    }
    else
    {
        g_partial_clears++;
    }
}

bsp_status_t ili9341_fill_step(const ili9341_t *display, ili9341_fill_t *fill, uint16_t max_pixels)
{
    (void)display;
    if ((fill == NULL) || !fill->active)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    uint32_t chunk = (max_pixels == 0u) ? ILI9341_FILL_CHUNK_PIXELS : max_pixels;
    if (chunk > fill->remaining_pixels)
    {
        chunk = fill->remaining_pixels;
    }
    fill->remaining_pixels -= chunk;
    g_pixels_written += chunk;
    if (fill->remaining_pixels == 0u)
    {
        fill->active = false;
    }
    return BSP_STATUS_OK;
}

void ui_fallback_text_scaled_start(ui_fallback_text_op_t *op,
                                   uint16_t x,
                                   uint16_t y,
                                   const char *text,
                                   uint8_t scale,
                                   uint16_t fg_rgb565,
                                   uint16_t bg_rgb565)
{
    (void)x;
    (void)y;
    (void)fg_rgb565;
    (void)bg_rgb565;
    if (op == NULL)
    {
        return;
    }
    *op = (ui_fallback_text_op_t){0};
    if ((text == NULL) || (scale == 0u))
    {
        return;
    }
    size_t i = 0u;
    while ((text[i] != '\0') && ((i + 1u) < sizeof(op->text)))
    {
        op->text[i] = text[i];
        i++;
    }
    op->text[i] = '\0';
    op->scale = scale;
    op->active = op->text[0] != '\0';
}

bsp_status_t ui_fallback_text_scaled_step(const ili9341_t *display,
                                          ui_fallback_text_op_t *op)
{
    (void)display;
    if ((op == NULL) || !op->active)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    g_text_chars_written++;
    op->byte_index++;
    if ((op->byte_index >= (sizeof(op->text) - 1u)) || (op->text[op->byte_index] == '\0'))
    {
        op->active = false;
        return BSP_STATUS_OK;
    }
    return BSP_STATUS_BUSY;
}

static ui_product_view_t ready_view(uint32_t generation)
{
    return (ui_product_view_t){
        .state = UI_PRODUCT_STATE_READY,
        .page = UI_PRODUCT_PAGE_PRIMARY,
        .generation = generation,
        .display_ready = true,
    };
}

static int drain_render(ui_product_t *ui, const ili9341_t *display)
{
    for (uint32_t i = 0u; i < 2000u; i++)
    {
        const bsp_status_t status = ui_product_step(ui, display, false);
        if (status == BSP_STATUS_OK)
        {
            return 0;
        }
        if (status != BSP_STATUS_BUSY)
        {
            return 1;
        }
    }
    return 1;
}

static int test_quiet_pause_retains_render_state(void)
{
    int failures = 0;
    g_pixels_written = 0u;
    g_text_chars_written = 0u;
    g_full_clears = 0u;
    g_partial_clears = 0u;
    ui_product_t ui;
    ili9341_t display = {.ready = true};
    ui_product_init(&ui);
    ui_product_view_t view = ready_view(1u);
    ui_product_request(&ui, &view);
    failures += expect_true(ui_product_step(&ui, &display, false) == BSP_STATUS_BUSY,
                            "render starts");
    const uint32_t pixels_before_quiet = g_pixels_written;
    const uint32_t chars_before_quiet = g_text_chars_written;
    for (uint8_t i = 0u; i < 4u; i++)
    {
        failures += expect_true(ui_product_step(&ui, &display, true) == BSP_STATUS_BUSY,
                                "quiet pauses render");
    }
    failures += expect_true(g_pixels_written == pixels_before_quiet,
                            "quiet writes no pixels");
    failures += expect_true(g_text_chars_written == chars_before_quiet,
                            "quiet writes no text");
    failures += expect_true(drain_render(&ui, &display) == 0, "render resumes after quiet");
    return failures;
}

static int test_partial_region_and_latest_generation_wins(void)
{
    int failures = 0;
    g_pixels_written = 0u;
    g_text_chars_written = 0u;
    g_full_clears = 0u;
    g_partial_clears = 0u;
    ui_product_t ui;
    ili9341_t display = {.ready = true};
    ui_product_init(&ui);

    ui_product_view_t first = ready_view(1u);
    ui_product_request(&ui, &first);
    failures += expect_true(drain_render(&ui, &display) == 0, "first render completes");
    failures += expect_true(g_full_clears == 1u, "initial render uses full clear");

    ui_product_view_t slow = ready_view(2u);
    ui_product_request(&ui, &slow);
    failures += expect_true(ui_product_step(&ui, &display, false) == BSP_STATUS_BUSY,
                            "partial update begins");
    ui_product_view_t newest = ready_view(3u);
    newest.battery_state = UI_PRODUCT_BATTERY_LOW;
    ui_product_request(&ui, &newest);
    failures += expect_true(drain_render(&ui, &display) == 0, "coalesced render completes");
    failures += expect_true(g_partial_clears >= 1u, "same-screen update uses partial clear");
    failures += expect_true(g_full_clears == 1u, "same-screen update does not full clear");
    failures += expect_true(ui_product_step(&ui, &display, false) == BSP_STATUS_OK,
                            "latest generation rendered and queue is bounded");
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_quiet_pause_retains_render_state();
    failures += test_partial_region_and_latest_generation_wins();
    return failures == 0 ? 0 : 1;
}
