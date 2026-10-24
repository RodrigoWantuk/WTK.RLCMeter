#include "hardware/hw_k1_bench.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct
{
    hw_k1_t k1;
    hw_k1_bench_t test;
    bool level;
    bool fail_high;
    bool fail_low;
    uint32_t high_writes;
} fixture_t;

static int failures;

static void expect(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

static bsp_status_t write_cmd(bool high, void *user_data)
{
    fixture_t *f = user_data;
    if (high)
    {
        f->high_writes++;
        f->level = true; /* A failed HIGH may still have changed hardware. */
        return f->fail_high ? BSP_STATUS_ERROR : BSP_STATUS_OK;
    }
    if (f->fail_low)
    {
        return BSP_STATUS_ERROR;
    }
    f->level = false;
    return BSP_STATUS_OK;
}

static void setup(fixture_t *f)
{
    *f = (fixture_t){0};
    const hw_k1_io_t io = {.write_cmd = write_cmd, .user_data = f};
    expect(hw_k1_init(&f->k1, &io) == BSP_STATUS_OK, "safe initialization");
    hw_k1_bench_init(&f->test);
}

static hw_k1_bench_input_t ready(void)
{
    return (hw_k1_bench_input_t){
        .charger = HW_CHARGER_ABSENT,
        .residual = HW_RESIDUAL_SAFE,
        .residual_age_ms = HW_K1_BENCH_RESIDUAL_MAX_AGE_MS,
        .vmid_valid = true,
        .range_disabled = true,
        .excitation_off = true,
        .idle = true,
        .clock_ready = true,
        .fixture_empty_confirmed = true,
        .faulted = false,
    };
}

static void make_unsafe(hw_k1_bench_input_t *input, unsigned int condition)
{
    switch (condition)
    {
    case 0u: input->charger = HW_CHARGER_PRESENT; break;
    case 1u: input->charger = HW_CHARGER_UNKNOWN; break;
    case 2u: input->residual = HW_RESIDUAL_UNSAFE; break;
    case 3u: input->residual = HW_RESIDUAL_UNKNOWN; break;
    case 4u: input->residual = HW_RESIDUAL_SATURATED; break;
    case 5u: input->residual_age_ms++; break;
    case 6u: input->vmid_valid = false; break;
    case 7u: input->range_disabled = false; break;
    case 8u: input->excitation_off = false; break;
    case 9u: input->idle = false; break;
    case 10u: input->clock_ready = false; break;
    case 11u: input->fixture_empty_confirmed = false; break;
    case 12u: input->faulted = true; break;
    default: break;
    }
}

static void test_guard_matrix(void)
{
    for (unsigned int condition = 0u; condition < 13u; condition++)
    {
        fixture_t f;
        setup(&f);
        hw_k1_bench_input_t input = ready();
        make_unsafe(&input, condition);
        expect(hw_k1_bench_start(&f.test, &f.k1, &input, 100u) == BSP_STATUS_ERROR,
               "each unsafe input refuses start");
        expect(!f.level && f.high_writes == 0u && !f.test.used, "refused start never energizes or consumes test");
        input = ready();
        expect(hw_k1_bench_start(&f.test, &f.k1, &input, 100u) == BSP_STATUS_OK, "valid start");
        make_unsafe(&input, condition);
        expect(hw_k1_bench_step(&f.test, &f.k1, &input, 101u) == BSP_STATUS_OK, "unsafe transition safely aborts");
        expect(!f.level && f.k1.commanded_state == HW_K1_STATE_SAFE, "abort immediately commands LOW");
        expect(hw_k1_bench_step(&f.test, &f.k1, &input, 109u) == BSP_STATUS_OK, "abort release guard");
        expect(f.test.state == HW_K1_BENCH_ABORTED && !hw_k1_bench_owns_k1(&f.test), "abort terminal ownership");
    }
}

static void test_deadline_and_one_shot(uint32_t start)
{
    fixture_t f;
    setup(&f);
    const hw_k1_bench_input_t input = ready();
    expect(hw_k1_bench_start(&f.test, &f.k1, &input, start) == BSP_STATUS_OK, "pulse starts");
    expect(f.level && f.high_writes == 1u, "exactly one HIGH");
    expect(hw_k1_bench_start(&f.test, &f.k1, &input, start + 20u) == BSP_STATUS_BUSY, "duplicate cannot extend pulse");
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, start + 249u) == BSP_STATUS_OK && f.level, "before fixed deadline");
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, start + 250u) == BSP_STATUS_OK && !f.level, "LOW at fixed deadline");
    expect(f.test.state == HW_K1_BENCH_RELEASE && hw_k1_bench_owns_k1(&f.test), "release owns until guard ends");
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, start + 257u) == BSP_STATUS_OK, "release not complete early");
    expect(f.test.state == HW_K1_BENCH_RELEASE, "full release guard required");
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, start + 258u) == BSP_STATUS_OK, "release deadline");
    expect(f.test.state == HW_K1_BENCH_DONE && !hw_k1_bench_owns_k1(&f.test), "done is SAFE and no owner");
    expect(hw_k1_bench_start(&f.test, &f.k1, &input, start + 300u) == BSP_STATUS_ERROR, "one attempt per initialization");
    expect(f.high_writes == 1u && !f.level, "terminal retry never energizes");
}

