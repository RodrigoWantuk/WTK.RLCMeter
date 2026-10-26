#include "hardware/hw_power.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

static int check_anchor(unsigned raw, float reference_c)
{
    bool valid = false;
    float temperature = 0.0f;
    const float voltage = (float)raw * (3.3f / 4095.0f);
    const float resistance = hw_ntc_resistance_from_voltage(voltage, 3.3f, &valid);
    if (!valid || !hw_ntc_temperature_from_resistance(resistance, &temperature) ||
        fabsf(temperature - reference_c) > 0.02f)
    {
        (void)fprintf(stderr, "factory anchor %u: %.6f expected %.6f\n",
                      raw, (double)temperature, (double)reference_c);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = check_anchor(1952u, 23.5f) + check_anchor(1856u, 30.1f);
    float ignored = 0.0f;
    if (hw_ntc_temperature_from_resistance(0.0f, &ignored) ||
        hw_battery_state_from_voltage(3.5f, true) != HW_BATTERY_LOW ||
        hw_battery_state_from_voltage(3.25f, true) != HW_BATTERY_CRITICAL)
    {
        failures++;
    }
    return failures;
}
