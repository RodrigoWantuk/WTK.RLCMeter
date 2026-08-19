#include "hardware/hw_range.h"

#include <stddef.h>

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (now_ms - deadline_ms) < 0x80000000u;
}

static bsp_status_t write_enable(hw_range_t *range, bool high)
{
    if ((range == NULL) || (range->io.write_enable == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    const bsp_status_t status = range->io.write_enable(high, range->io.user_data);
    if (status == BSP_STATUS_OK)
    {
        range->enabled = high;
    }
    return status;
}

static bsp_status_t write_address(hw_range_t *range, uint8_t address)
{
    if ((range == NULL) || (range->io.write_address == NULL) || range->enabled)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    const bsp_status_t status = range->io.write_address(address, range->io.user_data);
    if (status == BSP_STATUS_OK)
    {
        range->address = address;
    }
    return status;
}

bsp_status_t hw_range_id_to_address(hw_range_id_t id, uint8_t *address)
{
    if (address == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    switch (id)
    {
    case HW_RANGE_ID_10R:
        *address = 0u;
        return BSP_STATUS_OK;
    case HW_RANGE_ID_100R:
        *address = 1u;
        return BSP_STATUS_OK;
    case HW_RANGE_ID_1K:
        *address = 2u;
        return BSP_STATUS_OK;
    case HW_RANGE_ID_10K:
        *address = 3u;
        return BSP_STATUS_OK;
    case HW_RANGE_ID_100K:
        *address = 4u;
        return BSP_STATUS_OK;
    case HW_RANGE_ID_1M:
        *address = 5u;
        return BSP_STATUS_OK;
    case HW_RANGE_ID_INVALID:
    default:
        return BSP_STATUS_INVALID_ARG;
    }
}

bsp_status_t hw_range_init(hw_range_t *range, const hw_range_io_t *io)
{
    if ((range == NULL) || (io == NULL) || (io->write_enable == NULL) || (io->write_address == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    range->io = *io;
    range->state = HW_RANGE_FSM_DISABLED;
    range->current = HW_RANGE_ID_INVALID;
    range->requested = HW_RANGE_ID_INVALID;
    range->address = 0u;
    range->deadline_ms = 0u;
    range->enabled = false;

    bsp_status_t status = write_enable(range, false);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    status = write_address(range, 0u);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    return BSP_STATUS_OK;
}

bsp_status_t hw_range_request(hw_range_t *range, hw_range_id_t id, uint32_t now_ms)
{
    uint8_t address = 0u;
    if ((range == NULL) || (hw_range_id_to_address(id, &address) != BSP_STATUS_OK))
    {
        if (range != NULL)
        {
            (void)write_enable(range, false);
            range->state = HW_RANGE_FSM_INVALID;
            range->current = HW_RANGE_ID_INVALID;
            range->requested = HW_RANGE_ID_INVALID;
        }
        return BSP_STATUS_INVALID_ARG;
    }

    bsp_status_t status = write_enable(range, false);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    status = write_address(range, address);
    if (status != BSP_STATUS_OK)
    {
        range->state = HW_RANGE_FSM_INVALID;
        return status;
    }

    range->requested = id;
    range->state = HW_RANGE_FSM_DEAD_TIME;
    range->deadline_ms = now_ms + HW_RANGE_DEAD_TIME_MS;
    return BSP_STATUS_BUSY;
}

void hw_range_step(hw_range_t *range, uint32_t now_ms)
{
    if (range == NULL)
    {
        return;
    }

    switch (range->state)
    {
    case HW_RANGE_FSM_DEAD_TIME:
        if (deadline_reached(now_ms, range->deadline_ms) && (write_enable(range, true) == BSP_STATUS_OK))
        {
            range->state = HW_RANGE_FSM_SETTLING;
            range->deadline_ms = now_ms + HW_RANGE_SETTLE_TIME_MS;
        }
        break;
    case HW_RANGE_FSM_SETTLING:
        if (deadline_reached(now_ms, range->deadline_ms))
        {
            range->current = range->requested;
            range->state = HW_RANGE_FSM_READY;
        }
        break;
    case HW_RANGE_FSM_DISABLED:
    case HW_RANGE_FSM_READY:
    case HW_RANGE_FSM_INVALID:
    default:
        break;
    }
}

void hw_range_force_disabled(hw_range_t *range)
{
    if (range == NULL)
    {
        return;
    }

    (void)write_enable(range, false);
    range->state = HW_RANGE_FSM_DISABLED;
    range->current = HW_RANGE_ID_INVALID;
}

bool hw_range_is_ready(const hw_range_t *range)
{
    return (range != NULL) && (range->state == HW_RANGE_FSM_READY) && range->enabled;
}

hw_range_id_t hw_range_get_current(const hw_range_t *range)
{
    return (range == NULL) ? HW_RANGE_ID_INVALID : range->current;
}

hw_range_id_t hw_range_get_requested(const hw_range_t *range)
{
    return (range == NULL) ? HW_RANGE_ID_INVALID : range->requested;
}

hw_range_fsm_state_t hw_range_get_state(const hw_range_t *range)
{
    return (range == NULL) ? HW_RANGE_FSM_INVALID : range->state;
}

hw_safety_range_state_t hw_range_safety_state(const hw_range_t *range)
{
    if (range == NULL)
    {
        return HW_RANGE_INVALID;
    }

    switch (range->state)
    {
    case HW_RANGE_FSM_READY:
        return range->enabled ? HW_RANGE_READY : HW_RANGE_INVALID;
    case HW_RANGE_FSM_DISABLED:
        return HW_RANGE_DISABLED;
    case HW_RANGE_FSM_DEAD_TIME:
    case HW_RANGE_FSM_SETTLING:
        return HW_RANGE_TRANSITIONING;
    case HW_RANGE_FSM_INVALID:
    default:
        return HW_RANGE_INVALID;
    }
}

const char *hw_range_id_string(hw_range_id_t id)
{
    switch (id)
    {
    case HW_RANGE_ID_10R:
        return "10R";
    case HW_RANGE_ID_100R:
        return "100R";
    case HW_RANGE_ID_1K:
        return "1K";
    case HW_RANGE_ID_10K:
        return "10K";
    case HW_RANGE_ID_100K:
        return "100K";
    case HW_RANGE_ID_1M:
        return "1M";
    case HW_RANGE_ID_INVALID:
    default:
        return "INVALID";
    }
}

const char *hw_range_fsm_state_string(hw_range_fsm_state_t state)
{
    switch (state)
    {
    case HW_RANGE_FSM_DISABLED:
        return "DISABLED";
    case HW_RANGE_FSM_DEAD_TIME:
        return "DEAD_TIME";
    case HW_RANGE_FSM_SETTLING:
        return "SETTLING";
    case HW_RANGE_FSM_READY:
        return "READY";
    case HW_RANGE_FSM_INVALID:
    default:
        return "INVALID";
    }
}
