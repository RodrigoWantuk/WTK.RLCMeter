#ifndef WTK_UI_PERIPHERAL_TEST_H
#define WTK_UI_PERIPHERAL_TEST_H

#include "drivers/buttons.h"
#include "ui/ui_fallback_renderer.h"

typedef struct
{
    ili9341_fill_t fill;
    ui_fallback_text_op_t text;
    uint8_t stage;
    uint8_t pressed_mask;
    uint8_t dirty_mask;
    bool failed;
} ui_peripheral_test_t;

void ui_peripheral_test_init(ui_peripheral_test_t *ui);
void ui_peripheral_test_button(ui_peripheral_test_t *ui, const button_event_t *event);
bsp_status_t ui_peripheral_test_step(ui_peripheral_test_t *ui,
                                     const ili9341_t *display,
                                     const char *firmware,
                                     const char *git,
                                     bool quiet);

#endif
