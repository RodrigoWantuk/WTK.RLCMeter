#ifndef WTK_HW_RANGE_H
#define WTK_HW_RANGE_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "hardware/hw_safety.h"

enum
{
    HW_RANGE_DEAD_TIME_MS = 2u,
    HW_RANGE_SETTLE_TIME_MS = 5u,
};

typedef enum
{
    HW_RANGE_ID_10R = 0,
    HW_RANGE_ID_100R,
    HW_RANGE_ID_1K,
    HW_RANGE_ID_10K,
    HW_RANGE_ID_100K,
    HW_RANGE_ID_1M,
    HW_RANGE_ID_INVALID = 255,
} hw_range_id_t;

typedef enum
{
    HW_RANGE_FSM_DISABLED = 0,
    HW_RANGE_FSM_DEAD_TIME,
    HW_RANGE_FSM_SETTLING,
    HW_RANGE_FSM_READY,
    HW_RANGE_FSM_INVALID,
} hw_range_fsm_state_t;

typedef bsp_status_t (*hw_range_write_enable_fn)(bool high, void *user_data);
typedef bsp_status_t (*hw_range_write_address_fn)(uint8_t address, void *user_data);

typedef struct
{
    hw_range_write_enable_fn write_enable;
    hw_range_write_address_fn write_address;
    void *user_data;
} hw_range_io_t;

typedef struct
{
    hw_range_io_t io;
    hw_range_fsm_state_t state;
    hw_range_id_t current;
    hw_range_id_t requested;
    uint8_t address;
    uint32_t deadline_ms;
    bool enabled;
} hw_range_t;

bsp_status_t hw_range_init(hw_range_t *range, const hw_range_io_t *io);
bsp_status_t hw_range_request(hw_range_t *range, hw_range_id_t id, uint32_t now_ms);
void hw_range_step(hw_range_t *range, uint32_t now_ms);
void hw_range_force_disabled(hw_range_t *range);
bool hw_range_is_ready(const hw_range_t *range);
hw_range_id_t hw_range_get_current(const hw_range_t *range);
hw_range_id_t hw_range_get_requested(const hw_range_t *range);
hw_range_fsm_state_t hw_range_get_state(const hw_range_t *range);
hw_safety_range_state_t hw_range_safety_state(const hw_range_t *range);
bsp_status_t hw_range_id_to_address(hw_range_id_t id, uint8_t *address);
const char *hw_range_id_string(hw_range_id_t id);
const char *hw_range_fsm_state_string(hw_range_fsm_state_t state);

#endif
