#include "hardware/hw_charger.h"
#include "hardware/hw_power.h"
#include "hardware/hw_residual.h"
#include "hardware/hw_safety.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static hw_safety_input_t valid_input(void)
{
    return (hw_safety_input_t){
        .charger = HW_CHARGER_ABSENT,
        .residual = HW_RESIDUAL_SAFE,
        .battery = HW_BATTERY_OK,
        .range = HW_RANGE_READY,
        .application_fault = false,
    };
}

static int expect_primary(hw_safety_input_t input, hw_safety_primary_blocker_t primary, const char *message)
{
    const hw_safety_result_t result = hw_safety_evaluate(&input);
    return expect_true(result.primary_blocker == primary, message);
}

static int test_safety_policy_matrix(void)
{
    int failures = 0;
    hw_safety_input_t input = valid_input();
    hw_safety_result_t result = hw_safety_evaluate(&input);
    failures += expect_true(result.measure_allowed, "valid synthetic state allows measure");
    failures += expect_true(result.blocker_flags == HW_SAFETY_BLOCK_NONE, "valid state has no blockers");

    input = valid_input();
    input.charger = HW_CHARGER_PRESENT;
    failures += expect_primary(input, HW_SAFETY_BLOCKED_CHARGER, "charger present blocks");

    input = valid_input();
    input.charger = HW_CHARGER_UNKNOWN;
    failures += expect_primary(input, HW_SAFETY_BLOCKED_SENSOR_INVALID, "charger unknown fails closed");

    input = valid_input();
    input.residual = HW_RESIDUAL_UNSAFE;
    failures += expect_primary(input, HW_SAFETY_BLOCKED_RESIDUAL, "residual unsafe blocks");

    input = valid_input();
    input.residual = HW_RESIDUAL_UNKNOWN;
    failures += expect_primary(input, HW_SAFETY_BLOCKED_SENSOR_INVALID, "residual unknown fails closed");

    input = valid_input();
    input.residual = HW_RESIDUAL_SATURATED;
    failures += expect_primary(input, HW_SAFETY_BLOCKED_SENSOR_INVALID, "residual saturated fails closed");

    input = valid_input();
    input.battery = HW_BATTERY_CRITICAL;
    failures += expect_primary(input, HW_SAFETY_BLOCKED_SUPPLY, "battery critical blocks supply");

    input = valid_input();
    input.battery = HW_BATTERY_UNKNOWN;
    failures += expect_primary(input, HW_SAFETY_BLOCKED_SENSOR_INVALID, "battery unknown is sensor invalid first");

    input = valid_input();
    input.battery = HW_BATTERY_LOW;
    result = hw_safety_evaluate(&input);
    failures += expect_true(result.measure_allowed, "battery low remains allowed for now");
    failures += expect_true(result.battery_low, "battery low status is exposed");

    input = valid_input();
    input.range = HW_RANGE_DISABLED;
    failures += expect_primary(input, HW_SAFETY_BLOCKED_RANGE, "range disabled blocks");

    input = valid_input();
    input.range = HW_RANGE_TRANSITIONING;
    failures += expect_primary(input, HW_SAFETY_BLOCKED_RANGE, "range transitioning blocks");

    input = valid_input();
    input.range = HW_RANGE_INVALID;
    failures += expect_primary(input, HW_SAFETY_BLOCKED_RANGE, "range invalid blocks");

    input = valid_input();
    input.application_fault = true;
    failures += expect_primary(input, HW_SAFETY_BLOCKED_FAULT, "application fault has highest precedence");

    input = valid_input();
    input.application_fault = true;
    input.charger = HW_CHARGER_PRESENT;
    input.residual = HW_RESIDUAL_UNSAFE;
    result = hw_safety_evaluate(&input);
    failures += expect_true(result.primary_blocker == HW_SAFETY_BLOCKED_FAULT, "multiple blockers preserve precedence");
    failures += expect_true((result.blocker_flags & HW_SAFETY_BLOCK_CHARGER) != 0u, "charger flag preserved");
    failures += expect_true((result.blocker_flags & HW_SAFETY_BLOCK_RESIDUAL) != 0u, "residual flag preserved");

    input = valid_input();
    result = hw_safety_evaluate(&input);
    failures += expect_true(result.measure_allowed, "recovery succeeds after all blockers clear");

    return failures;
}

