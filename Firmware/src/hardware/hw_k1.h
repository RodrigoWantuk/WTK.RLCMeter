#ifndef WTK_HW_K1_H
#define WTK_HW_K1_H

#include <stdbool.h>

#include "bsp/bsp_status.h"
#include "hardware/hw_safety.h"

typedef enum
{
    HW_K1_STATE_UNKNOWN = 0,
    HW_K1_STATE_SAFE,
    HW_K1_STATE_MEASURE,
} hw_k1_state_t;

typedef bsp_status_t (*hw_k1_write_cmd_fn)(bool high, void *user_data);

typedef struct
{
    hw_k1_write_cmd_fn write_cmd;
    void *user_data;
} hw_k1_io_t;

typedef struct
{
    hw_k1_io_t io;
    hw_k1_state_t commanded_state;
    bsp_status_t last_status;
} hw_k1_t;

bsp_status_t hw_k1_init(hw_k1_t *k1, const hw_k1_io_t *io);
bsp_status_t hw_k1_force_safe(hw_k1_t *k1);
/*
 * Reserved for the future authorized measurement sequencer. Normal application
 * code must keep K1 SAFE during Phase 04 and must not call this directly.
 */
bsp_status_t hw_k1_request_measure(hw_k1_t *k1, const hw_safety_result_t *permission);
hw_k1_state_t hw_k1_commanded_state(const hw_k1_t *k1);
bsp_status_t hw_k1_last_status(const hw_k1_t *k1);
const char *hw_k1_state_string(hw_k1_state_t state);

#endif
