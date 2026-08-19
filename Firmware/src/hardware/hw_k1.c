#include "hardware/hw_k1.h"

#include <stddef.h>

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
