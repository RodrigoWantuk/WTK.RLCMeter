#ifndef WTK_UI_FORMAT_H
#define WTK_UI_FORMAT_H

#include <stdbool.h>
#include <stddef.h>

#include "measurement/measurement_dsp.h"
#include "measurement/measurement_engine.h"

typedef enum
{
    UI_FORMAT_STATUS_OK = 0,
    UI_FORMAT_STATUS_UNAVAILABLE,
    UI_FORMAT_STATUS_TRUNCATED,
    UI_FORMAT_STATUS_INVALID_ARG,
} ui_format_status_t;

ui_format_status_t ui_format_resistance(float ohms, char *dst, size_t capacity);
ui_format_status_t ui_format_reactance(float ohms, char *dst, size_t capacity);
ui_format_status_t ui_format_capacitance(float farads, char *dst, size_t capacity);
ui_format_status_t ui_format_inductance(float henries, char *dst, size_t capacity);
ui_format_status_t ui_format_phase_rad(float radians, char *dst, size_t capacity);
ui_format_status_t ui_format_q(float value, char *dst, size_t capacity);
ui_format_status_t ui_format_d(float value, char *dst, size_t capacity);
ui_format_status_t ui_format_primary_value(const measurement_session_result_t *result,
                                           char *dst,
                                           size_t capacity);
const char *ui_format_interpretation_token(measurement_interpretation_t interpretation);

#endif
