#include "app/app_safety_fault.h"
#include "hardware/hw_safety.h"

#include <stdbool.h>
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

int main(void)
{
    int failures = 0;
    app_safety_fault_latch_t faults;
    app_safety_fault_init(&faults);
    failures += expect_true(!app_safety_fault_any(&faults), "no fault initially");
    failures += expect_true(app_safety_fault_mask(&faults) == APP_SAFETY_FAULT_NONE, "initial mask is zero");

    app_safety_fault_latch(&faults, APP_SAFETY_FAULT_K1_IO);
    failures += expect_true(app_safety_fault_any(&faults), "single fault latches");
    failures += expect_true((app_safety_fault_mask(&faults) & APP_SAFETY_FAULT_K1_IO) != 0u, "K1 fault present");

    app_safety_fault_latch(&faults, APP_SAFETY_FAULT_NONE);
    failures += expect_true((app_safety_fault_mask(&faults) & APP_SAFETY_FAULT_K1_IO) != 0u,
                            "successful later operation does not clear fault");

    app_safety_fault_latch(&faults, APP_SAFETY_FAULT_ADC_RUNTIME | APP_SAFETY_FAULT_RANGE_IO);
    failures += expect_true((app_safety_fault_mask(&faults) & APP_SAFETY_FAULT_ADC_RUNTIME) != 0u,
                            "ADC fault accumulates");
    failures += expect_true((app_safety_fault_mask(&faults) & APP_SAFETY_FAULT_RANGE_IO) != 0u,
                            "range fault accumulates");

    const hw_safety_input_t input = {
        .charger = HW_CHARGER_ABSENT,
        .residual = HW_RESIDUAL_SAFE,
        .battery = HW_BATTERY_OK,
        .range = HW_RANGE_READY,
        .application_fault = app_safety_fault_any(&faults),
    };
    const hw_safety_result_t result = hw_safety_evaluate(&input);
    failures += expect_true(!result.measure_allowed, "nonzero fault blocks safety");
    failures += expect_true(result.primary_blocker == HW_SAFETY_BLOCKED_FAULT, "fault has primary precedence");

    app_safety_fault_init(&faults);
    failures += expect_true(!app_safety_fault_any(&faults), "reset/reinit clears latch for Stage 2");
    return (failures == 0) ? 0 : 1;
}
