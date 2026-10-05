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
    case UI_TEXT_ID_STARTING:
        return "STARTING";
    case UI_TEXT_ID_CAL_CHECK:
        return "CAL CHECK";
    case UI_TEXT_ID_RESOURCE_ERROR:
        return "RESOURCE ERR";
    case UI_TEXT_ID_STORAGE_ERROR:
        return "STORAGE ERR";
    case UI_TEXT_ID_CALIBRATION_REQUIRED:
        return "CAL REQUIRED";
    case UI_TEXT_ID_READY:
        return "READY";
    case UI_TEXT_ID_MENU:
        return "MENU";
    case UI_TEXT_ID_CALIBRATION:
        return "CAL";
    case UI_TEXT_ID_DISPLAY:
        return "DISPLAY";
    case UI_TEXT_ID_SOUND:
        return "SOUND";
    case UI_TEXT_ID_ABOUT:
        return "ABOUT";
    case UI_TEXT_ID_BACK:
        return "BACK";
    case UI_TEXT_ID_BRIGHTNESS:
        return "BRIGHT";
    case UI_TEXT_ID_BACKLIGHT_TIMEOUT:
        return "BL TIMEOUT";
    case UI_TEXT_ID_ON:
        return "ON";
    case UI_TEXT_ID_OFF:
        return "OFF";
    case UI_TEXT_ID_ACTIVE:
        return "ACTIVE";
    case UI_TEXT_ID_REQUIRED:
        return "REQUIRED";
    case UI_TEXT_ID_FULL_CALIBRATION:
        return "FULL CAL";
    case UI_TEXT_ID_SETTINGS_SAVE_FAILED:
        return "SETTINGS ERR";
    case UI_TEXT_ID_LANGUAGE:
        return "LANG";
    case UI_TEXT_ID_ENGLISH:
        return "EN";
    case UI_TEXT_ID_PORTUGUESE_BR:
        return "PT-BR";
    case UI_TEXT_ID_TIMEOUT:
        return "TIMEOUT";
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
    case UI_TEXT_ID_REFERENCE_KIT_REQUIRED:
        return "REF KIT REQ";
    case UI_TEXT_ID_CONNECT_REF:
        return "CONNECT REF";
    case UI_TEXT_ID_OPEN_TERMINALS:
        return "OPEN TERM";
    case UI_TEXT_ID_SHORT_TERMINALS:
        return "SHORT TERM";
    case UI_TEXT_ID_OK_TO_START:
        return "OK START";
    case UI_TEXT_ID_CALIBRATING:
        return "CAL RUN";
    case UI_TEXT_ID_COMPLETE:
        return "DONE";
    case UI_TEXT_ID_SAVE_CALIBRATION:
        return "SAVE CAL";
    case UI_TEXT_ID_SAVING_CALIBRATION:
        return "SAVING";
    case UI_TEXT_ID_CALIBRATION_SAVED:
        return "CAL SAVED";
    case UI_TEXT_ID_CANCELING:
        return "CANCEL";
    case UI_TEXT_ID_CALIBRATION_FAILED:
        return "CAL FAILED";
    case UI_TEXT_ID_OK_RETRY_LONG_BACK:
        return "OK RETRY";
    case UI_TEXT_ID_SAFETY:
        return "SAFETY";
    case UI_TEXT_ID_PHASE:
        return "PHASE";
    case UI_TEXT_ID_GIT:
        return "GIT";
    case UI_TEXT_ID_CAL_SCHEMA:
        return "CAL SCHEMA";
    case UI_TEXT_ID_SEQUENCE:
        return "SEQUENCE";
    case UI_TEXT_ID_RANGE:
        return "RANGE";
    case UI_TEXT_ID_DIAGNOSTICS:
        return "DIAG";
    case UI_TEXT_ID_MAINTENANCE:
        return "SERVICE";
    case UI_TEXT_ID_PC_LINK:
        return "PC LINK";
    case UI_TEXT_ID_RESOURCES:
        return "RESOURCES";
    default:
        return "?";
    }
}
