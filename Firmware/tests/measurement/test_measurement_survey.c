#include "measurement/measurement_engine.h"

#include <math.h>
#include <stdio.h>

static int check(bool condition, const char *message)
{
    if (!condition) (void)fprintf(stderr, "FAIL: %s\n", message);
    return condition ? 0 : 1;
}

static measurement_attempt_result_t point(measurement_attempt_config_t config, float r, float c, float l)
{
    const float omega = 6.28318530718f * (float)hw_excitation_frequency_hz(config.frequency);
    const measurement_complex_t z = measurement_complex(r, c > 0.0f ? -1.0f / (omega * c) : omega * l);
    const measurement_dsp_config_t dsp = measurement_dsp_config_ideal(config.range_id);
    const float denominator = measurement_complex_mag(measurement_complex_add(dsp.zref_ohms, z));
    const float vx = 0.141421f * measurement_complex_mag(z) / denominator;
    measurement_attempt_result_t result = {0};
    result.config = config;
    result.z_ohms = z;
    result.dsp_status = MEASUREMENT_STATUS_OK;
    result.derived = measurement_derive_quantities(z, hw_excitation_frequency_hz(config.frequency), &dsp,
                                                  MEASUREMENT_STATUS_OK);
    result.selected_channel = MEASUREMENT_RETURN_1X;
    result.ret_1x_quality = (measurement_channel_quality_t){.usable = true, .calibration_valid = true, .signal_peak_v = vx};
    result.ret_hg_quality = result.ret_1x_quality;
    result.source_peak_v = 0.141421f;
    result.return_peak_v = vx;
    result.denominator_peak_v = 0.141421f * dsp.zref_ohms.re / denominator;
    result.short_like = vx < 0.00005f;
    if (vx < 0.00005f)
    {
        result.dsp_status = MEASUREMENT_STATUS_CHANNEL_UNUSABLE;
        result.derived.valid = false;
        result.ret_1x_quality.usable = false;
        result.ret_hg_quality.usable = false;
        result.short_like = false;
    }
    return result;
}

static int run(float r, float c, float l, measurement_interpretation_t expected, bool low_primary)
{
    int failures = 0;
    measurement_auto_session_t session;
    const measurement_auto_start_t start = {.frequency_survey = true,
        .qualification = MEASUREMENT_QUALIFICATION_UNQUALIFIED};
    failures += check(measurement_auto_start_session(&session, &start), "survey starts");
    uint8_t mask = 0u;
    uint8_t stages[3] = {0};
    for (uint8_t i = 0u; i < MEASUREMENT_AUTO_HISTORY_CAPACITY && session.has_next_attempt; i++)
    {
        measurement_attempt_config_t config;
        failures += check(measurement_auto_next_attempt(&session, &config), "next bounded survey condition");
        mask |= (uint8_t)(1u << (unsigned)config.frequency);
        stages[(unsigned)config.frequency]++;
        failures += check(stages[(unsigned)config.frequency] <= 4u, "four conditions per frequency maximum");
        for (uint8_t j = 0u; j < session.attempt_count; j++)
            failures += check(session.history[j].config.range_id != config.range_id ||
                              session.history[j].config.frequency != config.frequency ||
                              session.history[j].config.amplitude != config.amplitude, "no repeated condition");
        const measurement_attempt_result_t result = point(config, r, c, l);
        (void)measurement_auto_submit_result(&session, &result);
        if (mask != 7u) failures += check(!session.has_final, "no terminal classification before all frequencies");
    }
    const measurement_session_result_t *final = measurement_auto_last_result(&session);
    failures += check(session.has_final && mask == 7u && final->frequency_attempted_mask == 7u,
                      "three frequencies included before final");
    failures += check(final->status == MEASUREMENT_AUTO_STATUS_FINAL_OK &&
                      final->classification.interpretation == expected, "expected physical model from combined evidence");
    if (low_primary)
        failures += check(final->primary_attempt.config.frequency == HW_EXCITATION_FREQ_10HZ,
                          "large capacitor selects a usable 10-Hz primary");
    if (c > 0.0f)
        failures += check(final->primary_attempt.derived.capacitance_valid &&
            fabsf(final->primary_attempt.derived.capacitance_f / c - 1.0f) < 0.00001f, "synthetic C preserved");
    if (l > 0.0f && expected == MEASUREMENT_INTERPRET_INDUCTIVE)
        failures += check(final->primary_attempt.derived.inductance_valid &&
            fabsf(final->primary_attempt.derived.inductance_h / l - 1.0f) < 0.00001f, "synthetic L preserved");
    return failures;
}

