#include "ui/ui_format.h"

#include <math.h>
#include <stdint.h>

enum
{
    UI_FORMAT_DECIMAL_SCALE = 10,
    UI_FORMAT_DEG_PER_RAD_X10 = 573,
};

static float absf_local(float value)
{
    return (value < 0.0f) ? -value : value;
}

static bool append_char(char *dst, size_t capacity, size_t *used, char ch)
{
    if ((dst == NULL) || (used == NULL) || ((*used + 1u) >= capacity))
    {
        return false;
    }
    dst[*used] = ch;
    (*used)++;
    dst[*used] = '\0';
    return true;
}

static bool append_text(char *dst, size_t capacity, size_t *used, const char *text)
{
    if (text == NULL)
    {
        return false;
    }
    for (size_t i = 0u; text[i] != '\0'; i++)
    {
        if (!append_char(dst, capacity, used, text[i]))
        {
            return false;
        }
    }
    return true;
}

static bool append_u32(char *dst, size_t capacity, size_t *used, uint32_t value)
{
    char tmp[10];
    size_t count = 0u;
    do
    {
        tmp[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while ((value != 0u) && (count < sizeof(tmp)));
    while (count > 0u)
    {
        count--;
        if (!append_char(dst, capacity, used, tmp[count]))
        {
            return false;
        }
    }
    return true;
}

static ui_format_status_t write_unavailable(char *dst, size_t capacity)
{
    if ((dst == NULL) || (capacity == 0u))
    {
        return UI_FORMAT_STATUS_INVALID_ARG;
    }
    size_t used = 0u;
    dst[0] = '\0';
    return append_text(dst, capacity, &used, "n/a") ? UI_FORMAT_STATUS_UNAVAILABLE :
                                                      UI_FORMAT_STATUS_TRUNCATED;
}

static ui_format_status_t write_literal(const char *text, char *dst, size_t capacity)
{
    if ((dst == NULL) || (capacity == 0u))
    {
        return UI_FORMAT_STATUS_INVALID_ARG;
    }
    size_t used = 0u;
    dst[0] = '\0';
    return append_text(dst, capacity, &used, text) ? UI_FORMAT_STATUS_OK :
                                                    UI_FORMAT_STATUS_TRUNCATED;
}

static ui_format_status_t write_scaled(float value, float scale, const char *unit, char *dst, size_t capacity)
{
    if ((dst == NULL) || (unit == NULL) || (capacity == 0u) || !isfinite(value) || (scale <= 0.0f))
    {
        return write_unavailable(dst, capacity);
    }
    size_t used = 0u;
    dst[0] = '\0';
    if ((value < 0.0f) && !append_char(dst, capacity, &used, '-'))
    {
        return UI_FORMAT_STATUS_TRUNCATED;
    }
    const float scaled_abs = absf_local(value) / scale;
    const uint32_t x10 = (uint32_t)((scaled_abs * (float)UI_FORMAT_DECIMAL_SCALE) + 0.5f);
    const uint32_t whole = x10 / UI_FORMAT_DECIMAL_SCALE;
    const uint32_t frac = x10 % UI_FORMAT_DECIMAL_SCALE;
    if (!append_u32(dst, capacity, &used, whole) ||
        !append_char(dst, capacity, &used, '.') ||
        !append_u32(dst, capacity, &used, frac) ||
        !append_char(dst, capacity, &used, ' ') ||
        !append_text(dst, capacity, &used, unit))
    {
        return UI_FORMAT_STATUS_TRUNCATED;
    }
    return UI_FORMAT_STATUS_OK;
}

ui_format_status_t ui_format_resistance(float ohms, char *dst, size_t capacity)
{
    if (!isfinite(ohms))
    {
        return write_unavailable(dst, capacity);
    }
    const float magnitude = absf_local(ohms);
    if (magnitude >= 1000000.0f)
    {
        return write_scaled(ohms, 1000000.0f, "MOhm", dst, capacity);
    }
    if (magnitude >= 1000.0f)
    {
        return write_scaled(ohms, 1000.0f, "kOhm", dst, capacity);
    }
    return write_scaled(ohms, 1.0f, "Ohm", dst, capacity);
}

ui_format_status_t ui_format_reactance(float ohms, char *dst, size_t capacity)
{
    return ui_format_resistance(ohms, dst, capacity);
}

ui_format_status_t ui_format_capacitance(float farads, char *dst, size_t capacity)
{
    if (!isfinite(farads) || (farads <= 0.0f))
    {
        return write_unavailable(dst, capacity);
    }
    if (farads < 1.0e-9f)
    {
        return write_scaled(farads, 1.0e-12f, "pF", dst, capacity);
    }
    if (farads < 1.0e-6f)
    {
        return write_scaled(farads, 1.0e-9f, "nF", dst, capacity);
    }
    if (farads < 1.0e-3f)
    {
        return write_scaled(farads, 1.0e-6f, "uF", dst, capacity);
    }
    return write_scaled(farads, 1.0e-3f, "mF", dst, capacity);
}

ui_format_status_t ui_format_inductance(float henries, char *dst, size_t capacity)
{
    if (!isfinite(henries) || (henries <= 0.0f))
    {
        return write_unavailable(dst, capacity);
    }
    if (henries < 1.0e-3f)
    {
        return write_scaled(henries, 1.0e-6f, "uH", dst, capacity);
    }
    if (henries < 1.0f)
    {
        return write_scaled(henries, 1.0e-3f, "mH", dst, capacity);
    }
    return write_scaled(henries, 1.0f, "H", dst, capacity);
}

ui_format_status_t ui_format_phase_rad(float radians, char *dst, size_t capacity)
{
    if (!isfinite(radians))
    {
        return write_unavailable(dst, capacity);
    }
    const int32_t deg_x10 = (int32_t)((radians * (float)UI_FORMAT_DEG_PER_RAD_X10) +
                                      ((radians >= 0.0f) ? 0.5f : -0.5f));
    return write_scaled((float)deg_x10 / 10.0f, 1.0f, "deg", dst, capacity);
}

ui_format_status_t ui_format_q(float value, char *dst, size_t capacity)
{
    return isfinite(value) ? write_scaled(value, 1.0f, "Q", dst, capacity) :
                             write_unavailable(dst, capacity);
}

ui_format_status_t ui_format_d(float value, char *dst, size_t capacity)
{
    return isfinite(value) ? write_scaled(value, 1.0f, "D", dst, capacity) :
                             write_unavailable(dst, capacity);
}

ui_format_status_t ui_format_primary_value(const measurement_session_result_t *result,
                                           char *dst,
                                           size_t capacity)
{
    if ((result == NULL) || (dst == NULL) || (capacity == 0u))
    {
        return UI_FORMAT_STATUS_INVALID_ARG;
    }
    if (result->status == MEASUREMENT_AUTO_STATUS_OPEN_LIKE)
    {
        return write_literal("OPEN", dst, capacity);
    }
    if (result->status == MEASUREMENT_AUTO_STATUS_SHORT_LIKE)
    {
        return write_literal("SHORT", dst, capacity);
    }
    if (!result->primary_attempt.derived.valid)
    {
        return write_unavailable(dst, capacity);
    }

    switch (result->classification.interpretation)
    {
    case MEASUREMENT_INTERPRET_RESISTIVE:
        return ui_format_resistance(result->primary_attempt.derived.resistance_ohms, dst, capacity);
    case MEASUREMENT_INTERPRET_CAPACITIVE:
        return result->primary_attempt.derived.capacitance_valid ?
                   ui_format_capacitance(result->primary_attempt.derived.capacitance_f, dst, capacity) :
                   write_unavailable(dst, capacity);
    case MEASUREMENT_INTERPRET_INDUCTIVE:
        return result->primary_attempt.derived.inductance_valid ?
                   ui_format_inductance(result->primary_attempt.derived.inductance_h, dst, capacity) :
                   write_unavailable(dst, capacity);
    case MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN:
    default:
        return ui_format_resistance(result->primary_attempt.derived.magnitude_ohms, dst, capacity);
    }
}

const char *ui_format_interpretation_token(measurement_interpretation_t interpretation)
{
    switch (interpretation)
    {
    case MEASUREMENT_INTERPRET_RESISTIVE:
        return "RESISTOR";
    case MEASUREMENT_INTERPRET_CAPACITIVE:
        return "CAPACITOR";
    case MEASUREMENT_INTERPRET_INDUCTIVE:
        return "INDUCTOR";
    case MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}
