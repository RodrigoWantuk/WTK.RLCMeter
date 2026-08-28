#include "ui/ui_product.h"

#include <stddef.h>

#include "ui/ui_text.h"
#include "wtk_build_config.h"

#include "ui/ui_fallback_renderer.h"
#include "ui/ui_format.h"

enum
{
    UI_COLOR_BLACK = 0x0000u,
    UI_COLOR_WHITE = 0xFFFFu,
    UI_COLOR_AMBER = 0xFFE0u,
    UI_COLOR_RED = 0xF800u,
    UI_COLOR_CYAN = 0x07FFu,
    UI_COLOR_GREEN = 0x07E0u,
};

static const char *freq_token(hw_excitation_freq_t frequency)
{
    switch (frequency)
    {
    case HW_EXCITATION_FREQ_100HZ:
        return "100HZ";
    case HW_EXCITATION_FREQ_1KHZ:
        return "1KHZ";
    case HW_EXCITATION_FREQ_10KHZ:
        return "10KHZ";
    case HW_EXCITATION_FREQ_INVALID:
    default:
        return "FREQ?";
    }
}

static const char *amp_token(hw_excitation_amp_t amplitude)
{
    switch (amplitude)
    {
    case HW_EXCITATION_AMP_100MVRMS:
        return "100MV";
    case HW_EXCITATION_AMP_500MVRMS:
        return "500MV";
    case HW_EXCITATION_AMP_INVALID:
    default:
        return "AMP?";
    }
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

static bool append_hex8(char *dst, size_t capacity, size_t *used, uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4)
    {
        if (!append_char(dst, capacity, used, hex[(value >> (uint32_t)shift) & 0x0Fu]))
        {
            return false;
        }
    }
    return true;
}

