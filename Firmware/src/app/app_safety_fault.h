#ifndef WTK_APP_SAFETY_FAULT_H
#define WTK_APP_SAFETY_FAULT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    APP_SAFETY_FAULT_NONE = 0u,
    APP_SAFETY_FAULT_GPIO_INIT = 1u << 0u,
    APP_SAFETY_FAULT_K1_IO = 1u << 1u,
    APP_SAFETY_FAULT_K2_IO = 1u << 2u,
    APP_SAFETY_FAULT_RANGE_IO = 1u << 3u,
    APP_SAFETY_FAULT_ADC_INIT = 1u << 4u,
    APP_SAFETY_FAULT_ADC_RUNTIME = 1u << 5u,
    APP_SAFETY_FAULT_CLOCK = 1u << 6u,
    APP_SAFETY_FAULT_METROLOGY_RUNTIME = 1u << 7u,
} app_safety_fault_t;

typedef struct
{
    uint32_t latched_mask;
} app_safety_fault_latch_t;

void app_safety_fault_init(app_safety_fault_latch_t *latch);
void app_safety_fault_latch(app_safety_fault_latch_t *latch, uint32_t fault_mask);
uint32_t app_safety_fault_mask(const app_safety_fault_latch_t *latch);
bool app_safety_fault_any(const app_safety_fault_latch_t *latch);

#endif
