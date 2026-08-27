#include "ui/ui_format.h"

#include <stdio.h>
#include <string.h>

static int expect_text(const char *actual, const char *expected, const char *message)
{
    if (strcmp(actual, expected) != 0)
    {
        (void)fprintf(stderr, "FAIL: %s (got '%s' expected '%s')\n", message, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_status(ui_format_status_t actual, ui_format_status_t expected, const char *message)
{
    if (actual != expected)
    {
        (void)fprintf(stderr, "FAIL: %s (got %u expected %u)\n", message, (unsigned)actual, (unsigned)expected);
        return 1;
    }
    return 0;
}

static int test_si_units(void)
{
    int failures = 0;
    char text[24];
    failures += expect_status(ui_format_resistance(987.34f, text, sizeof(text)), UI_FORMAT_STATUS_OK, "ohm status");
    failures += expect_text(text, "987.3 Ohm", "ohm text");
    failures += expect_status(ui_format_resistance(12340.0f, text, sizeof(text)), UI_FORMAT_STATUS_OK, "kohm status");
    failures += expect_text(text, "12.3 kOhm", "kohm text");
    failures += expect_status(ui_format_resistance(2200000.0f, text, sizeof(text)), UI_FORMAT_STATUS_OK, "Mohm status");
    failures += expect_text(text, "2.2 MOhm", "Mohm text");
    failures += expect_status(ui_format_capacitance(0.000001f, text, sizeof(text)), UI_FORMAT_STATUS_OK, "cap status");
    failures += expect_text(text, "1.0 uF", "cap text");
    failures += expect_status(ui_format_inductance(0.00047f, text, sizeof(text)), UI_FORMAT_STATUS_OK, "ind status");
    failures += expect_text(text, "470.0 uH", "ind text");
    failures += expect_status(ui_format_phase_rad(1.5707963f, text, sizeof(text)), UI_FORMAT_STATUS_OK, "phase status");
    failures += expect_text(text, "90.0 deg", "phase text");
    return failures;
}

static int test_unavailable_and_primary(void)
{
    int failures = 0;
    char text[24];
    failures += expect_status(ui_format_capacitance(-1.0f, text, sizeof(text)),
                              UI_FORMAT_STATUS_UNAVAILABLE,
                              "negative capacitance status");
    failures += expect_text(text, "n/a", "negative capacitance text");

    measurement_session_result_t result = {0};
    result.status = MEASUREMENT_AUTO_STATUS_FINAL_OK;
    result.classification.interpretation = MEASUREMENT_INTERPRET_CAPACITIVE;
    result.primary_attempt.derived.valid = true;
    result.primary_attempt.derived.capacitance_valid = true;
    result.primary_attempt.derived.capacitance_f = 1.0e-9f;
    failures += expect_status(ui_format_primary_value(&result, text, sizeof(text)),
                              UI_FORMAT_STATUS_OK,
                              "primary cap status");
    failures += expect_text(text, "1.0 nF", "primary cap text");

    result.status = MEASUREMENT_AUTO_STATUS_OPEN_LIKE;
    failures += expect_status(ui_format_primary_value(&result, text, sizeof(text)),
                              UI_FORMAT_STATUS_OK,
                              "open status");
    failures += expect_text(text, "OPEN", "open text");
    result.status = MEASUREMENT_AUTO_STATUS_SHORT_LIKE;
    failures += expect_status(ui_format_primary_value(&result, text, sizeof(text)),
                              UI_FORMAT_STATUS_OK,
                              "short status");
    failures += expect_text(text, "SHORT", "short text");
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_si_units();
    failures += test_unavailable_and_primary();
    return failures == 0 ? 0 : 1;
}