static bool append_u32(char *dst, size_t capacity, size_t *used, uint32_t value)
{
    char tmp[10];
    uint8_t count = 0u;
    do
    {
        tmp[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while ((value != 0u) && (count < (uint8_t)sizeof(tmp)));
    while (count != 0u)
    {
        count--;
        if (!append_char(dst, capacity, used, tmp[count]))
        {
            return false;
        }
    }
    return true;
}

static ui_format_status_t write_literal(const char *text, char *dst, size_t capacity)
{
    if ((text == NULL) || (dst == NULL) || (capacity == 0u))
    {
        return UI_FORMAT_STATUS_INVALID_ARG;
    }
    size_t used = 0u;
    dst[0] = '\0';
    return append_text(dst, capacity, &used, text) ? UI_FORMAT_STATUS_OK :
                                                    UI_FORMAT_STATUS_TRUNCATED;
}

static const char *wizard_standard_text(uint8_t standard)
{
    switch ((ui_product_wizard_standard_t)standard)
    {
    case UI_PRODUCT_WIZARD_STANDARD_OPEN:
        return "OPEN";
    case UI_PRODUCT_WIZARD_STANDARD_SHORT:
        return "SHORT";
    case UI_PRODUCT_WIZARD_STANDARD_LOAD:
    default:
        return "LOAD";
    }
}

static const char *range_prompt_text(hw_range_id_t range_id)
{
    switch (range_id)
    {
    case HW_RANGE_ID_10R:
        return "10R";
    case HW_RANGE_ID_100R:
        return "100R";
    case HW_RANGE_ID_1K:
        return "1K";
    case HW_RANGE_ID_10K:
        return "10K";
    case HW_RANGE_ID_100K:
        return "100K";
    case HW_RANGE_ID_1M:
        return "1M";
    case HW_RANGE_ID_INVALID:
    default:
        return "RANGE?";
    }
}

static const char *blocker_text(ui_product_blocker_t blocker)
{
    switch (blocker)
    {
    case UI_PRODUCT_BLOCK_CHARGER:
        return "REMOVE CHARGER";
    case UI_PRODUCT_BLOCK_RESIDUAL:
        return "VOLTAGE DETECTED";
    case UI_PRODUCT_BLOCK_SENSOR:
        return "SENSOR ERROR";
    case UI_PRODUCT_BLOCK_SUPPLY:
        return "SUPPLY ERROR";
    case UI_PRODUCT_BLOCK_RANGE:
        return "RANGE ERROR";
    case UI_PRODUCT_BLOCK_FAULT:
        return "FAULT";
    case UI_PRODUCT_BLOCK_NONE:
    default:
        return "SAFETY";
    }
}

static ui_format_status_t format_primary_value(const ui_product_measurement_t *result,
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
    if (!result->derived_valid)
    {
        return write_literal("n/a", dst, capacity);
    }
    switch (result->interpretation)
    {
    case MEASUREMENT_INTERPRET_RESISTIVE:
        return ui_format_resistance(result->resistance_ohms, dst, capacity);
    case MEASUREMENT_INTERPRET_CAPACITIVE:
        return result->capacitance_valid ? ui_format_capacitance(result->capacitance_f, dst, capacity) :
                                           write_literal("n/a", dst, capacity);
    case MEASUREMENT_INTERPRET_INDUCTIVE:
        return result->inductance_valid ? ui_format_inductance(result->inductance_h, dst, capacity) :
                                          write_literal("n/a", dst, capacity);
    case MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN:
    default:
        return ui_format_resistance(result->magnitude_ohms, dst, capacity);
    }
}

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t color;
    uint8_t scale;
    char text[UI_FALLBACK_TEXT_MAX_CHARS];
} ui_product_line_t;

static void line_set(ui_product_line_t *line,
                     uint16_t x,
                     uint16_t y,
                     uint8_t scale,
                     uint16_t color,
                     const char *text)
{
    if (line == NULL)
    {
        return;
    }
    *line = (ui_product_line_t){0};
    line->x = x;
    line->y = y;
    line->scale = scale;
    line->color = color;
    if (text != NULL)
    {
        size_t used = 0u;
        (void)append_text(line->text, sizeof(line->text), &used, text);
    }
}

static bool prepare_result_primary_line(const ui_product_measurement_t *result,
                                        uint8_t index,
                                        ui_product_line_t *line)
{
    if ((result == NULL) || (line == NULL))
    {
        return false;
    }
    if (index == 0u)
    {
        line_set(line, 8u, 24u, 2u, UI_COLOR_CYAN, ui_format_interpretation_token(result->interpretation));
        return true;
    }
    if (index == 1u)
    {
        char value[24] = {0};
        (void)format_primary_value(result, value, sizeof(value));
        line_set(line, 8u, 64u, 3u, UI_COLOR_WHITE, value);
        return true;
    }
    if (index != 2u)
    {
        return false;
    }
    char footer[32] = {0};
    size_t used = 0u;
    if (!append_text(footer, sizeof(footer), &used, amp_token(result->amplitude)) ||
        !append_char(footer, sizeof(footer), &used, ' ') ||
        !append_text(footer, sizeof(footer), &used, freq_token(result->frequency)))
    {
        return false;
    }
    line_set(line, 8u, 118u, 1u, UI_COLOR_GREEN, footer);
    return true;
}

static bool prepare_result_details_line(const ui_product_measurement_t *result,
                                        uint8_t index,
                                        ui_product_line_t *line)
{
    if ((result == NULL) || (line == NULL))
    {
        return false;
    }
    if (index == 0u)
    {
        line_set(line, 8u, 12u, 2u, UI_COLOR_CYAN, "DETAILS");
        return true;
    }
    char text[32] = {0};
    size_t used = 0u;
    if (index == 1u)
    {
        (void)ui_format_resistance(result->resistance_ohms, text, sizeof(text));
        char row[32] = {0};
        (void)append_text(row, sizeof(row), &used, "R ");
        (void)append_text(row, sizeof(row), &used, text);
        line_set(line, 8u, 48u, 1u, UI_COLOR_WHITE, row);
        return true;
    }
    if (index == 2u)
    {
        (void)ui_format_reactance(result->reactance_ohms, text, sizeof(text));
        char row[32] = {0};
        (void)append_text(row, sizeof(row), &used, "X ");
        (void)append_text(row, sizeof(row), &used, text);
        line_set(line, 8u, 66u, 1u, UI_COLOR_WHITE, row);
        return true;
    }
    if (index == 3u)
    {
        (void)ui_format_phase_rad(result->phase_rad, text, sizeof(text));
        char row[32] = {0};
        (void)append_text(row, sizeof(row), &used, "PHASE ");
        (void)append_text(row, sizeof(row), &used, text);
        line_set(line, 8u, 84u, 1u, UI_COLOR_WHITE, row);
        return true;
    }
    return false;
}

static bool prepare_menu_line(const ui_product_view_t *view, uint8_t index, ui_product_line_t *line)
{
    if ((view == NULL) || (line == NULL))
    {
        return false;
    }
    if (index == 0u)
    {
        line_set(line, 8u, 16u, 2u, UI_COLOR_CYAN, ui_text_fallback(UI_TEXT_ID_MENU));
        return true;
    }
    static const ui_text_id_t ids[] = {
        UI_TEXT_ID_CALIBRATION,
        UI_TEXT_ID_DISPLAY,
        UI_TEXT_ID_SOUND,
        UI_TEXT_ID_ABOUT,
        UI_TEXT_ID_BACK,
    };
    if ((index >= 1u) && (index <= (uint8_t)(sizeof(ids) / sizeof(ids[0]))))
    {
        line_set(line,
                 8u,
                 (uint16_t)(54u + ((uint16_t)(index - 1u) * 18u)),
                 1u,
                 (view->menu.selected_index == (uint8_t)(index - 1u)) ? UI_COLOR_GREEN : UI_COLOR_WHITE,
                 ui_text_fallback(ids[index - 1u]));
        return true;
    }
    return false;
}

static const char *timeout_token(uint16_t seconds, char *scratch, size_t capacity)
{
    if (seconds == 0u)
    {
        return ui_text_fallback(UI_TEXT_ID_OFF);
    }
    size_t used = 0u;
    (void)append_u32(scratch, capacity, &used, seconds);
    (void)append_char(scratch, capacity, &used, 's');
    return scratch;
}

static bool prepare_display_menu_line(const ui_product_view_t *view, uint8_t index, ui_product_line_t *line)
{
    if ((view == NULL) || (line == NULL))
    {
        return false;
    }
    if (index == 0u)
    {
        line_set(line, 8u, 16u, 2u, UI_COLOR_CYAN, ui_text_fallback(UI_TEXT_ID_DISPLAY));
        return true;
    }
    char row[32] = {0};
    size_t used = 0u;
    if (index == 1u)
    {
        (void)append_text(row, sizeof(row), &used, ui_text_fallback(UI_TEXT_ID_BRIGHTNESS));
        (void)append_char(row, sizeof(row), &used, ' ');
        (void)append_u32(row, sizeof(row), &used, view->menu.brightness_percent);
        (void)append_char(row, sizeof(row), &used, '%');
        line_set(line, 8u, 54u, 1u,
                 (view->menu.selected_index == 0u) ? UI_COLOR_GREEN : UI_COLOR_WHITE, row);
        return true;
    }
    if (index == 2u)
    {
        char timeout[8] = {0};
        (void)append_text(row, sizeof(row), &used, "TIMEOUT ");
        (void)append_text(row, sizeof(row), &used,
                          timeout_token(view->menu.timeout_seconds, timeout, sizeof(timeout)));
        line_set(line, 8u, 74u, 1u,
                 (view->menu.selected_index == 1u) ? UI_COLOR_GREEN : UI_COLOR_WHITE, row);
        return true;
    }
    if (index == 3u)
    {
        line_set(line,
                 8u,
                 94u,
                 1u,
                 (view->menu.selected_index == 2u) ? UI_COLOR_GREEN : UI_COLOR_WHITE,
                 ui_text_fallback(UI_TEXT_ID_BACK));
        return true;
    }
    if ((index == 4u) && view->menu.save_failed)
    {
        line_set(line, 8u, 122u, 1u, UI_COLOR_AMBER, ui_text_fallback(UI_TEXT_ID_SETTINGS_SAVE_FAILED));
        return true;
    }
    return false;
}

static bool prepare_sound_menu_line(const ui_product_view_t *view, uint8_t index, ui_product_line_t *line)
{
    if ((view == NULL) || (line == NULL))
    {
        return false;
    }
    if (index == 0u)
    {
        line_set(line, 8u, 16u, 2u, UI_COLOR_CYAN, ui_text_fallback(UI_TEXT_ID_SOUND));
        return true;
    }
    if (index == 1u)
    {
        line_set(line,
                 8u,
                 54u,
                 1u,
                 (view->menu.selected_index == 0u) ? UI_COLOR_GREEN : UI_COLOR_WHITE,
                 view->menu.sound_enabled ? ui_text_fallback(UI_TEXT_ID_ON) : ui_text_fallback(UI_TEXT_ID_OFF));
        return true;
    }
    if (index == 2u)
    {
        line_set(line,
                 8u,
                 74u,
                 1u,
                 (view->menu.selected_index == 1u) ? UI_COLOR_GREEN : UI_COLOR_WHITE,
                 ui_text_fallback(UI_TEXT_ID_BACK));
        return true;
    }
    if ((index == 3u) && view->menu.save_failed)
    {
        line_set(line, 8u, 122u, 1u, UI_COLOR_AMBER, ui_text_fallback(UI_TEXT_ID_SETTINGS_SAVE_FAILED));
        return true;
    }
    return false;
}

static bool prepare_about_line(uint8_t index, ui_product_line_t *line)
{
    if (line == NULL)
    {
        return false;
    }
    if (index == 0u)
    {
        line_set(line, 8u, 16u, 2u, UI_COLOR_CYAN, ui_text_fallback(UI_TEXT_ID_WTK_RLCMETER));
        return true;
    }
    if (index == 1u)
    {
        line_set(line, 8u, 54u, 1u, UI_COLOR_WHITE, WTK_PROJECT_VERSION);
        return true;
    }
    if (index == 2u)
    {
        line_set(line, 8u, 74u, 1u, UI_COLOR_WHITE, WTK_HARDWARE_COMPATIBILITY);
        return true;
    }
    if (index == 3u)
    {
        char row[28] = {0};
        size_t used = 0u;
        (void)append_text(row, sizeof(row), &used, "GIT ");
        for (uint8_t i = 0u; (i < 7u) && (WTK_GIT_COMMIT[i] != '\0'); i++)
        {
            (void)append_char(row, sizeof(row), &used, WTK_GIT_COMMIT[i]);
        }
        line_set(line, 8u, 94u, 1u, UI_COLOR_WHITE, row);
        return true;
    }
    if (index == 4u)
    {
        char row[28] = {0};
        size_t used = 0u;
        (void)append_text(row, sizeof(row), &used, "CAL SCHEMA ");
        (void)append_u32(row, sizeof(row), &used, WTK_CALIBRATION_SCHEMA_VERSION);
        line_set(line, 8u, 114u, 1u, UI_COLOR_WHITE, row);
        return true;
    }
    return false;
}

static bool prepare_calibration_status_line(const ui_product_view_t *view,
                                            uint8_t index,
                                            ui_product_line_t *line)
{
    if ((view == NULL) || (line == NULL))
    {
        return false;
    }
    if (index == 0u)
    {
        line_set(line, 8u, 16u, 2u, UI_COLOR_CYAN, ui_text_fallback(UI_TEXT_ID_CALIBRATION));
        return true;
    }
    if (index == 1u)
    {
        line_set(line,
                 8u,
                 54u,
                 1u,
                 view->calibration_active_valid ? UI_COLOR_GREEN : UI_COLOR_AMBER,
                 view->calibration_active_valid ? ui_text_fallback(UI_TEXT_ID_ACTIVE) :
                                                  ui_text_fallback(UI_TEXT_ID_REQUIRED));
        return true;
    }
    if (index == 2u)
    {
        char text[24] = {0};
        size_t used = 0u;
        (void)append_text(text, sizeof(text), &used, "SEQ ");
        (void)append_u32(text, sizeof(text), &used, view->calibration_sequence);
        line_set(line, 8u, 74u, 1u, UI_COLOR_WHITE, text);
        return true;
    }
    if (index == 3u)
    {
        line_set(line, 8u, 104u, 1u, UI_COLOR_GREEN, ui_text_fallback(UI_TEXT_ID_FULL_CALIBRATION));
        return true;
    }
    return false;
}

static bool progress_line(char *dst, size_t capacity, uint8_t a, uint8_t b)
{
    size_t used = 0u;
    return append_u32(dst, capacity, &used, a) &&
           append_text(dst, capacity, &used, " / ") &&
           append_u32(dst, capacity, &used, b);
}

static bool condition_line(const ui_product_wizard_t *wizard, char *dst, size_t capacity)
{
    size_t used = 0u;
    return append_text(dst, capacity, &used, freq_token(wizard->frequency)) &&
           append_char(dst, capacity, &used, ' ') &&
           append_text(dst, capacity, &used, amp_token(wizard->amplitude));
}

static bool prepare_wizard_line(const ui_product_wizard_t *wizard,
                                uint8_t index,
                                ui_product_line_t *line)
{
    if ((wizard == NULL) || (line == NULL))
    {
        return false;
    }
    switch ((ui_product_wizard_state_t)wizard->state)
    {
    case UI_PRODUCT_WIZARD_INTRO:
        if (index == 0u)
        {
            line_set(line, 8u, 20u, 2u, UI_COLOR_CYAN, "CALIBRATION");
            return true;
        }
        if (index == 1u)
        {
            line_set(line, 8u, 58u, 1u, UI_COLOR_WHITE, "REFERENCE KIT REQUIRED");
            return true;
        }
        return false;
    case UI_PRODUCT_WIZARD_WAIT_OPEN:
    case UI_PRODUCT_WIZARD_WAIT_SHORT:
    case UI_PRODUCT_WIZARD_WAIT_LOAD:
        if (index == 0u)
        {
            char text[24] = {0};
            size_t used = 0u;
            (void)append_text(text, sizeof(text), &used, "RANGE ");
            (void)append_text(text, sizeof(text), &used, range_prompt_text(wizard->range_id));
            line_set(line, 8u, 16u, 2u, UI_COLOR_CYAN, text);
            return true;
        }
        if (index == 1u)
        {
            const char *text = "CONNECT REF";
            if ((ui_product_wizard_state_t)wizard->state == UI_PRODUCT_WIZARD_WAIT_OPEN)
            {
                text = "OPEN TERMINALS";
            }
            else if ((ui_product_wizard_state_t)wizard->state == UI_PRODUCT_WIZARD_WAIT_SHORT)
            {
                text = "SHORT TERMINALS";
            }
            else
            {
                char row[28] = {0};
                size_t used = 0u;
                (void)append_text(row, sizeof(row), &used, "CONNECT ");
                (void)append_text(row, sizeof(row), &used, range_prompt_text(wizard->range_id));
                (void)append_text(row, sizeof(row), &used, " REF");
                line_set(line, 8u, 56u, 1u, UI_COLOR_WHITE, row);
                return true;
            }
            line_set(line, 8u, 56u, 1u, UI_COLOR_WHITE, text);
            return true;
        }
        if (index == 2u)
        {
            line_set(line, 8u, 92u, 1u, UI_COLOR_GREEN, "OK TO START");
            return true;
        }
        return false;
    case UI_PRODUCT_WIZARD_CAPTURE_OPEN:
    case UI_PRODUCT_WIZARD_CAPTURE_SHORT:
    case UI_PRODUCT_WIZARD_CAPTURE_LOAD:
        if (index == 0u)
        {
            line_set(line, 8u, 14u, 2u, UI_COLOR_WHITE, "CALIBRATING");
            return true;
        }
        if (index == 1u)
        {
            char text[24] = {0};
            size_t used = 0u;
            (void)append_text(text, sizeof(text), &used, range_prompt_text(wizard->range_id));
            (void)append_char(text, sizeof(text), &used, ' ');
            (void)append_text(text, sizeof(text), &used, wizard_standard_text(wizard->standard));
            line_set(line, 8u, 52u, 1u, UI_COLOR_CYAN, text);
            return true;
        }
        if (index == 2u)
        {
            char text[24] = {0};
            (void)condition_line(wizard, text, sizeof(text));
            line_set(line, 8u, 72u, 1u, UI_COLOR_WHITE, text);
            return true;
        }
        if (index == 3u)
        {
            char text[24] = {0};
            (void)progress_line(text,
                                sizeof(text),
                                (uint8_t)(wizard->condition_index + 1u),
                                wizard->condition_count);
            line_set(line, 8u, 92u, 1u, UI_COLOR_GREEN, text);
            return true;
        }
        return false;
    case UI_PRODUCT_WIZARD_RANGE_COMPLETE:
        if (index == 0u)
        {
            char text[24] = {0};
            size_t used = 0u;
            (void)append_text(text, sizeof(text), &used, "RANGE ");
            (void)append_text(text, sizeof(text), &used, range_prompt_text(wizard->range_id));
            line_set(line, 8u, 24u, 2u, UI_COLOR_CYAN, text);
            return true;
        }
        if (index == 1u)
        {
            line_set(line, 8u, 62u, 2u, UI_COLOR_GREEN, "COMPLETE");
            return true;
        }
        return false;
    case UI_PRODUCT_WIZARD_CONFIRM_SAVE:
        if (index == 0u)
        {
            char text[24] = {0};
            (void)progress_line(text, sizeof(text), wizard->solved_count, wizard->total_conditions);
            line_set(line, 8u, 20u, 2u, UI_COLOR_GREEN, text);
            return true;
        }
        if (index == 1u)
        {
            line_set(line, 8u, 60u, 1u, UI_COLOR_WHITE, "SAVE CALIBRATION?");
            return true;
        }
        return false;
    case UI_PRODUCT_WIZARD_COMMITTING:
        if (index == 0u)
        {
            line_set(line, 8u, 24u, 2u, UI_COLOR_WHITE, "SAVING CALIBRATION");
            return true;
        }
        return false;
    case UI_PRODUCT_WIZARD_COMPLETE:
        if (index == 0u)
        {
            line_set(line, 8u, 24u, 2u, UI_COLOR_GREEN, "CALIBRATION SAVED");
            return true;
        }
        return false;
    case UI_PRODUCT_WIZARD_SAFETY_BLOCKED:
        if (index == 0u)
        {
            line_set(line, 8u, 24u, 1u, UI_COLOR_AMBER, "SAFETY BLOCKED");
            return true;
        }
        return false;
    case UI_PRODUCT_WIZARD_CANCELING:
        if (index == 0u)
        {
            line_set(line, 8u, 24u, 2u, UI_COLOR_AMBER, "CANCELING");
            return true;
        }
        return false;
    case UI_PRODUCT_WIZARD_FAILED:
    case UI_PRODUCT_WIZARD_CANCELED:
    case UI_PRODUCT_WIZARD_IDLE:
    default:
        if (index == 0u)
        {
            line_set(line, 8u, 24u, 2u, UI_COLOR_RED, "CALIBRATION FAILED");
            return true;
        }
        if (index == 1u)
        {
            line_set(line, 8u, 62u, 1u, UI_COLOR_WHITE, "OK RETRY  LONG BACK");
            return true;
        }
        return false;
    }
}

static bool prepare_line(const ui_product_view_t *view, uint8_t index, ui_product_line_t *line)
{
    if ((view == NULL) || (line == NULL))
    {
        return false;
    }
    switch (view->state)
    {
    case UI_PRODUCT_STATE_STARTUP:
        if (index == 0u)
        {
            line_set(line, 8u, 24u, 3u, UI_COLOR_WHITE, "WTK RLC");
            return true;
        }
        return false;
    case UI_PRODUCT_STATE_SELF_TEST:
        if (index == 0u)
        {
            line_set(line, 8u, 24u, 2u, UI_COLOR_WHITE, ui_text_fallback(UI_TEXT_ID_STARTING));
            return true;
        }
        return false;
    case UI_PRODUCT_STATE_CALIBRATION_CHECK:
        if (index == 0u)
        {
            line_set(line, 8u, 24u, 2u, UI_COLOR_WHITE, ui_text_fallback(UI_TEXT_ID_CAL_CHECK));
            return true;
        }
        return false;
    case UI_PRODUCT_STATE_CALIBRATION_REQUIRED:
        if (index == 0u)
        {
            line_set(line,
                     8u,
                     24u,
                     (view->storage_unavailable ||
                      (view->calibration_status == UI_PRODUCT_CAL_STORAGE_ERROR)) ? 2u : 1u,
                     (view->storage_unavailable ||
                      (view->calibration_status == UI_PRODUCT_CAL_STORAGE_ERROR)) ? UI_COLOR_RED : UI_COLOR_AMBER,
                     (view->storage_unavailable ||
                      (view->calibration_status == UI_PRODUCT_CAL_STORAGE_ERROR)) ?
                         ui_text_fallback(UI_TEXT_ID_STORAGE_ERROR) :
                         ui_text_fallback(UI_TEXT_ID_CALIBRATION_REQUIRED));
            return true;
        }
        return false;
    case UI_PRODUCT_STATE_READY:
        if (index == 0u)
        {
            line_set(line, 8u, 24u, 3u, UI_COLOR_GREEN, ui_text_fallback(UI_TEXT_ID_READY));
            return true;
        }
        return false;
    case UI_PRODUCT_STATE_MENU:
        return prepare_menu_line(view, index, line);
    case UI_PRODUCT_STATE_DISPLAY_MENU:
    case UI_PRODUCT_STATE_BRIGHTNESS_EDIT:
    case UI_PRODUCT_STATE_TIMEOUT_EDIT:
        return prepare_display_menu_line(view, index, line);
    case UI_PRODUCT_STATE_SOUND_MENU:
        return prepare_sound_menu_line(view, index, line);
    case UI_PRODUCT_STATE_ABOUT:
        return prepare_about_line(index, line);
    case UI_PRODUCT_STATE_CALIBRATION_STATUS:
        return prepare_calibration_status_line(view, index, line);
    case UI_PRODUCT_STATE_CALIBRATION_WIZARD:
        return prepare_wizard_line(&view->wizard, index, line);
    case UI_PRODUCT_STATE_MEASURING:
        if (index == 0u)
        {
            line_set(line, 8u, 24u, 2u, UI_COLOR_WHITE, ui_text_fallback(UI_TEXT_ID_MEASURING));
            return true;
        }
        return view->has_measurement_result ?
                   prepare_result_primary_line(&view->measurement_result, (uint8_t)(index - 1u), line) :
                   false;
    case UI_PRODUCT_STATE_RESULT:
        if (!view->has_measurement_result)
        {
            if (index == 0u)
            {
                line_set(line, 8u, 24u, 3u, UI_COLOR_GREEN, ui_text_fallback(UI_TEXT_ID_READY));
                return true;
            }
            return false;
        }
        return (view->page == UI_PRODUCT_PAGE_DETAILS) ?
                   prepare_result_details_line(&view->measurement_result, index, line) :
                   prepare_result_primary_line(&view->measurement_result, index, line);
    case UI_PRODUCT_STATE_SAFETY_BLOCKED:
        if (index == 0u)
        {
            line_set(line, 8u, 24u, 1u, UI_COLOR_RED, blocker_text(view->safety_blocker));
            return true;
        }
        return false;
    case UI_PRODUCT_STATE_FAULT:
    default:
        {
            if (index != 0u)
            {
                return false;
            }
            char fault[28] = {0};
            size_t used = 0u;
            if (!append_text(fault, sizeof(fault), &used, "FAULT ") ||
                !append_hex8(fault, sizeof(fault), &used, view->safety_fault_mask))
            {
                return false;
            }
            line_set(line, 8u, 24u, 1u, UI_COLOR_RED, fault);
            return true;
        }
    }
}

static bool requires_full_clear(const ui_product_t *ui)
{
    return (ui == NULL) || !ui->have_rendered ||
           (ui->pending.state != ui->rendered.state) ||
           (ui->pending.page != ui->rendered.page);
}

static void start_render(ui_product_t *ui)
{
    if (ui == NULL)
    {
        return;
    }
    ui->rendering = ui->pending;
    ui->line_index = 0u;
    ui->text_op.active = false;
    ui->clear_started = false;
    ui->render_state = UI_PRODUCT_RENDER_CLEAR;
    ui->active = true;
}

static void start_clear_region(ui_product_t *ui)
{
    if (requires_full_clear(ui))
    {
        ili9341_fill_start(&ui->clear_fill, 0u, 0u, ILI9341_WIDTH, ILI9341_HEIGHT, UI_COLOR_BLACK);
    }
    else
    {
        ili9341_fill_start(&ui->clear_fill, 0u, 0u, ILI9341_WIDTH, 144u, UI_COLOR_BLACK);
    }
    ui->clear_started = true;
}

void ui_product_init(ui_product_t *ui)
{
    if (ui != NULL)
    {
        *ui = (ui_product_t){0};
    }
}

void ui_product_request(ui_product_t *ui, const ui_product_view_t *view)
{
    if ((ui == NULL) || (view == NULL) || (ui->rendered_generation == view->generation))
    {
        return;
    }
    ui->pending = *view;
    if (!ui->active)
    {
        start_render(ui);
    }
}

bsp_status_t ui_product_step(ui_product_t *ui, const ili9341_t *display, bool quiet)
{
    if ((ui == NULL) || (display == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!ui->active || !display->ready)
    {
        return BSP_STATUS_OK;
    }
    if (quiet)
    {
        return BSP_STATUS_BUSY;
    }
    if (ui->render_state == UI_PRODUCT_RENDER_CLEAR)
    {
        if (!ui->clear_started)
        {
            start_clear_region(ui);
        }
        const bsp_status_t clear_status = ili9341_fill_step(display,
                                                            &ui->clear_fill,
                                                            ILI9341_FILL_CHUNK_PIXELS);
        if (clear_status != BSP_STATUS_OK)
        {
            ui->active = false;
            ui->render_state = UI_PRODUCT_RENDER_IDLE;
            return clear_status;
        }
        if (ui->clear_fill.active)
        {
            return BSP_STATUS_BUSY;
        }
        ui->render_state = UI_PRODUCT_RENDER_TEXT;
        return BSP_STATUS_BUSY;
    }

    if (ui->render_state != UI_PRODUCT_RENDER_TEXT)
    {
        ui->active = false;
        ui->render_state = UI_PRODUCT_RENDER_IDLE;
        return BSP_STATUS_ERROR;
    }

    if (ui->text_op.active)
    {
        const bsp_status_t text_status = ui_fallback_text_scaled_step(display, &ui->text_op);
        if ((text_status != BSP_STATUS_OK) && (text_status != BSP_STATUS_BUSY))
        {
            ui->active = false;
            ui->render_state = UI_PRODUCT_RENDER_IDLE;
            return text_status;
        }
        if (ui->text_op.active || (text_status == BSP_STATUS_BUSY))
        {
            return BSP_STATUS_BUSY;
        }
        ui->line_index++;
        return BSP_STATUS_BUSY;
    }

    if (ui->pending.generation != ui->rendering.generation)
    {
        start_render(ui);
        return BSP_STATUS_BUSY;
    }

    ui_product_line_t line;
    if (!prepare_line(&ui->rendering, ui->line_index, &line))
    {
        ui->rendered = ui->rendering;
        ui->rendered_generation = ui->rendering.generation;
        ui->have_rendered = true;
        ui->active = false;
        ui->render_state = UI_PRODUCT_RENDER_IDLE;
        if (ui->pending.generation != ui->rendered_generation)
        {
            start_render(ui);
            return BSP_STATUS_BUSY;
        }
        return BSP_STATUS_OK;
    }
    ui_fallback_text_scaled_start(&ui->text_op,
                                  line.x,
                                  line.y,
                                  line.text,
                                  line.scale,
                                  line.color,
                                  UI_COLOR_BLACK);
    if (!ui->text_op.active)
    {
        ui->line_index++;
    }
    return BSP_STATUS_BUSY;
}

uint32_t ui_product_context_size_bytes(void)
{
    return (uint32_t)sizeof(ui_product_t);
}

const char *ui_product_state_string(ui_product_state_t state)
{
    switch (state)
    {
    case UI_PRODUCT_STATE_STARTUP:
        return "STARTUP";
    case UI_PRODUCT_STATE_SELF_TEST:
        return "SELF_TEST";
    case UI_PRODUCT_STATE_CALIBRATION_CHECK:
        return "CALIBRATION_CHECK";
    case UI_PRODUCT_STATE_CALIBRATION_REQUIRED:
        return "CALIBRATION_REQUIRED";
    case UI_PRODUCT_STATE_READY:
        return "READY";
    case UI_PRODUCT_STATE_MENU:
        return "MENU";
    case UI_PRODUCT_STATE_DISPLAY_MENU:
        return "DISPLAY_MENU";
    case UI_PRODUCT_STATE_BRIGHTNESS_EDIT:
        return "BRIGHTNESS_EDIT";
    case UI_PRODUCT_STATE_TIMEOUT_EDIT:
        return "TIMEOUT_EDIT";
    case UI_PRODUCT_STATE_SOUND_MENU:
        return "SOUND_MENU";
    case UI_PRODUCT_STATE_ABOUT:
        return "ABOUT";
    case UI_PRODUCT_STATE_CALIBRATION_STATUS:
        return "CALIBRATION_STATUS";
    case UI_PRODUCT_STATE_CALIBRATION_WIZARD:
        return "CALIBRATION_WIZARD";
    case UI_PRODUCT_STATE_MEASURING:
        return "MEASURING";
    case UI_PRODUCT_STATE_RESULT:
        return "RESULT";
    case UI_PRODUCT_STATE_SAFETY_BLOCKED:
        return "SAFETY_BLOCKED";
    case UI_PRODUCT_STATE_FAULT:
    default:
        return "FAULT";
    }
}