static int test_faults_and_contrary_evidence(void)
{
    int failures = 0;
    measurement_attempt_result_t data[3];
    const hw_excitation_freq_t frequencies[] = {HW_EXCITATION_FREQ_1KHZ, HW_EXCITATION_FREQ_10KHZ, HW_EXCITATION_FREQ_10HZ};
    for (uint8_t i = 0u; i < 3u; i++)
        data[i] = point((measurement_attempt_config_t){.range_id = HW_RANGE_ID_10R,
            .frequency = frequencies[i]}, 7.5f, 0.0f, 0.000470f);
    failures += check(data[2].derived.interpretation == MEASUREMENT_INTERPRET_RESISTIVE &&
        measurement_auto_classify_survey(data, 3u).interpretation == MEASUREMENT_INTERPRET_INDUCTIVE,
        "low-frequency resistance is compatible with an inductive trend");
    data[2] = point(data[2].config, 1.0f, 0.001f, 0.0f);
    failures += check(measurement_auto_classify_survey(data, 3u).interpretation == MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN,
                      "opposing reactive signs remain unknown");
    data[0] = point(data[0].config, 10.0f, 0.0f, 0.0f);
    data[1] = point(data[1].config, 10.0f, 0.0f, 0.0f);
    data[2] = point(data[2].config, 10.0f, 0.0f, 0.0f);
    data[2].ret_1x_quality.signal_peak_v = 0.001f;
    failures += check(measurement_auto_classify_survey(data, 3u).interpretation == MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN,
                      "weak missing evidence cannot confirm three-frequency resistor");
    for (uint8_t cancel = 0u; cancel < 2u; cancel++)
    {
        measurement_auto_session_t session;
        const measurement_auto_start_t start = {.frequency_survey = true};
        (void)measurement_auto_start_session(&session, &start);
        measurement_attempt_result_t failed = point(session.next_attempt, 10.0f, 0.0f, 0.0f);
        failed.canceled = cancel != 0u;
        failed.safety_abort = cancel == 0u;
        (void)measurement_auto_submit_result(&session, &failed);
        failures += check(session.has_final && session.attempt_count == 1u && !session.has_next_attempt &&
            session.status == (cancel != 0u ? MEASUREMENT_AUTO_STATUS_CANCELED : MEASUREMENT_AUTO_STATUS_SAFETY_ABORT),
            "safety and cancellation stop before later frequencies");
    }
    return failures;
}

static int test_endpoint_and_invalid_surveys(void)
{
    int failures = 0;
    for (uint8_t variant = 0u; variant < 3u; variant++)
    {
        measurement_auto_session_t session;
        const measurement_auto_start_t start = {.frequency_survey = true};
        (void)measurement_auto_start_session(&session, &start);
        while (session.has_next_attempt)
        {
            measurement_attempt_result_t result = point(session.next_attempt, 0.001f, 0.0f, 0.0f);
            if (variant == 1u)
            {
                result.short_like = false;
                result.open_like = true;
                result.dsp_status = MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL;
                result.derived.valid = false;
            }
            if (variant == 0u)
            {
                result.short_like = true;
                result.dsp_status = MEASUREMENT_STATUS_OK;
            }
            if (variant == 2u)
            {
                result.short_like = false;
                result.clipped = true;
                result.ret_1x_quality.clipped = true;
            }
            (void)measurement_auto_submit_result(&session, &result);
        }
        failures += check(session.attempt_count <= MEASUREMENT_AUTO_HISTORY_CAPACITY &&
                          session.last_result.frequency_attempted_mask == 7u,
                          "OPEN SHORT and unusable results still explore all frequencies within bound");
        const measurement_auto_status_t expected = variant == 0u ? MEASUREMENT_AUTO_STATUS_SHORT_LIKE :
            variant == 1u ? MEASUREMENT_AUTO_STATUS_OPEN_LIKE : MEASUREMENT_AUTO_STATUS_NO_VALID_CONDITION;
        failures += check(session.status == expected, "endpoint and clipped surveys have truthful terminal status");
    }
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += run(10.0f, 0.0f, 0.0f, MEASUREMENT_INTERPRET_RESISTIVE, false);
    failures += run(1000.0f, 0.0f, 0.0f, MEASUREMENT_INTERPRET_RESISTIVE, false);
    failures += run(10000000.0f, 0.0f, 0.0f, MEASUREMENT_INTERPRET_RESISTIVE, false);
    failures += run(0.07f, 0.0047f, 0.0f, MEASUREMENT_INTERPRET_CAPACITIVE, true);
    failures += run(0.07f, 0.0100f, 0.0f, MEASUREMENT_INTERPRET_CAPACITIVE, true);
    failures += run(2.0f, 0.000000010f, 0.0f, MEASUREMENT_INTERPRET_CAPACITIVE, false);
    failures += run(7.5f, 0.0f, 0.000470f, MEASUREMENT_INTERPRET_INDUCTIVE, false);
    failures += run(10.0f, 0.0f, 10.0f, MEASUREMENT_INTERPRET_INDUCTIVE, false);
    failures += run(0.5f, 0.0f, 0.000000470f, MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN, false);
    failures += test_faults_and_contrary_evidence();
    failures += test_endpoint_and_invalid_surveys();
    return failures == 0 ? 0 : 1;
}
