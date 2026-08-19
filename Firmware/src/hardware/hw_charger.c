#include "hardware/hw_charger.h"

#include <stddef.h>

bsp_status_t hw_charger_init(hw_charger_t *charger, const hw_charger_io_t *io)
{
    if ((charger == NULL) || (io == NULL) || (io->read_gpio == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    charger->io = *io;
    return BSP_STATUS_OK;
}

hw_charger_state_t hw_charger_state_from_gpio(bsp_status_t read_status, bool high)
{
    if (read_status != BSP_STATUS_OK)
    {
        return HW_CHARGER_UNKNOWN;
    }
    return high ? HW_CHARGER_PRESENT : HW_CHARGER_ABSENT;
}

hw_charger_state_t hw_charger_get_state(hw_charger_t *charger)
{
    bool high = false;
    if ((charger == NULL) || (charger->io.read_gpio == NULL))
    {
        return HW_CHARGER_UNKNOWN;
    }

    return hw_charger_state_from_gpio(charger->io.read_gpio(&high, charger->io.user_data), high);
}
