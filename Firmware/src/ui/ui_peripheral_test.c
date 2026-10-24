#include "ui/ui_peripheral_test.h"

#include <stddef.h>
#include <string.h>

void ui_peripheral_test_init(ui_peripheral_test_t *ui)
{
    if (ui != NULL)
    {
        memset(ui, 0, sizeof(*ui));
        ui->dirty_mask = 7u;
    }
}

void ui_peripheral_test_button(ui_peripheral_test_t *ui, const button_event_t *event)
{
    if (ui == NULL || event == NULL || (unsigned)event->button >= BUTTON_ID_COUNT)
        return;
    if (event->type != BUTTON_EVENT_PRESS && event->type != BUTTON_EVENT_RELEASE)
        return;
    const uint8_t bit = (uint8_t)(1u << (unsigned)event->button);
    if (event->type == BUTTON_EVENT_PRESS)
        ui->pressed_mask |= bit;
    else
        ui->pressed_mask &= (uint8_t)~bit;
    ui->dirty_mask |= bit;
}

bsp_status_t ui_peripheral_test_step(ui_peripheral_test_t *ui,
                                     const ili9341_t *display,
                                     const char *firmware,
                                     const char *git,
                                     bool quiet)
{
    if (ui == NULL || display == NULL || firmware == NULL || git == NULL)
        return BSP_STATUS_INVALID_ARG;
    if (ui->failed)
        return BSP_STATUS_ERROR;
    if (quiet || !display->ready)
        return BSP_STATUS_BUSY;

    bsp_status_t status = BSP_STATUS_OK;
    if (ui->stage < 4u)
    {
        static const uint16_t colors[] = {0x0000u, 0xF800u, 0x07E0u, 0x001Fu};
        if (!ui->fill.active)
        {
            const bool clear = ui->stage == 0u;
            ili9341_fill_start(&ui->fill,
                clear ? 0u : (uint16_t)((ui->stage - 1u) * 80u), 0u,
                clear ? 240u : 80u, clear ? 320u : 64u, colors[ui->stage]);
        }
        status = ili9341_fill_step(display, &ui->fill, ILI9341_FILL_CHUNK_PIXELS);
        if (status == BSP_STATUS_OK && !ui->fill.active)
            ui->stage++;
    }
    else if (ui->stage < 8u)
    {
        if (!ui->text.active)
        {
            const char *line = ui->stage == 4u ? "WTK RLC SETUP" :
                ui->stage == 5u ? firmware : ui->stage == 6u ? git : "BRINGUP PERIPHERAL";
            ui_fallback_text_scaled_start(&ui->text, 8u,
                (uint16_t)(80u + (ui->stage - 4u) * 24u), line,
                ui->stage == 7u ? 1u : 2u, 0xFFFFu, 0u);
        }
        status = ui_fallback_text_scaled_step(display, &ui->text);
        if (status == BSP_STATUS_OK && !ui->text.active)
            ui->stage++;
    }
    else
    {
        if (!ui->text.active)
        {
            static const char *const released[] = {"UP:   RELEASED", "OK:   RELEASED", "DOWN: RELEASED"};
            static const char *const pressed[] = {"UP:   PRESSED ", "OK:   PRESSED ", "DOWN: PRESSED "};
            unsigned row = 0u;
            while (row < BUTTON_ID_COUNT && (ui->dirty_mask & (1u << row)) == 0u)
                row++;
            if (row == BUTTON_ID_COUNT)
                return BSP_STATUS_OK;
            const uint8_t bit = (uint8_t)(1u << row);
            const bool down = (ui->pressed_mask & bit) != 0u;
            /* Clear at start: a new event during rendering must redraw the row. */
            ui->dirty_mask &= (uint8_t)~bit;
            ui_fallback_text_scaled_start(&ui->text, 8u, (uint16_t)(184u + row * 32u),
                down ? pressed[row] : released[row], 2u, down ? 0x07E0u : 0xFFFFu, 0u);
        }
        status = ui_fallback_text_scaled_step(display, &ui->text);
    }
    if (status != BSP_STATUS_OK && status != BSP_STATUS_BUSY)
        ui->failed = true;
    return status;
}
