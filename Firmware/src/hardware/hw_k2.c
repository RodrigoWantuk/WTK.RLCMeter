#include "hardware/hw_k2.h"

#include <stddef.h>

static const hw_k2_topology_t g_rev1_topology = {
    .populated = false,
    .lowz_bank_mode = HW_LOWZ_BANK_MODE_FIXED_0R_LINK,
};

bsp_status_t hw_k2_init(hw_k2_t *k2, const hw_k2_io_t *io)
{
    if ((k2 == NULL) || (io == NULL) || (io->write_cmd == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    k2->io = *io;
    k2->topology = g_rev1_topology;
    return k2->io.write_cmd(false, k2->io.user_data);
}

bsp_status_t hw_k2_request_physical_switch(hw_k2_t *k2)
{
    if ((k2 == NULL) || (k2->io.write_cmd == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    const bsp_status_t status = k2->io.write_cmd(false, k2->io.user_data);
    return (status == BSP_STATUS_OK) ? BSP_STATUS_NOT_SUPPORTED : status;
}

hw_k2_topology_t hw_k2_topology(const hw_k2_t *k2)
{
    if (k2 == NULL)
    {
        return g_rev1_topology;
    }
    return k2->topology;
}

const char *hw_lowz_bank_mode_string(hw_lowz_bank_mode_t mode)
{
    switch (mode)
    {
    case HW_LOWZ_BANK_MODE_FIXED_0R_LINK:
    default:
        return "FIXED_0R_LINK";
    }
}
