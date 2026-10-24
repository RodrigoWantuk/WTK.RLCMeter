#ifndef WTK_HW_K1_BENCH_H
#define WTK_HW_K1_BENCH_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/hw_k1.h"

enum
{
    HW_K1_BENCH_PULSE_MS = 250u,
    HW_K1_BENCH_RELEASE_MS = 8u,
    HW_K1_BENCH_RESIDUAL_MAX_AGE_MS = 20u,
};

typedef enum
{
    HW_K1_BENCH_IDLE = 0,
    HW_K1_BENCH_PULSE,
    HW_K1_BENCH_RELEASE,
    HW_K1_BENCH_DONE,
    HW_K1_BENCH_ABORTED,
    HW_K1_BENCH_IO_ERROR,
} hw_k1_bench_state_t;

/* A distinct empty-fixture test, never a measurement permission or fake battery. */
typedef struct
{
    hw_charger_state_t charger;
    hw_residual_state_t residual;
    uint32_t residual_age_ms;
    bool vmid_valid;
    bool range_disabled;
    bool excitation_off;
    bool idle;
    bool clock_ready;
    bool fixture_empty_confirmed;
    bool faulted;
} hw_k1_bench_input_t;

typedef struct
{
    hw_k1_bench_state_t state;
    hw_k1_bench_state_t completion;
    uint32_t deadline_ms;
    bool used;
    bool safe_written;
} hw_k1_bench_t;

/* Defined only in host tests or the explicitly enabled BRINGUP variant. */
void hw_k1_bench_init(hw_k1_bench_t *test);
bsp_status_t hw_k1_bench_start(hw_k1_bench_t *test, hw_k1_t *k1,
                              const hw_k1_bench_input_t *input, uint32_t now_ms);
bsp_status_t hw_k1_bench_step(hw_k1_bench_t *test, hw_k1_t *k1,
                             const hw_k1_bench_input_t *input, uint32_t now_ms);
bsp_status_t hw_k1_bench_cancel(hw_k1_bench_t *test, hw_k1_t *k1, uint32_t now_ms);
bool hw_k1_bench_owns_k1(const hw_k1_bench_t *test);
const char *hw_k1_bench_state_string(hw_k1_bench_state_t state);

#endif