static int test_residual_policy(void)
{
    int failures = 0;
    hw_residual_policy_t policy;
    const hw_residual_policy_input_t safe = {
        .valid = true,
        .saturated = false,
        .safe_hi_v = 0.10f,
        .safe_lo_v = -0.10f,
        .residual_diff_v = 0.20f,
    };

    hw_residual_policy_init(&policy);
    for (uint8_t i = 0u; i < 7u; i++)
    {
        failures += expect_true(hw_residual_policy_evaluate(&policy, &safe) == HW_RESIDUAL_UNKNOWN,
                                "seven safe evaluations do not grant SAFE");
    }
    failures += expect_true(hw_residual_policy_evaluate(&policy, &safe) == HW_RESIDUAL_SAFE,
                            "eighth safe evaluation grants SAFE");

    const hw_residual_policy_input_t hysteresis_080 = {
        .valid = true,
        .saturated = false,
        .safe_hi_v = 0.80f,
        .safe_lo_v = 0.0f,
        .residual_diff_v = 0.80f,
    };
    failures += expect_true(hw_residual_policy_evaluate(&policy, &hysteresis_080) == HW_RESIDUAL_SAFE,
                            "SAFE remains SAFE at 0.80 V hysteresis");
    failures += expect_true(hw_residual_policy_evaluate(&policy, &safe) == HW_RESIDUAL_SAFE,
                            "SAFE returning to release region does not requalify");

    const hw_residual_policy_input_t hysteresis_099 = {
        .valid = true,
        .saturated = false,
        .safe_hi_v = 0.99f,
        .safe_lo_v = 0.0f,
        .residual_diff_v = 0.99f,
    };
    failures += expect_true(hw_residual_policy_evaluate(&policy, &hysteresis_099) == HW_RESIDUAL_SAFE,
                            "SAFE remains SAFE below block threshold");

    const hw_residual_policy_input_t unsafe = {
        .valid = true,
        .saturated = false,
        .safe_hi_v = 1.00f,
        .safe_lo_v = 0.0f,
        .residual_diff_v = 1.00f,
    };
    failures += expect_true(hw_residual_policy_evaluate(&policy, &unsafe) == HW_RESIDUAL_UNSAFE,
                            "block threshold is immediate UNSAFE");

    const hw_residual_policy_input_t hysteresis = {
        .valid = true,
        .saturated = false,
        .safe_hi_v = 0.90f,
        .safe_lo_v = 0.0f,
        .residual_diff_v = 0.90f,
    };
    failures += expect_true(hw_residual_policy_evaluate(&policy, &hysteresis) == HW_RESIDUAL_UNKNOWN,
                            "hysteresis band does not newly grant SAFE");
    failures += expect_true(hw_residual_policy_evaluate(&policy, &safe) == HW_RESIDUAL_UNKNOWN,
                            "one safe sample after UNKNOWN is not enough");

    const hw_residual_policy_input_t invalid = {
        .valid = false,
        .saturated = false,
        .safe_hi_v = 0.0f,
        .safe_lo_v = 0.0f,
        .residual_diff_v = 0.0f,
    };
    failures += expect_true(hw_residual_policy_evaluate(&policy, &invalid) == HW_RESIDUAL_UNKNOWN,
                            "invalid sample fails closed");

    const hw_residual_policy_input_t saturated = {
        .valid = true,
        .saturated = true,
        .safe_hi_v = 0.0f,
        .safe_lo_v = 0.0f,
        .residual_diff_v = 0.0f,
    };
    failures += expect_true(hw_residual_policy_evaluate(&policy, &saturated) == HW_RESIDUAL_SATURATED,
                            "saturated sample fails closed");

    failures += expect_true(hw_residual_raw_is_saturated(16u), "raw low guard saturates");
    failures += expect_true(hw_residual_raw_is_saturated(4079u), "raw high guard saturates");
    failures += expect_true(!hw_residual_raw_is_saturated(2048u), "middle raw sample is plausible");

    const float terminal = hw_residual_terminal_voltage(1.65f, 1.65f + 0.015817f);
    failures += expect_true((terminal > 2.64f) && (terminal < 2.66f), "residual transfer K is about 63.222");
    failures += expect_true(hw_residual_differential_voltage(0.40f, -0.20f) > 0.59f,
                            "differential residual subtracts LO from HI");

    return failures;
}

