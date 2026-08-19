#include "app/app_safety_fault.h"

#include <stddef.h>

void app_safety_fault_init(app_safety_fault_latch_t *latch)
{
    if (latch == NULL)
    {
        return;
    }

    latch->latched_mask = APP_SAFETY_FAULT_NONE;
}

void app_safety_fault_latch(app_safety_fault_latch_t *latch, uint32_t fault_mask)
{
    if (latch == NULL)
    {
        return;
    }

    latch->latched_mask |= fault_mask;
}

uint32_t app_safety_fault_mask(const app_safety_fault_latch_t *latch)
{
    return (latch == NULL) ? APP_SAFETY_FAULT_NONE : latch->latched_mask;
}

bool app_safety_fault_any(const app_safety_fault_latch_t *latch)
{
    return app_safety_fault_mask(latch) != APP_SAFETY_FAULT_NONE;
}
