#ifndef WTK_HW_PERIPHERALS_H
#define WTK_HW_PERIPHERALS_H

#include <stdbool.h>

void hw_peripherals_request_quiet(bool requested);
bool hw_peripherals_quiet_requested(void);

#endif
