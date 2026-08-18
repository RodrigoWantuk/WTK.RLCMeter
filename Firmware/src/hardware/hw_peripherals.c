#include "hardware/hw_peripherals.h"

#include "bsp/bsp_quiet.h"
#include "hardware/hw_buzzer.h"

void hw_peripherals_request_quiet(bool requested)
{
    bsp_quiet_request(requested);
    hw_buzzer_on_quiet_changed(requested);
}

bool hw_peripherals_quiet_requested(void)
{
    return bsp_quiet_requested();
}
