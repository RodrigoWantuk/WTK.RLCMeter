#include "ui/ui_text.h"

const char *ui_text_fallback(ui_text_id_t id)
{
    switch (id)
    {
    case UI_TEXT_ID_WTK_RLCMETER:
        return "WTK RLCMETER";
    case UI_TEXT_ID_STARTING:
        return "STARTING";
    case UI_TEXT_ID_CAL_CHECK:
        return "CAL CHECK";
    case UI_TEXT_ID_CALIBRATION_REQUIRED:
        return "CALIBRATION REQUIRED";
    case UI_TEXT_ID_STORAGE_ERROR:
        return "STORAGE ERROR";
    case UI_TEXT_ID_READY:
        return "READY";
    case UI_TEXT_ID_MENU:
        return "MENU";
    case UI_TEXT_ID_CALIBRATION:
        return "CALIBRATION";
    case UI_TEXT_ID_DISPLAY:
        return "DISPLAY";
    case UI_TEXT_ID_SOUND:
        return "SOUND";
    case UI_TEXT_ID_ABOUT:
        return "ABOUT";
    case UI_TEXT_ID_BACK:
        return "BACK";
    case UI_TEXT_ID_BRIGHTNESS:
        return "BRIGHTNESS";
    case UI_TEXT_ID_BACKLIGHT_TIMEOUT:
        return "BACKLIGHT TIMEOUT";
    case UI_TEXT_ID_ON:
        return "ON";
    case UI_TEXT_ID_OFF:
        return "OFF";
    case UI_TEXT_ID_ACTIVE:
        return "ACTIVE";
    case UI_TEXT_ID_REQUIRED:
        return "REQUIRED";
    case UI_TEXT_ID_FULL_CALIBRATION:
        return "FULL CALIBRATION";
    case UI_TEXT_ID_SETTINGS_SAVE_FAILED:
        return "SETTINGS SAVE FAILED";
    case UI_TEXT_ID_MEASURING:
        return "MEASURING";
    case UI_TEXT_ID_DETAILS:
        return "DETAILS";
    default:
        return "?";
    }
}
