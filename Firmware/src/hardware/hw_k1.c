#include "hardware/hw_k1.h"

#include <stddef.h>

#include "wtk_build_config.h"
#if WTK_ENABLE_K1_BENCH_TEST
#include "hardware/hw_k1_bench.h"
#endif

static bsp_status_t write_safe(hw_k1_t *k1)
{
    if ((k1 == NULL) || (k1->io.write_cmd == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    const bsp_status_t status = k1->io.write_cmd(false, k1->io.user_data);
    k1->last_status = status;
    if (status == BSP_STATUS_OK)
    {
        k1->commanded_state = HW_K1_STATE_SAFE;
    }
    return status;
}

bsp_status_t hw_k1_init(hw_k1_t *k1, const hw_k1_io_t *io)
{
    if ((k1 == NULL) || (io == NULL) || (io->write_cmd == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    k1->io = *io;
    k1->commanded_state = HW_K1_STATE_UNKNOWN;
    k1->last_status = BSP_STATUS_OK;
    return write_safe(k1);
}

bsp_status_t hw_k1_force_safe(hw_k1_t *k1)
{
    return write_safe(k1);
}

bsp_status_t hw_k1_request_measure(hw_k1_t *k1, const hw_safety_result_t *permission)
{
    if ((k1 == NULL) || (k1->io.write_cmd == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    if (permission == NULL)
    {
        const bsp_status_t safe_status = write_safe(k1);
        if (safe_status != BSP_STATUS_OK)
        {
            return safe_status;
        }
        return BSP_STATUS_INVALID_ARG;
    }

    if (!permission->measure_allowed || (permission->blocker_flags != HW_SAFETY_BLOCK_NONE))
    {
        const bsp_status_t safe_status = write_safe(k1);
        if (safe_status != BSP_STATUS_OK)
        {
            return safe_status;
        }
        return BSP_STATUS_ERROR;
    }

    const bsp_status_t status = k1->io.write_cmd(true, k1->io.user_data);
    k1->last_status = status;
    if (status == BSP_STATUS_OK)
    {
        k1->commanded_state = HW_K1_STATE_MEASURE;
    }
    return status;
}

hw_k1_state_t hw_k1_commanded_state(const hw_k1_t *k1)
{
    if (k1 == NULL)
    {
        return HW_K1_STATE_UNKNOWN;
    }
    return k1->commanded_state;
}

bsp_status_t hw_k1_last_status(const hw_k1_t *k1)
{
    return (k1 == NULL) ? BSP_STATUS_INVALID_ARG : k1->last_status;
}

const char *hw_k1_state_string(hw_k1_state_t state)
{
    switch (state)
    {
    case HW_K1_STATE_MEASURE:
        return "MEASURE";
    case HW_K1_STATE_SAFE:
        return "SAFE";
    case HW_K1_STATE_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

#if WTK_ENABLE_K1_BENCH_TEST
static bool bench_input_safe(const hw_k1_bench_input_t *input)
{
    return (input != NULL) && !input->faulted && input->clock_ready && input->idle &&
           input->fixture_empty_confirmed && input->vmid_valid &&
           (input->charger == HW_CHARGER_ABSENT) && (input->residual == HW_RESIDUAL_SAFE) &&
           (input->residual_age_ms <= HW_K1_BENCH_RESIDUAL_MAX_AGE_MS) &&
           input->range_disabled && input->excitation_off;
}

static bool bench_deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (now_ms - deadline_ms) < 0x80000000u;
}

void hw_k1_bench_init(hw_k1_bench_t *test)
{
    if (test != NULL)
    {
        *test = (hw_k1_bench_t){.state = HW_K1_BENCH_IDLE};
    }
}

bool hw_k1_bench_owns_k1(const hw_k1_bench_t *test)
{
    return (test != NULL) &&
           ((test->state == HW_K1_BENCH_PULSE) || (test->state == HW_K1_BENCH_RELEASE));
}

static bsp_status_t bench_return_safe(hw_k1_bench_t *test, hw_k1_t *k1, uint32_t now_ms)
{
    if (!test->safe_written)
    {
        const bsp_status_t status = hw_k1_force_safe(k1);
        if (status != BSP_STATUS_OK)
        {
            test->completion = HW_K1_BENCH_IO_ERROR;
            return status;
        }
        test->safe_written = true;
        test->deadline_ms = now_ms + HW_K1_BENCH_RELEASE_MS;
    }
    if (bench_deadline_reached(now_ms, test->deadline_ms))
    {
        test->state = test->completion;
    }
    return BSP_STATUS_OK;
}

bsp_status_t hw_k1_bench_start(hw_k1_bench_t *test, hw_k1_t *k1,
                              const hw_k1_bench_input_t *input, uint32_t now_ms)
{
    if ((test == NULL) || (k1 == NULL) || (k1->io.write_cmd == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (hw_k1_bench_owns_k1(test))
    {
        return BSP_STATUS_BUSY;
    }
    if (test->used || !bench_input_safe(input) || (k1->commanded_state != HW_K1_STATE_SAFE))
    {
        return BSP_STATUS_ERROR;
    }
    test->used = true;
    test->safe_written = false;
    test->completion = HW_K1_BENCH_DONE;
    test->deadline_ms = now_ms + HW_K1_BENCH_PULSE_MS;
    const bsp_status_t status = k1->io.write_cmd(true, k1->io.user_data);
    k1->last_status = status;
    if (status != BSP_STATUS_OK)
    {
        test->state = HW_K1_BENCH_RELEASE;
        test->completion = HW_K1_BENCH_IO_ERROR;
        (void)bench_return_safe(test, k1, now_ms);
        return status;
    }
    k1->commanded_state = HW_K1_STATE_MEASURE;
    test->state = HW_K1_BENCH_PULSE;
    return BSP_STATUS_OK;
}

bsp_status_t hw_k1_bench_step(hw_k1_bench_t *test, hw_k1_t *k1,
                             const hw_k1_bench_input_t *input, uint32_t now_ms)
{
    if ((test == NULL) || (k1 == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (test->state == HW_K1_BENCH_PULSE)
    {
        if (!bench_input_safe(input) || (k1->commanded_state != HW_K1_STATE_MEASURE))
        {
            test->completion = HW_K1_BENCH_ABORTED;
            test->state = HW_K1_BENCH_RELEASE;
        }
        else if (bench_deadline_reached(now_ms, test->deadline_ms))
        {
            test->state = HW_K1_BENCH_RELEASE;
        }
    }
    if (test->state == HW_K1_BENCH_RELEASE)
    {
        if (k1->commanded_state != HW_K1_STATE_SAFE)
        {
            test->safe_written = false;
        }
        return bench_return_safe(test, k1, now_ms);
    }
    return BSP_STATUS_OK;
}

bsp_status_t hw_k1_bench_cancel(hw_k1_bench_t *test, hw_k1_t *k1, uint32_t now_ms)
{
    if ((test == NULL) || (k1 == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (test->state == HW_K1_BENCH_PULSE)
    {
        test->completion = HW_K1_BENCH_ABORTED;
        test->state = HW_K1_BENCH_RELEASE;
    }
    if ((test->state == HW_K1_BENCH_RELEASE) && (k1->commanded_state != HW_K1_STATE_SAFE))
    {
        test->safe_written = false;
    }
    return (test->state == HW_K1_BENCH_RELEASE) ? bench_return_safe(test, k1, now_ms) : BSP_STATUS_OK;
}

const char *hw_k1_bench_state_string(hw_k1_bench_state_t state)
{
    switch (state)
    {
    case HW_K1_BENCH_IDLE: return "IDLE";
    case HW_K1_BENCH_PULSE: return "PULSE";
    case HW_K1_BENCH_RELEASE: return "RELEASE";
    case HW_K1_BENCH_DONE: return "DONE";
    case HW_K1_BENCH_ABORTED: return "ABORTED";
    case HW_K1_BENCH_IO_ERROR: return "IO_ERROR";
    default: return "INVALID";
    }
}
#endif
