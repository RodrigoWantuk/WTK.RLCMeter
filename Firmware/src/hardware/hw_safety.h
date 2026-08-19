#ifndef WTK_HW_SAFETY_H
#define WTK_HW_SAFETY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    HW_CHARGER_ABSENT = 0,
    HW_CHARGER_PRESENT,
    HW_CHARGER_UNKNOWN,
} hw_charger_state_t;

typedef enum
{
    HW_RESIDUAL_SAFE = 0,
    HW_RESIDUAL_UNSAFE,
    HW_RESIDUAL_UNKNOWN,
    HW_RESIDUAL_SATURATED,
} hw_residual_state_t;

typedef enum
{
    HW_BATTERY_OK = 0,
    HW_BATTERY_LOW,
    HW_BATTERY_CRITICAL,
    HW_BATTERY_UNKNOWN,
} hw_battery_state_t;

typedef enum
{
    HW_RANGE_READY = 0,
    HW_RANGE_DISABLED,
    HW_RANGE_TRANSITIONING,
    HW_RANGE_INVALID,
} hw_safety_range_state_t;

typedef enum
{
    HW_SAFETY_BLOCK_NONE = 0u,
    HW_SAFETY_BLOCK_CHARGER = 1u << 0,
    HW_SAFETY_BLOCK_RESIDUAL = 1u << 1,
    HW_SAFETY_BLOCK_SENSOR_INVALID = 1u << 2,
    HW_SAFETY_BLOCK_RANGE = 1u << 3,
    HW_SAFETY_BLOCK_SUPPLY = 1u << 4,
    HW_SAFETY_BLOCK_FAULT = 1u << 5,
} hw_safety_block_t;

typedef enum
{
    HW_SAFETY_MEASURE_ALLOWED = 0,
    HW_SAFETY_BLOCKED_FAULT,
    HW_SAFETY_BLOCKED_CHARGER,
    HW_SAFETY_BLOCKED_SENSOR_INVALID,
    HW_SAFETY_BLOCKED_RESIDUAL,
    HW_SAFETY_BLOCKED_SUPPLY,
    HW_SAFETY_BLOCKED_RANGE,
} hw_safety_primary_blocker_t;

typedef struct
{
    hw_charger_state_t charger;
    hw_residual_state_t residual;
    hw_battery_state_t battery;
    hw_safety_range_state_t range;
    bool application_fault;
} hw_safety_input_t;

typedef struct
{
    bool measure_allowed;
    bool battery_low;
    uint32_t blocker_flags;
    hw_safety_primary_blocker_t primary_blocker;
} hw_safety_result_t;

hw_safety_result_t hw_safety_evaluate(const hw_safety_input_t *input);
const char *hw_safety_primary_blocker_string(hw_safety_primary_blocker_t blocker);
const char *hw_charger_state_string(hw_charger_state_t state);
const char *hw_residual_state_string(hw_residual_state_t state);
const char *hw_battery_state_string(hw_battery_state_t state);
const char *hw_safety_range_state_string(hw_safety_range_state_t state);

#endif
