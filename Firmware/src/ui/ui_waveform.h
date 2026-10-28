#ifndef WTK_UI_WAVEFORM_H
#define WTK_UI_WAVEFORM_H
#include "drivers/ili9341.h"
#include "measurement/measurement_waveform.h"

enum { UI_WAVEFORM_X = 40u, UI_WAVEFORM_Y = 62u,
       UI_WAVEFORM_WIDTH = 240u, UI_WAVEFORM_HEIGHT = 108u };
typedef struct
{
    float mv_div[2];
    uint16_t column, row;
} ui_waveform_t;

void ui_waveform_start(ui_waveform_t *op, const measurement_waveform_snapshot_t *snapshot);
bsp_status_t ui_waveform_step(ui_waveform_t *op, const ili9341_t *display,
                              const measurement_waveform_snapshot_t *snapshot);
#endif
