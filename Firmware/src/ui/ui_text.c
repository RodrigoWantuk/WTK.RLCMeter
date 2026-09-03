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
        return true;
    default:
        return false;
    }
}

const char *ui_text_emergency(ui_text_id_t id)
{
    switch (id)
    {
    case UI_TEXT_ID_WTK_RLCMETER:
        return "WTK RLC";
    case UI_TEXT_ID_RESOURCE_ERROR:
        return "RESOURCE ERROR";
    case UI_TEXT_ID_STORAGE_ERROR:
        return "STORAGE ERROR";
    case UI_TEXT_ID_CALIBRATION_REQUIRED:
        return "CALIBRATION REQUIRED";
    case UI_TEXT_ID_FAULT:
        return "FAULT";
    case UI_TEXT_ID_REMOVE_CHARGER:
        return "REMOVE CHARGER";
    case UI_TEXT_ID_VOLTAGE_DETECTED:
        return "VOLTAGE DETECTED";
    case UI_TEXT_ID_SENSOR_ERROR:
        return "SENSOR ERROR";
    case UI_TEXT_ID_SUPPLY_ERROR:
        return "SUPPLY ERROR";
    case UI_TEXT_ID_RANGE_ERROR:
        return "RANGE ERROR";
    case UI_TEXT_ID_MEASURING:
        return "MEASURING";
    case UI_TEXT_ID_OPEN:
        return "OPEN";
    case UI_TEXT_ID_SHORT:
        return "SHORT";
    default:
        return "?";
    }
}
