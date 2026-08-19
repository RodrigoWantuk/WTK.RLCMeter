#ifndef WTK_HW_POWER_H
#define WTK_HW_POWER_H

#include "hardware/hw_safety.h"

#define HW_BATTERY_DIVIDER_RATIO (2.0f)
#define HW_BATTERY_LOW_V (3.50f)
#define HW_BATTERY_CRITICAL_V (3.25f)
#define HW_BATTERY_MAX_PLAUSIBLE_V (4.35f)

#define HW_NTC_FIXED_TOP_OHM (100000.0f)
#define HW_NTC_R0_OHM (100000.0f)
#define HW_NTC_T0_K (298.15f)
#define HW_NTC_BETA_K (3950.0f)

float hw_battery_vbat_from_adc_voltage(float adc_bat_v);
hw_battery_state_t hw_battery_state_from_voltage(float vbat_v, bool valid);
float hw_ntc_resistance_from_voltage(float adc_ntc_v, float vdd_v, bool *valid);
bool hw_ntc_temperature_from_resistance(float resistance_ohm, float *temperature_c);

#endif