static void test_cancel_and_late_step(void)
{
    fixture_t f;
    setup(&f);
    const hw_k1_bench_input_t input = ready();
    expect(hw_k1_bench_cancel(&f.test, &f.k1, 0u) == BSP_STATUS_OK && !f.test.used, "idle cancel has no pulse");
    expect(hw_k1_bench_start(&f.test, &f.k1, &input, 0u) == BSP_STATUS_OK, "cancel setup");
    expect(hw_k1_bench_cancel(&f.test, &f.k1, 7u) == BSP_STATUS_OK && !f.level, "cancel immediately LOW");
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, 15u) == BSP_STATUS_OK && f.test.state == HW_K1_BENCH_ABORTED, "cancel completes guard");
    setup(&f);
    expect(hw_k1_bench_start(&f.test, &f.k1, &input, 0u) == BSP_STATUS_OK, "late-step setup");
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, 600u) == BSP_STATUS_OK && !f.level, "late main loop turns LOW without rearm");
}

static void test_io_failures(void)
{
    fixture_t f;
    setup(&f);
    const hw_k1_bench_input_t input = ready();
    f.fail_high = true;
    expect(hw_k1_bench_start(&f.test, &f.k1, &input, 0u) == BSP_STATUS_ERROR, "HIGH failure visible");
    expect(!f.level && f.test.used, "failed HIGH immediately attempts SAFE and consumes one shot");
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, 8u) == BSP_STATUS_OK && f.test.state == HW_K1_BENCH_IO_ERROR, "failed start never reports DONE");
    setup(&f);
    expect(hw_k1_bench_start(&f.test, &f.k1, &input, 0u) == BSP_STATUS_OK, "LOW failure setup");
    f.fail_low = true;
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, 250u) == BSP_STATUS_ERROR, "LOW failure visible");
    expect(f.level && hw_k1_bench_owns_k1(&f.test), "failed LOW retains cleanup ownership");
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, 300u) == BSP_STATUS_ERROR, "LOW is retried");
    f.fail_low = false;
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, 301u) == BSP_STATUS_OK && !f.level, "eventual LOW succeeds");
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, 309u) == BSP_STATUS_OK && f.test.state == HW_K1_BENCH_IO_ERROR, "I/O failure remains diagnostic after cleanup");
    expect(f.high_writes == 1u, "cleanup never reasserts HIGH");
}

static void test_unexpected_state_and_null(void)
{
    fixture_t f;
    setup(&f);
    const hw_k1_bench_input_t input = ready();
    expect(hw_k1_bench_start(&f.test, &f.k1, NULL, 0u) == BSP_STATUS_ERROR && !f.level, "missing input fails closed");
    expect(hw_k1_bench_start(NULL, &f.k1, &input, 0u) == BSP_STATUS_INVALID_ARG, "missing context");
    expect(hw_k1_bench_step(&f.test, NULL, &input, 0u) == BSP_STATUS_INVALID_ARG, "missing relay");
    expect(hw_k1_bench_start(&f.test, &f.k1, &input, 0u) == BSP_STATUS_OK, "unexpected state setup");
    expect(hw_k1_force_safe(&f.k1) == BSP_STATUS_OK, "external SAFE write");
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, 1u) == BSP_STATUS_OK && !f.level, "unexpected SAFE aborts without reenergizing");
    expect(f.test.completion == HW_K1_BENCH_ABORTED && f.high_writes == 1u, "unexpected state recorded");
    setup(&f);
    expect(hw_k1_bench_start(&f.test, &f.k1, &input, 0u) == BSP_STATUS_OK, "null-active input setup");
    expect(hw_k1_bench_step(&f.test, &f.k1, NULL, 1u) == BSP_STATUS_OK && !f.level, "lost active input forces SAFE");
    f.k1.commanded_state = HW_K1_STATE_UNKNOWN;
    f.level = true;
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, 2u) == BSP_STATUS_OK && !f.level, "release reasserts SAFE if commanded state is lost");
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, 9u) == BSP_STATUS_OK && f.test.state == HW_K1_BENCH_RELEASE, "lost state restarts release guard");
    expect(hw_k1_bench_step(&f.test, &f.k1, &input, 10u) == BSP_STATUS_OK && f.test.state == HW_K1_BENCH_ABORTED, "recovered release completes");
    setup(&f);
    f.k1.commanded_state = HW_K1_STATE_UNKNOWN;
    expect(hw_k1_bench_start(&f.test, &f.k1, &input, 0u) == BSP_STATUS_ERROR && f.high_writes == 0u, "unknown initial relay state is blocked");
}

int main(void)
{
    test_guard_matrix();
    test_deadline_and_one_shot(100u);
    test_deadline_and_one_shot(UINT32_MAX - 100u);
    test_deadline_and_one_shot(UINT32_MAX - 251u);
    test_cancel_and_late_step();
    test_io_failures();
    test_unexpected_state_and_null();
    return failures == 0 ? 0 : 1;
}
