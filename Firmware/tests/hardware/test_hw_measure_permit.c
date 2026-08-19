#include "hardware/hw_measure_permit.h"

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

static hw_measure_permit_issue_input_t valid_issue_input(void)
{
    return (hw_measure_permit_issue_input_t){
        .charger = HW_CHARGER_ABSENT,
        .residual = HW_RESIDUAL_SAFE,
        .residual_age_ms = 0u,
        .battery = HW_BATTERY_OK,
        .battery_age_ms = 0u,
        .range = HW_RANGE_READY,
        .range_id = HW_RANGE_ID_1K,
        .k1_state = HW_K1_STATE_SAFE,
        .safety_fault_mask = 0u,
    };
}

static hw_measure_permit_validate_input_t valid_validate_input(void)
{
    return (hw_measure_permit_validate_input_t){
        .charger = HW_CHARGER_ABSENT,
        .range = HW_RANGE_READY,
        .range_id = HW_RANGE_ID_1K,
        .k1_state = HW_K1_STATE_SAFE,
        .safety_fault_mask = 0u,
    };
}

static int expect_issue_denied(hw_measure_permit_issue_input_t input,
                               hw_measure_permit_rejection_t reason,
                               const char *message)
{
    const hw_measure_permit_issue_result_t result = hw_measure_permit_check_issue(&input);
    return expect_true(!result.issued && (result.reason == reason), message);
}

static int test_issue_matrix(void)
{
    int failures = 0;
    hw_measure_permit_issue_input_t input = valid_issue_input();
    hw_measure_permit_issue_result_t result = hw_measure_permit_check_issue(&input);
    failures += expect_true(result.issued && (result.reason == HW_MEASURE_PERMIT_OK),
                            "valid prerequisites issue permit eligibility");

    input = valid_issue_input();
    input.charger = HW_CHARGER_PRESENT;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_CHARGER, "charger present denies issue");
    input.charger = HW_CHARGER_UNKNOWN;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_CHARGER, "charger unknown denies issue");

    input = valid_issue_input();
    input.residual = HW_RESIDUAL_UNKNOWN;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_RESIDUAL, "residual unknown denies issue");
    input.residual = HW_RESIDUAL_UNSAFE;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_RESIDUAL, "residual unsafe denies issue");
    input.residual = HW_RESIDUAL_SATURATED;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_RESIDUAL, "residual saturated denies issue");
    input = valid_issue_input();
    input.residual_age_ms = HW_MEASURE_PERMIT_RESIDUAL_MAX_AGE_MS + 1u;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_RESIDUAL_STALE,
                                    "residual age 21 ms denies issue");

    input = valid_issue_input();
    input.battery = HW_BATTERY_LOW;
    result = hw_measure_permit_check_issue(&input);
    failures += expect_true(result.issued, "battery low remains eligible");
    input.battery = HW_BATTERY_CRITICAL;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_BATTERY, "battery critical denies issue");
    input.battery = HW_BATTERY_UNKNOWN;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_BATTERY, "battery unknown denies issue");
    input = valid_issue_input();
    input.battery_age_ms = HW_MEASURE_PERMIT_BATTERY_MAX_AGE_MS + 1u;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_BATTERY_STALE,
                                    "battery age 1001 ms denies issue");

    input = valid_issue_input();
    input.range = HW_RANGE_DISABLED;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_RANGE, "range disabled denies issue");
    input.range = HW_RANGE_TRANSITIONING;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_RANGE, "range transitioning denies issue");
    input = valid_issue_input();
    input.range_id = HW_RANGE_ID_INVALID;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_RANGE_ID, "invalid range ID denies issue");

    input = valid_issue_input();
    input.k1_state = HW_K1_STATE_UNKNOWN;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_K1, "K1 unknown denies issue");
    input.k1_state = HW_K1_STATE_MEASURE;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_K1, "K1 measure denies issue");

    input = valid_issue_input();
    input.safety_fault_mask = 1u;
    failures += expect_issue_denied(input, HW_MEASURE_PERMIT_REJECT_FAULT, "application fault denies issue");

    return failures;
}

