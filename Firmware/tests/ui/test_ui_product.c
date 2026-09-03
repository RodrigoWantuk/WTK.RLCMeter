#include "ui/ui_product.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static uint32_t g_pixels_written;
static uint32_t g_text_chars_written;
static uint32_t g_text_start_count;
static uint32_t g_full_clears;
static uint32_t g_partial_clears;
static char g_started_text[16][UI_FALLBACK_TEXT_MAX_CHARS];

static resource_status_t g_provider_status;
static uint32_t g_provider_calls;

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
    if (g_text_start_count < (uint32_t)(sizeof(g_started_text) / sizeof(g_started_text[0])))
    {
        (void)snprintf(g_started_text[g_text_start_count],
                       sizeof(g_started_text[g_text_start_count]),
                       "%s",
                       text);
    }
    g_text_start_count++;
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

static void reset_render_counters(void)
{
    g_pixels_written = 0u;
    g_text_chars_written = 0u;
    g_text_start_count = 0u;
    g_full_clears = 0u;
    g_partial_clears = 0u;
    (void)memset(g_started_text, 0, sizeof(g_started_text));
    g_provider_status = RESOURCE_STATUS_OK;
    g_provider_calls = 0u;
}

static resource_status_t fake_text_provider(void *context,
                                            ui_language_id_t language,
                                            ui_text_id_t id,
                                            char *dst,
                                            size_t capacity)
{
    (void)context;
    g_provider_calls++;
    if (g_provider_status != RESOURCE_STATUS_OK)
    {
        return g_provider_status;
    }
    const char *text = "?";
    if (id == UI_TEXT_ID_DETAILS)
    {
        text = (language == UI_LANGUAGE_PT_BR) ? "DETALHES" : "DETAILS";
    }
    else if (id == UI_TEXT_ID_PHASE)
    {
        text = (language == UI_LANGUAGE_PT_BR) ? "FASE" : "PHASE";
    }
    else if (id == UI_TEXT_ID_MENU)
    {
        text = "MENU";
    }
    else if (id == UI_TEXT_ID_READY)
    {
        text = "READY";
    }
    if (snprintf(dst, capacity, "%s", text) >= (int)capacity)
    {
        return RESOURCE_STATUS_OUT_OF_RANGE;
    }
    return RESOURCE_STATUS_OK;
}

static bool rendered_text_starts_with(const char *prefix)
{
    for (uint32_t i = 0u; i < g_text_start_count; i++)
    {
        if (strncmp(g_started_text[i], prefix, strlen(prefix)) == 0)
        {
            return true;
        }
    }
    return false;
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
    reset_render_counters();
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
    reset_render_counters();
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

static int test_details_phase_label_uses_catalog(void)
{
    int failures = 0;
    reset_render_counters();
    ui_product_t ui;
    ili9341_t display = {.ready = true};
    ui_product_init(&ui);
    ui_product_set_text_provider(&ui, fake_text_provider, NULL);
    ui_product_view_t view = ready_view(10u);
    view.state = UI_PRODUCT_STATE_RESULT;
    view.page = UI_PRODUCT_PAGE_DETAILS;
    view.menu.language_id = (uint8_t)UI_LANGUAGE_PT_BR;
    view.has_measurement_result = true;
    view.measurement_result = (ui_product_measurement_t){
        .status = MEASUREMENT_AUTO_STATUS_FINAL_OK,
        .interpretation = MEASUREMENT_INTERPRET_RESISTIVE,
        .frequency = HW_EXCITATION_FREQ_1KHZ,
        .amplitude = HW_EXCITATION_AMP_100MVRMS,
        .resistance_ohms = 1000.0f,
        .reactance_ohms = 0.0f,
        .magnitude_ohms = 1000.0f,
        .phase_rad = 0.0f,
        .derived_valid = true,
    };
    ui_product_request(&ui, &view);
    failures += expect_true(drain_render(&ui, &display) == 0, "details render drains");
    failures += expect_true(rendered_text_starts_with("FASE "), "PHASE label is localized");
    return failures;
}

static int test_resource_error_uses_no_external_text_reads(void)
{
    int failures = 0;
    reset_render_counters();
    ui_product_t ui;
    ili9341_t display = {.ready = true};
    ui_product_init(&ui);
    ui_product_set_text_provider(&ui, fake_text_provider, NULL);
    ui_product_view_t view = ready_view(20u);
    view.state = UI_PRODUCT_STATE_RESOURCE_ERROR;
    view.resource_status = RESOURCE_STATUS_CORRUPT;
    ui_product_request(&ui, &view);
    failures += expect_true(drain_render(&ui, &display) == 0, "resource error render drains");
    failures += expect_true(g_provider_calls == 0u, "resource error uses emergency internal text only");
    failures += expect_true(rendered_text_starts_with("RESOURCE ERROR"), "emergency text drawn");
    return failures;
}

static int test_normal_resource_failure_is_not_silent_success(void)
{
    int failures = 0;
    reset_render_counters();
    g_provider_status = RESOURCE_STATUS_CORRUPT;
    ui_product_t ui;
    ili9341_t display = {.ready = true};
    ui_product_init(&ui);
    ui_product_set_text_provider(&ui, fake_text_provider, NULL);
    ui_product_view_t view = ready_view(30u);
    view.state = UI_PRODUCT_STATE_MENU;
    ui_product_request(&ui, &view);
    bsp_status_t status = BSP_STATUS_BUSY;
    for (uint32_t i = 0u; i < 2000u && status == BSP_STATUS_BUSY; i++)
    {
        status = ui_product_step(&ui, &display, false);
    }
    failures += expect_true(status == BSP_STATUS_ERROR, "normal resource corruption fails render");
    failures += expect_true(!rendered_text_starts_with("?"), "normal fatal resource error is not '?'");
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_quiet_pause_retains_render_state();
    failures += test_partial_region_and_latest_generation_wins();
    failures += test_details_phase_label_uses_catalog();
    failures += test_resource_error_uses_no_external_text_reads();
    failures += test_normal_resource_failure_is_not_silent_success();
    return failures == 0 ? 0 : 1;
}
