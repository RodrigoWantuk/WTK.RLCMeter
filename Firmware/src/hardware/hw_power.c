#include "hardware/hw_power.h"

#include <stdbool.h>
#include <stddef.h>

float hw_battery_vbat_from_adc_voltage(float adc_bat_v)
{
    return adc_bat_v * HW_BATTERY_DIVIDER_RATIO;
}

hw_battery_state_t hw_battery_state_from_voltage(float vbat_v, bool valid)
{
    if (!valid)
    {
        return HW_BATTERY_UNKNOWN;
    }
    if (vbat_v <= HW_BATTERY_CRITICAL_V)
    {
        return HW_BATTERY_CRITICAL;
    }
    if (vbat_v <= HW_BATTERY_LOW_V)
    {
        return HW_BATTERY_LOW;
    }
    return HW_BATTERY_OK;
}

float hw_ntc_resistance_from_voltage(float adc_ntc_v, float vdd_v, bool *valid)
{
    if ((valid == NULL) || (vdd_v <= 0.0f) || (adc_ntc_v <= 0.0f) || (adc_ntc_v >= vdd_v))
    {
        if (valid != NULL)
        {
            *valid = false;
        }
        return 0.0f;
    }

    *valid = true;
    return (HW_NTC_FIXED_TOP_OHM * adc_ntc_v) / (vdd_v - adc_ntc_v);
}