static int test_validation_matrix(void)
{
    int failures = 0;
    hw_measure_permit_t permit;
    hw_measure_permit_issue_input_t issue = valid_issue_input();
    hw_measure_permit_validate_input_t validate = valid_validate_input();
    hw_measure_permit_issue_result_t issued;
    hw_measure_permit_validate_result_t validated;

    hw_measure_permit_init(&permit);
    issued = hw_measure_permit_issue(&permit, &issue, 100u);
    failures += expect_true(issued.issued && permit.valid && !permit.consumed, "permit is issued and unconsumed");
    validated = hw_measure_permit_validate(&permit, &validate, 105u);
    failures += expect_true(validated.allowed && (validated.reason == HW_MEASURE_PERMIT_OK),
                            "age 5 ms permit validates");
    validated = hw_measure_permit_validate(&permit, &validate, 105u);
    failures += expect_true(!validated.allowed && (validated.reason == HW_MEASURE_PERMIT_REJECT_CONSUMED),
                            "second validation is denied");

    issue = valid_issue_input();
    issued = hw_measure_permit_issue(&permit, &issue, 200u);
    failures += expect_true(issued.issued, "permit reissued for expiry test");
    validated = hw_measure_permit_validate(&permit, &validate, 206u);
    failures += expect_true(!validated.allowed && (validated.reason == HW_MEASURE_PERMIT_REJECT_EXPIRED),
                            "age greater than 5 ms expires permit");

    issue = valid_issue_input();
    issued = hw_measure_permit_issue(&permit, &issue, 300u);
    failures += expect_true(issued.issued, "permit reissued for changed range");
    validate.range_id = HW_RANGE_ID_10K;
    validated = hw_measure_permit_validate(&permit, &validate, 300u);
    failures += expect_true(!validated.allowed && (validated.reason == HW_MEASURE_PERMIT_REJECT_RANGE),
                            "changed range denies and consumes permit");
    validate = valid_validate_input();
    validated = hw_measure_permit_validate(&permit, &validate, 300u);
    failures += expect_true(!validated.allowed && (validated.reason == HW_MEASURE_PERMIT_REJECT_CONSUMED),
                            "failed validation also consumes");

    issue = valid_issue_input();
    issued = hw_measure_permit_issue(&permit, &issue, 400u);
    failures += expect_true(issued.issued, "permit reissued for dynamic blockers");
    validate.charger = HW_CHARGER_PRESENT;
    validated = hw_measure_permit_validate(&permit, &validate, 400u);
    failures += expect_true(!validated.allowed && (validated.reason == HW_MEASURE_PERMIT_REJECT_CHARGER),
                            "charger present denies validation");

    issue = valid_issue_input();
    issued = hw_measure_permit_issue(&permit, &issue, 500u);
    failures += expect_true(issued.issued, "permit reissued for fault blocker");
    validate = valid_validate_input();
    validate.safety_fault_mask = 1u;
    validated = hw_measure_permit_validate(&permit, &validate, 500u);
    failures += expect_true(!validated.allowed && (validated.reason == HW_MEASURE_PERMIT_REJECT_FAULT),
                            "latched fault denies validation");

    issue = valid_issue_input();
    issued = hw_measure_permit_issue(&permit, &issue, 600u);
    failures += expect_true(issued.issued, "permit reissued for K1 blocker");
    validate = valid_validate_input();
    validate.k1_state = HW_K1_STATE_MEASURE;
    validated = hw_measure_permit_validate(&permit, &validate, 600u);
    failures += expect_true(!validated.allowed && (validated.reason == HW_MEASURE_PERMIT_REJECT_K1),
                            "K1 no longer SAFE denies validation");

    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_issue_matrix();
    failures += test_validation_matrix();
    return (failures == 0) ? 0 : 1;
}
