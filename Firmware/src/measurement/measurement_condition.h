#ifndef WTK_MEASUREMENT_CONDITION_H
#define WTK_MEASUREMENT_CONDITION_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/hw_excitation.h"
#include "hardware/hw_range.h"

enum
{
    MEASUREMENT_CONDITION_REV1_MAX_SUPPORTED = 33u,
};

typedef enum
{
    MEASUREMENT_CONDITION_SUPPORT_OK = 0,
    MEASUREMENT_CONDITION_SUPPORT_INVALID_RANGE,
    MEASUREMENT_CONDITION_SUPPORT_INVALID_FREQUENCY,
    MEASUREMENT_CONDITION_SUPPORT_INVALID_AMPLITUDE,
    MEASUREMENT_CONDITION_SUPPORT_FORBIDDEN_10R_500MV,
} measurement_condition_support_t;

measurement_condition_support_t measurement_condition_support_status(
    hw_range_id_t range_id,
    hw_excitation_freq_t frequency,
    hw_excitation_amp_t amplitude);
bool measurement_condition_supported(hw_range_id_t range_id,
                                     hw_excitation_freq_t frequency,
                                     hw_excitation_amp_t amplitude);
bool measurement_condition_calibratable(hw_range_id_t range_id,
                                        hw_excitation_freq_t frequency,
                                        hw_excitation_amp_t amplitude);
const char *measurement_condition_support_string(measurement_condition_support_t status);

#endif
