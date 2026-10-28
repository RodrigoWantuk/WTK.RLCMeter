#include "ui/ui_text.h"

bool ui_language_valid(uint8_t language_id)
{
    return (language_id == (uint8_t)UI_LANGUAGE_EN) ||
           (language_id == (uint8_t)UI_LANGUAGE_PT_BR);
}

uint32_t ui_language_text_resource_id(uint8_t language_id)
{
    switch (language_id)
    {
    case UI_LANGUAGE_EN:
        return RESOURCE_ID_TEXT_EN;
    case UI_LANGUAGE_PT_BR:
        return RESOURCE_ID_TEXT_PT_BR;
    default:
        return 0u;
    }
}

bool ui_text_is_emergency(ui_text_id_t id)
{
    switch (id)
    {
    case UI_TEXT_ID_WAVEFORM_TITLE:
    case UI_TEXT_ID_WAVEFORM_UNAVAILABLE:
    case UI_TEXT_ID_WAVEFORM_INDIVIDUAL:
    case UI_TEXT_ID_WAVEFORM_ADC_PHASE:
    case UI_TEXT_ID_WAVEFORM_WEAK:
    case UI_TEXT_ID_WAVEFORM_CLIPPED:
    case UI_TEXT_ID_WAVEFORM_NO_TRIGGER:
    case UI_TEXT_ID_WAVEFORM_BACK:
    case UI_TEXT_ID_NOT_CHARACTERIZED:
    case UI_TEXT_ID_INVALID_RESULT:
    case UI_TEXT_ID_NOT_APPLICABLE:
    case UI_TEXT_ID_ERROR_LABEL:
    case UI_TEXT_ID_PARTIAL:
    case UI_TEXT_ID_CRC_UNVERIFIED:
    case UI_TEXT_ID_CHARGER_CONNECTED:
    case UI_TEXT_ID_UNPLUG_TO_MEASURE:
    case UI_TEXT_ID_HOLD_OK_MENU:
    case UI_TEXT_ID_WTK_RLCMETER:
    case UI_TEXT_ID_RESOURCE_ERROR:
    case UI_TEXT_ID_STORAGE_ERROR:
    case UI_TEXT_ID_CALIBRATION_REQUIRED:
    case UI_TEXT_ID_FAULT:
    case UI_TEXT_ID_REMOVE_CHARGER:
    case UI_TEXT_ID_VOLTAGE_DETECTED:
    case UI_TEXT_ID_SENSOR_ERROR:
    case UI_TEXT_ID_SUPPLY_ERROR:
    case UI_TEXT_ID_RANGE_ERROR:
    case UI_TEXT_ID_MEASURING:
    case UI_TEXT_ID_OPEN:
    case UI_TEXT_ID_SHORT:
    case UI_TEXT_ID_LOAD:
        return true;
    default:
        return false;
    }
}

const char *ui_text_emergency(ui_text_id_t id)
{
    switch (id)
    {
    case UI_TEXT_ID_WAVEFORM_TITLE: return "Waveforms";
    case UI_TEXT_ID_WAVEFORM_UNAVAILABLE: return "Snapshot unavailable";
    case UI_TEXT_ID_WAVEFORM_INDIVIDUAL: return "Individual ADC capture";
    case UI_TEXT_ID_WAVEFORM_ADC_PHASE: return "ADC phase";
    case UI_TEXT_ID_WAVEFORM_WEAK: return "Weak signal";
    case UI_TEXT_ID_WAVEFORM_CLIPPED: return "Clipping";
    case UI_TEXT_ID_WAVEFORM_NO_TRIGGER: return "No trigger";
    case UI_TEXT_ID_WAVEFORM_BACK: return "OK: value   Hold OK: menu";

    case UI_TEXT_ID_NOT_CHARACTERIZED: return "UNQUALIFIED";
    case UI_TEXT_ID_INVALID_RESULT: return "INVALID";
    case UI_TEXT_ID_NOT_APPLICABLE: return "N/A";
    case UI_TEXT_ID_ERROR_LABEL: return "ERROR:";
    case UI_TEXT_ID_PARTIAL: return "PARTIAL";
    case UI_TEXT_ID_CRC_UNVERIFIED: return "CRC UNVERIFIED";
    case UI_TEXT_ID_CHARGER_CONNECTED: return "USB";
    case UI_TEXT_ID_UNPLUG_TO_MEASURE: return "UNPLUG USB";
    case UI_TEXT_ID_HOLD_OK_MENU: return "HOLD OK: MENU";
    case UI_TEXT_ID_WTK_RLCMETER:
        return "WTK RLC";
    case UI_TEXT_ID_RESOURCE_ERROR:
        return "RESOURCE ERR";
    case UI_TEXT_ID_STORAGE_ERROR:
        return "STORAGE ERR";
    case UI_TEXT_ID_CALIBRATION_REQUIRED:
        return "CAL REQUIRED";
    case UI_TEXT_ID_FAULT:
        return "FAULT";
    case UI_TEXT_ID_REMOVE_CHARGER:
        return "UNPLUG USB";
    case UI_TEXT_ID_VOLTAGE_DETECTED:
        return "VOLTAGE";
    case UI_TEXT_ID_SENSOR_ERROR:
        return "SENSOR ERR";
    case UI_TEXT_ID_SUPPLY_ERROR:
        return "SUPPLY ERR";
    case UI_TEXT_ID_RANGE_ERROR:
        return "RANGE ERR";
    case UI_TEXT_ID_MEASURING:
        return "MEASURE";
    case UI_TEXT_ID_OPEN:
        return "OPEN";
    case UI_TEXT_ID_SHORT:
        return "SHORT";
    case UI_TEXT_ID_LOAD:
        return "LOAD";
    default:
        return "?";
    }
}
