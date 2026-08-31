#include "ui/ui_text.h"

const ui_text_id_t ui_text_required_ids[] = {
    UI_TEXT_ID_WTK_RLCMETER,
    UI_TEXT_ID_STARTING,
    UI_TEXT_ID_CAL_CHECK,
    UI_TEXT_ID_READY,
    UI_TEXT_ID_MENU,
    UI_TEXT_ID_CALIBRATION,
    UI_TEXT_ID_DISPLAY,
    UI_TEXT_ID_SOUND,
    UI_TEXT_ID_LANGUAGE,
    UI_TEXT_ID_ABOUT,
    UI_TEXT_ID_BACK,
    UI_TEXT_ID_BRIGHTNESS,
    UI_TEXT_ID_BACKLIGHT_TIMEOUT,
    UI_TEXT_ID_TIMEOUT,
    UI_TEXT_ID_ON,
    UI_TEXT_ID_OFF,
    UI_TEXT_ID_ACTIVE,
    UI_TEXT_ID_REQUIRED,
    UI_TEXT_ID_FULL_CALIBRATION,
    UI_TEXT_ID_SETTINGS_SAVE_FAILED,
    UI_TEXT_ID_MEASURING,
    UI_TEXT_ID_DETAILS,
    UI_TEXT_ID_ENGLISH,
    UI_TEXT_ID_PORTUGUESE_BR,
    UI_TEXT_ID_OPEN,
    UI_TEXT_ID_SHORT,
    UI_TEXT_ID_LOAD,
    UI_TEXT_ID_REFERENCE_KIT_REQUIRED,
    UI_TEXT_ID_CONNECT_REF,
    UI_TEXT_ID_OPEN_TERMINALS,
    UI_TEXT_ID_SHORT_TERMINALS,
    UI_TEXT_ID_OK_TO_START,
    UI_TEXT_ID_CALIBRATING,
    UI_TEXT_ID_COMPLETE,
    UI_TEXT_ID_SAVE_CALIBRATION,
    UI_TEXT_ID_SAVING_CALIBRATION,
    UI_TEXT_ID_CALIBRATION_SAVED,
    UI_TEXT_ID_SAFETY_BLOCKED,
    UI_TEXT_ID_CANCELING,
    UI_TEXT_ID_CALIBRATION_FAILED,
    UI_TEXT_ID_OK_RETRY_LONG_BACK,
    UI_TEXT_ID_PHASE,
    UI_TEXT_ID_GIT,
    UI_TEXT_ID_CAL_SCHEMA,
    UI_TEXT_ID_SEQUENCE,
};

const size_t ui_text_required_id_count = sizeof(ui_text_required_ids) / sizeof(ui_text_required_ids[0]);

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
