#include "ui/ui_peripheral_test.h"

#include <stdio.h>
#include <string.h>

static unsigned g_io;
static bool g_error;
static char g_last_text[UI_FALLBACK_TEXT_MAX_CHARS];

static int check(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

void ili9341_fill_start(ili9341_fill_t *fill, uint16_t x, uint16_t y,
                        uint16_t width, uint16_t height, uint16_t color)
{
    (void)x; (void)y; (void)color;
    fill->remaining_pixels = (uint32_t)width * height;
    fill->active = true;
}

bsp_status_t ili9341_fill_step(const ili9341_t *display, ili9341_fill_t *fill, uint16_t pixels)
{
    (void)display; (void)pixels;
    g_io++;
    if (g_error) return BSP_STATUS_ERROR;
    fill->active = false;
    return BSP_STATUS_OK;
}

void ui_fallback_text_scaled_start(ui_fallback_text_op_t *op, uint16_t x, uint16_t y,
                                   const char *text, uint8_t scale, uint16_t fg, uint16_t bg)
{
    (void)x; (void)y; (void)scale; (void)fg; (void)bg;
    (void)snprintf(op->text, sizeof(op->text), "%s", text);
    (void)snprintf(g_last_text, sizeof(g_last_text), "%s", text);
    op->active = true;
    op->byte_index = 0u;
}

bsp_status_t ui_fallback_text_scaled_step(const ili9341_t *display, ui_fallback_text_op_t *op)
{
    (void)display;
    g_io++;
    if (g_error) return BSP_STATUS_ERROR;
    op->byte_index++;
    if (op->text[op->byte_index] == '\0') op->active = false;
    return BSP_STATUS_OK;
}

static void drain(ui_peripheral_test_t *ui, const ili9341_t *display)
{
    for (unsigned i = 0u; i < 300u; i++)
        (void)ui_peripheral_test_step(ui, display, "FW TEST", "GIT TEST", false);
}

int main(void)
{
    int failures = 0;
    ui_peripheral_test_t ui;
    ui_peripheral_test_init(&ui);
    ili9341_t display = {0};
    failures += check(ui_peripheral_test_step(&ui, &display, "FW", "GIT", false) == BSP_STATUS_BUSY && g_io == 0u,
                      "unready display performs no bus activity");
    display.ready = true;
    (void)ui_peripheral_test_step(&ui, &display, "FW", "GIT", false);
    const unsigned before = g_io;
    const ui_peripheral_test_t snapshot = ui;
    for (unsigned i = 0u; i < 10u; i++)
        (void)ui_peripheral_test_step(&ui, &display, "FW", "GIT", true);
    failures += check(g_io == before && memcmp(&ui, &snapshot, sizeof(ui)) == 0,
                      "quiet acquisition pauses an in-progress panel without IO or lost state");
    drain(&ui, &display);
    failures += check(ui.stage == 8u && ui.dirty_mask == 0u,
                      "startup colors and metadata reach idle without UART");
    button_event_t event = {.button = BUTTON_ID_UP, .type = BUTTON_EVENT_PRESS};
    ui_peripheral_test_button(&ui, &event);
    (void)ui_peripheral_test_step(&ui, &display, "FW", "GIT", false);
    failures += check(strcmp(g_last_text, "UP:   PRESSED ") == 0, "UP pressed row begins rendering");
    event.type = BUTTON_EVENT_RELEASE;
    ui_peripheral_test_button(&ui, &event);
    event.button = BUTTON_ID_DOWN;
    event.type = BUTTON_EVENT_PRESS;
    ui_peripheral_test_button(&ui, &event);
    failures += check(ui.pressed_mask == 4u && ui.dirty_mask == 5u,
                      "release during rendering and another simultaneous key remain pending");
    drain(&ui, &display);
    failures += check(ui.dirty_mask == 0u && strcmp(g_last_text, "DOWN: PRESSED ") == 0,
                      "pending events are eventually displayed without dropping a row");
    event.button = BUTTON_ID_OK;
    ui_peripheral_test_button(&ui, &event);
    drain(&ui, &display);
    failures += check(strcmp(g_last_text, "OK:   PRESSED ") == 0 && ui.pressed_mask == 6u,
                      "OK and DOWN state are independently retained");
    event.type = BUTTON_EVENT_REPEAT;
    ui_peripheral_test_button(&ui, &event);
    failures += check(ui.dirty_mask == 0u, "repeat does not invent a new press or release");
    ui_peripheral_test_init(&ui);
    g_error = true;
    failures += check(ui_peripheral_test_step(&ui, &display, "FW", "GIT", false) == BSP_STATUS_ERROR,
                      "bus failure is explicit");
    const unsigned after_error = g_io;
    drain(&ui, &display);
    failures += check(ui.failed && g_io == after_error, "failed display does not create an unbounded IO retry loop");
    return failures == 0 ? 0 : 1;
}
