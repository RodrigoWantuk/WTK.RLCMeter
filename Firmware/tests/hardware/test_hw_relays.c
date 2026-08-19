#include "hardware/hw_k1.h"
#include "hardware/hw_k2.h"
#include "hardware/hw_safety.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct
{
    bool level;
    unsigned int writes;
} fake_output_t;

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static bsp_status_t fake_write(bool high, void *user_data)
{
    fake_output_t *output = (fake_output_t *)user_data;
    output->level = high;
    output->writes++;
    return BSP_STATUS_OK;
}

static hw_safety_result_t allowed_permission(void)
{
    return (hw_safety_result_t){
        .measure_allowed = true,
        .battery_low = false,
        .blocker_flags = HW_SAFETY_BLOCK_NONE,
        .primary_blocker = HW_SAFETY_MEASURE_ALLOWED,
    };
}

static hw_safety_result_t denied_permission(void)
{
    return (hw_safety_result_t){
        .measure_allowed = false,
        .battery_low = false,
        .blocker_flags = HW_SAFETY_BLOCK_RESIDUAL,
        .primary_blocker = HW_SAFETY_BLOCKED_RESIDUAL,
    };
}

static int test_k1_service(void)
{
    int failures = 0;
    fake_output_t output = {
        .level = true,
        .writes = 0u,
    };
    hw_k1_t k1;
    const hw_k1_io_t io = {
        .write_cmd = fake_write,
        .user_data = &output,
    };
    hw_safety_result_t permission = allowed_permission();

    failures += expect_true(hw_k1_init(&k1, &io) == BSP_STATUS_OK, "K1 init succeeds");
    failures += expect_true(!output.level, "K1 init commands LOW/SAFE");
    failures += expect_true(hw_k1_commanded_state(&k1) == HW_K1_STATE_SAFE, "K1 state starts SAFE");
    failures += expect_true(hw_k1_request_measure(&k1, &permission) == BSP_STATUS_OK,
                            "valid permission commands MEASURE");
    failures += expect_true(output.level, "K1 MEASURE command is HIGH");
    failures += expect_true(hw_k1_commanded_state(&k1) == HW_K1_STATE_MEASURE, "K1 state records MEASURE");
    failures += expect_true(hw_k1_force_safe(&k1) == BSP_STATUS_OK, "K1 force-safe succeeds");
    failures += expect_true(!output.level, "K1 force-safe commands LOW");
    permission = denied_permission();
    failures += expect_true(hw_k1_request_measure(&k1, &permission) == BSP_STATUS_ERROR,
                            "denied permission cannot energize K1");
    failures += expect_true(!output.level, "denied permission leaves K1 LOW");
    failures += expect_true(hw_k1_request_measure(&k1, NULL) == BSP_STATUS_INVALID_ARG,
                            "NULL permission is rejected");
    failures += expect_true(!output.level, "NULL permission leaves K1 LOW");

    return failures;
}

static int test_k2_service(void)
{
    int failures = 0;
    fake_output_t output = {
        .level = true,
        .writes = 0u,
    };
    hw_k2_t k2;
    const hw_k2_io_t io = {
        .write_cmd = fake_write,
        .user_data = &output,
    };

    failures += expect_true(hw_k2_init(&k2, &io) == BSP_STATUS_OK, "K2 init succeeds");
    failures += expect_true(!output.level, "K2 init commands LOW");
    const hw_k2_topology_t topology = hw_k2_topology(&k2);
    failures += expect_true(!topology.populated, "Rev.1 K2 is not populated");
    failures += expect_true(topology.lowz_bank_mode == HW_LOWZ_BANK_MODE_FIXED_0R_LINK,
                            "Rev.1 low-Z bank is fixed 0R link");
    failures += expect_true(hw_k2_request_physical_switch(&k2) == BSP_STATUS_NOT_SUPPORTED,
                            "physical K2 request is not supported");
    failures += expect_true(!output.level, "unsupported K2 request keeps PA11 LOW");

    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_k1_service();
    failures += test_k2_service();
    return (failures == 0) ? 0 : 1;
}