static int test_charger_and_power_helpers(void)
{
    int failures = 0;
    bool ntc_valid = false;
    float temperature_c = 0.0f;
    failures += expect_true(hw_charger_state_from_gpio(BSP_STATUS_OK, false) == HW_CHARGER_ABSENT,
                            "LOW charger GPIO is absent");
    failures += expect_true(hw_charger_state_from_gpio(BSP_STATUS_OK, true) == HW_CHARGER_PRESENT,
                            "HIGH charger GPIO is present");
    failures += expect_true(hw_charger_state_from_gpio(BSP_STATUS_ERROR, false) == HW_CHARGER_UNKNOWN,
                            "charger read failure is unknown");
    failures += expect_true(hw_battery_vbat_from_adc_voltage(1.80f) > 3.59f, "battery divider is 2x");
    failures += expect_true(hw_battery_state_from_voltage(3.60f, true) == HW_BATTERY_OK, "battery OK threshold");
    failures += expect_true(hw_battery_state_from_voltage(3.40f, true) == HW_BATTERY_LOW, "battery low threshold");
    failures += expect_true(hw_battery_state_from_voltage(3.20f, true) == HW_BATTERY_CRITICAL,
                            "battery critical threshold");
    failures += expect_true(hw_battery_state_from_voltage(4.36f, true) == HW_BATTERY_UNKNOWN,
                            "battery above plausible ceiling is unknown");
    failures += expect_true(hw_battery_state_from_voltage(4.00f, false) == HW_BATTERY_UNKNOWN,
                            "invalid battery sample is unknown");
    const float ntc_ohm = hw_ntc_resistance_from_voltage(1.65f, 3.30f, &ntc_valid);
    failures += expect_true(ntc_valid && (ntc_ohm > 99900.0f) && (ntc_ohm < 100100.0f),
                            "NTC bottom-leg divider returns 100k at half-scale");
    (void)hw_ntc_resistance_from_voltage(3.30f, 3.30f, &ntc_valid);
    failures += expect_true(!ntc_valid, "NTC rail sample is invalid");
    failures += expect_true(hw_ntc_temperature_from_resistance(336206.0f, &temperature_c) &&
                                (temperature_c > -0.1f) && (temperature_c < 0.1f),
                            "NTC LUT has 0 C point");
    failures += expect_true(hw_ntc_temperature_from_resistance(100000.0f, &temperature_c) &&
                                (temperature_c > 24.9f) && (temperature_c < 25.1f),
                            "NTC LUT has 25 C point");
    failures += expect_true(hw_ntc_temperature_from_resistance(35882.0f, &temperature_c) &&
                                (temperature_c > 49.9f) && (temperature_c < 50.1f),
                            "NTC LUT has 50 C point");
    failures += expect_true(hw_ntc_temperature_from_resistance(90185.5f, &temperature_c) &&
                                (temperature_c > 27.4f) && (temperature_c < 27.6f),
                            "NTC LUT interpolates around 27.5 C");
    failures += expect_true(!hw_ntc_temperature_from_resistance(1200000.0f, &temperature_c),
                            "NTC LUT refuses cold out-of-range resistance");
    failures += expect_true(!hw_ntc_temperature_from_resistance(10000.0f, &temperature_c),
                            "NTC LUT refuses hot out-of-range resistance");

    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_safety_policy_matrix();
    failures += test_residual_policy();
    failures += test_charger_and_power_helpers();
    return (failures == 0) ? 0 : 1;
}
