#include "measurement/measurement_condition.h"

#include "bsp/bsp_status.h"

static bool valid_frequency(hw_excitation_freq_t frequency)
{
    return (frequency == HW_EXCITATION_FREQ_100HZ) ||
           (frequency == HW_EXCITATION_FREQ_1KHZ) ||
           (frequency == HW_EXCITATION_FREQ_10KHZ);
}

static bool valid_amplitude(hw_excitation_amp_t amplitude)
{
    return (amplitude == HW_EXCITATION_AMP_100MVRMS) ||
           (amplitude == HW_EXCITATION_AMP_500MVRMS);
}

static bool valid_range(hw_range_id_t range_id)
{
    return range_id <= HW_RANGE_ID_1M;
}

measurement_condition_support_t measurement_condition_support_status(
    hw_range_id_t range_id,
    hw_excitation_freq_t frequency,
    hw_excitation_amp_t amplitude)
{
    if (!valid_range(range_id))
    {
        return MEASUREMENT_CONDITION_SUPPORT_INVALID_RANGE;
    }
    if (!valid_frequency(frequency))
    {
        return MEASUREMENT_CONDITION_SUPPORT_INVALID_FREQUENCY;
    }
    if (!valid_amplitude(amplitude))
    {
        return MEASUREMENT_CONDITION_SUPPORT_INVALID_AMPLITUDE;
    }
    if (hw_excitation_validate_amplitude(range_id, amplitude) == BSP_STATUS_NOT_SUPPORTED)
    {
        return MEASUREMENT_CONDITION_SUPPORT_FORBIDDEN_10R_500MV;
    }
    return MEASUREMENT_CONDITION_SUPPORT_OK;
}

bool measurement_condition_supported(hw_range_id_t range_id,
                                     hw_excitation_freq_t frequency,
                                     hw_excitation_amp_t amplitude)
{
    return measurement_condition_support_status(range_id, frequency, amplitude) ==
           MEASUREMENT_CONDITION_SUPPORT_OK;
}

bool measurement_condition_calibratable(hw_range_id_t range_id,
                                        hw_excitation_freq_t frequency,
                                        hw_excitation_amp_t amplitude)
{
    return measurement_condition_supported(range_id, frequency, amplitude);
}

const char *measurement_condition_support_string(measurement_condition_support_t status)
{
    switch (status)
    {
    case MEASUREMENT_CONDITION_SUPPORT_OK:
        return "OK";
    case MEASUREMENT_CONDITION_SUPPORT_INVALID_RANGE:
        return "INVALID_RANGE";
    case MEASUREMENT_CONDITION_SUPPORT_INVALID_FREQUENCY:
        return "INVALID_FREQUENCY";
    case MEASUREMENT_CONDITION_SUPPORT_INVALID_AMPLITUDE:
        return "INVALID_AMPLITUDE";
    case MEASUREMENT_CONDITION_SUPPORT_FORBIDDEN_10R_500MV:
    default:
        return "FORBIDDEN_10R_500MV";
    }
}
