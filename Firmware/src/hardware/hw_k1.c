#include "hardware/hw_k1.h"

#include <stddef.h>

static bsp_status_t write_safe(hw_k1_t *k1)
{
    if ((k1 == NULL) || (k1->io.write_cmd == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    const bsp_status_t status = k1->io.write_cmd(false, k1->io.user_data);
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
    k1->commanded_state = HW_K1_STATE_SAFE;
    return write_safe(k1);
}

bsp_status_t hw_k1_force_safe(hw_k1_t *k1)
{
    return write_safe(k1);
}

bsp_status_t hw_k1_request_measure(hw_k1_t *k1, const hw_safety_result_t *permission)
{
    if ((k1 == NULL) || (k1->io.write_cmd == NULL) || (permission == NULL))
    {
        if (k1 != NULL)
        {
            (void)write_safe(k1);
        }
        return BSP_STATUS_INVALID_ARG;
    }

    if (!permission->measure_allowed || (permission->blocker_flags != HW_SAFETY_BLOCK_NONE))
    {
        (void)write_safe(k1);
        return BSP_STATUS_ERROR;
    }

    const bsp_status_t status = k1->io.write_cmd(true, k1->io.user_data);
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
        return HW_K1_STATE_SAFE;
    }
    return k1->commanded_state;
}

const char *hw_k1_state_string(hw_k1_state_t state)
{
    switch (state)
    {
    case HW_K1_STATE_MEASURE:
        return "MEASURE";
    case HW_K1_STATE_SAFE:
    default:
        return "SAFE";
    }
}
