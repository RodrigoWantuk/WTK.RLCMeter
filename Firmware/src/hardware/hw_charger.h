#ifndef WTK_HW_CHARGER_H
#define WTK_HW_CHARGER_H

#include <stdbool.h>

#include "bsp/bsp_status.h"
#include "hardware/hw_safety.h"

typedef bsp_status_t (*hw_charger_read_gpio_fn)(bool *high, void *user_data);

typedef struct
{
    hw_charger_read_gpio_fn read_gpio;
    void *user_data;
} hw_charger_io_t;

typedef struct
{
    hw_charger_io_t io;
} hw_charger_t;

bsp_status_t hw_charger_init(hw_charger_t *charger, const hw_charger_io_t *io);
hw_charger_state_t hw_charger_state_from_gpio(bsp_status_t read_status, bool high);
hw_charger_state_t hw_charger_get_state(hw_charger_t *charger);

#endif
