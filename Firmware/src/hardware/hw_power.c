#include "hardware/hw_power.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    float temperature_c;
    float resistance_ohm;
} ntc_lut_point_t;

static const ntc_lut_point_t k_ntc_lut[] = {
    {.temperature_c = -20.0f, .resistance_ohm = 1053847.0f},
    {.temperature_c = -15.0f, .resistance_ohm = 778981.0f},
    {.temperature_c = -10.0f, .resistance_ohm = 582457.0f},
    {.temperature_c = -5.0f, .resistance_ohm = 440260.0f},
    {.temperature_c = 0.0f, .resistance_ohm = 336206.0f},
    {.temperature_c = 5.0f, .resistance_ohm = 259246.0f},
    {.temperature_c = 10.0f, .resistance_ohm = 201746.0f},
    {.temperature_c = 15.0f, .resistance_ohm = 158371.0f},
    {.temperature_c = 20.0f, .resistance_ohm = 125353.0f},
    {.temperature_c = 25.0f, .resistance_ohm = 100000.0f},
    {.temperature_c = 30.0f, .resistance_ohm = 80371.0f},
    {.temperature_c = 35.0f, .resistance_ohm = 65055.0f},
    {.temperature_c = 40.0f, .resistance_ohm = 53015.0f},
    {.temperature_c = 45.0f, .resistance_ohm = 43481.0f},
    {.temperature_c = 50.0f, .resistance_ohm = 35882.0f},
    {.temperature_c = 55.0f, .resistance_ohm = 29784.0f},
    {.temperature_c = 60.0f, .resistance_ohm = 24862.0f},
    {.temperature_c = 65.0f, .resistance_ohm = 20864.0f},
    {.temperature_c = 70.0f, .resistance_ohm = 17598.0f},
    {.temperature_c = 75.0f, .resistance_ohm = 14917.0f},
    {.temperature_c = 80.0f, .resistance_ohm = 12703.0f},
};

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
    if (vbat_v > HW_BATTERY_MAX_PLAUSIBLE_V)
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

bool hw_ntc_temperature_from_resistance(float resistance_ohm, float *temperature_c)
{
    if ((temperature_c == NULL) || (resistance_ohm <= 0.0f))
    {
        return false;
    }

    const size_t point_count = sizeof(k_ntc_lut) / sizeof(k_ntc_lut[0]);
    if ((resistance_ohm > k_ntc_lut[0].resistance_ohm) ||
        (resistance_ohm < k_ntc_lut[point_count - 1u].resistance_ohm))
    {
        return false;
    }

    for (size_t i = 0u; i < point_count; i++)
    {
        if (resistance_ohm == k_ntc_lut[i].resistance_ohm)
        {
            *temperature_c = k_ntc_lut[i].temperature_c;
            return true;
        }
    }

    for (size_t i = 0u; i < (point_count - 1u); i++)
    {
        const ntc_lut_point_t hot = k_ntc_lut[i + 1u];
        const ntc_lut_point_t cold = k_ntc_lut[i];
        if ((resistance_ohm <= cold.resistance_ohm) && (resistance_ohm >= hot.resistance_ohm))
        {
            const float span_ohm = cold.resistance_ohm - hot.resistance_ohm;
            const float fraction = (cold.resistance_ohm - resistance_ohm) / span_ohm;
            *temperature_c = cold.temperature_c + (fraction * (hot.temperature_c - cold.temperature_c));
            return true;
        }
    }

    return false;
}
