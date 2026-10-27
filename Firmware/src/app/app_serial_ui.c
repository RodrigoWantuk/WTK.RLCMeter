#include "app/app_serial_ui.h"
#include <math.h>
#include <string.h>

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0u; i < 4u; ++i) p[i] = (uint8_t)(value >> (8u * i));
}

static app_serial_ui_status_t request_status(app_serial_ui_t *control,
    const app_pc_link_frame_t *frame, bool telemetry, uint32_t *id)
{
    *id = frame->payload != NULL && frame->payload_length >= 8u ? get32(frame->payload + 4u) : 0u;
    const bool command_valid = telemetry ? frame->type == (app_pc_link_frame_type_t)APP_SERIAL_UI_TELEMETRY :
        (frame->type == (app_pc_link_frame_type_t)APP_SERIAL_UI_STATUS || frame->type == (app_pc_link_frame_type_t)APP_SERIAL_UI_OK_CLICK);
    if (frame->payload_length != APP_SERIAL_UI_REQUEST_BYTES || frame->payload == NULL ||
        frame->flags != 0u || frame->payload[0] != APP_SERIAL_UI_API || !command_valid ||
        frame->payload[1] != (uint8_t)frame->type || frame->payload[2] != 0u || frame->payload[3] != 0u ||
        *id == 0u || frame->sequence != (uint16_t)*id) return APP_SERIAL_UI_BAD_REQUEST;
    if (*id <= control->last_request) return APP_SERIAL_UI_STALE_REQUEST;
    control->last_request = *id;
    return APP_SERIAL_UI_ACCEPTED;
}

void app_serial_ui_handle(app_serial_ui_t *control, app_product_t *product,
    const app_pc_link_frame_t *frame, bool can_activate, uint32_t blocker_flags,
    uint32_t now_ms, uint8_t reply[APP_SERIAL_UI_RESPONSE_BYTES])
{
    if (control == NULL || product == NULL || frame == NULL || reply == NULL) return;
    uint32_t id = 0u;
    app_serial_ui_status_t status = request_status(control, frame, false, &id);
    if (status == APP_SERIAL_UI_ACCEPTED && frame->type == (app_pc_link_frame_type_t)APP_SERIAL_UI_OK_CLICK)
    {
        if (get32(frame->payload + 8u) != product->session_sequence)
            status = APP_SERIAL_UI_SESSION_MISMATCH;
        else if (!app_product_serial_idle(product) ||
            (product->view.state != UI_PRODUCT_STATE_READY && product->view.state != UI_PRODUCT_STATE_RESULT) ||
            product->ok_armed || product->request_click || product->request_menu ||
            product->measurement_deferred_for_settings || product->calibration_deferred_for_settings)
            status = APP_SERIAL_UI_NOT_READY;
        else if (!can_activate) status = APP_SERIAL_UI_SAFETY_BLOCKED;
        else
        {
            const bool waking = product->backlight_sleeping;
            button_event_t event = {.button = BUTTON_ID_OK, .type = BUTTON_EVENT_PRESS, .timestamp_ms = now_ms};
            app_product_handle_button_event(product, &event);
            event.type = BUTTON_EVENT_RELEASE;
            app_product_handle_button_event(product, &event);
            if (waking) status = APP_SERIAL_UI_WOKE_ONLY;
        }
    }
    reply[0] = APP_SERIAL_UI_API;
    reply[1] = (uint8_t)frame->type;
    reply[2] = (uint8_t)status;
    reply[3] = product->view.state;
    put32(reply + 4u, id);
    put32(reply + 8u, product->session_sequence);
    put32(reply + 12u, blocker_flags);
}

static void put_float(uint8_t *out, float value, bool valid)
{
    _Static_assert(sizeof(float) == sizeof(uint32_t), "protocol uses IEEE binary32");
    uint32_t bits = UINT32_C(0x7fc00000);
    if (valid && isfinite(value)) memcpy(&bits, &value, sizeof(bits));
    put32(out, bits);
}
static void put16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8u);
}
void app_serial_ui_telemetry(app_serial_ui_t *control, const app_product_t *product,
    const app_pc_link_frame_t *frame, const app_serial_telemetry_t *telemetry,
    uint8_t reply[APP_SERIAL_UI_TELEMETRY_BYTES])
{
    if (control == NULL || product == NULL || frame == NULL || telemetry == NULL ||
        telemetry->sensors == NULL || reply == NULL) return;
    const hw_aux_sensors_snapshot_t *s = telemetry->sensors;
    const ui_product_measurement_t *r = &product->view.measurement_result;
    const bool battery_valid = (s->battery_state == HW_BATTERY_OK || s->battery_state == HW_BATTERY_LOW ||
        s->battery_state == HW_BATTERY_CRITICAL) && isfinite(s->battery_v) &&
        s->battery_v >= 0.0f && s->battery_v <= HW_BATTERY_MAX_PLAUSIBLE_V && s->battery_age_ms <= HW_AUX_BATTERY_MAX_AGE_MS;
    const bool ntc_valid = s->ntc_temperature_valid && isfinite(s->ntc_temperature_c) &&
        s->ntc_temperature_c > -100.0f && s->ntc_temperature_c < 200.0f && s->ntc_age_ms <= HW_AUX_NTC_MAX_AGE_MS;
    const bool result_valid = product->view.has_measurement_result && !product->view.measurement_result_partial &&
        r->derived_valid && isfinite(r->resistance_ohms) && isfinite(r->reactance_ohms);
    const bool capacitance_valid = result_valid && r->capacitance_valid && isfinite(r->capacitance_f) && r->capacitance_f > 0.0f;
    const bool inductance_valid = result_valid && r->inductance_valid && isfinite(r->inductance_h) && r->inductance_h > 0.0f;
    uint32_t id = 0u;
    memset(reply, 0, APP_SERIAL_UI_TELEMETRY_BYTES);
    reply[0] = APP_SERIAL_UI_API;
    reply[1] = APP_SERIAL_UI_TELEMETRY;
    reply[2] = (uint8_t)request_status(control, frame, true, &id);
    reply[3] = product->view.state;
    put32(reply+4u, id);
    put32(reply+8u, product->session_sequence);
    put32(reply+12u, telemetry->now_ms);
    put32(reply+16u, telemetry->blocker_flags);
    put32(reply+20u, product->view.safety_fault_mask);
    put32(reply+24u, product->view.calibration_sequence);
    const uint32_t features = (WTK_ENABLE_SERIAL_UI ? 1u : 0u) | (WTK_ENABLE_MEASUREMENT_TRIMMED ? 2u : 0u) |
        (WTK_ENABLE_FREQUENCY_SURVEY ? 4u : 0u) | (WTK_CAL_FACTORY_LOAD_ONLY ? 8u : 0u) |
        (WTK_INTERNAL_FLASH_BYTES > 65536u ? 16u : 0u) | (WTK_NTC_FACTORY_CALIBRATION ? 32u : 0u);
    put32(reply+28u, features);
    put16(reply+32u, battery_valid ? hw_battery_telemetry_mv(s->battery_v, s->battery_state) : UINT16_MAX);
    reply[34] = (uint8_t)s->battery_state;
    reply[35] = (uint8_t)telemetry->charger;
    put32(reply+36u, s->battery_age_ms);
    put32(reply+40u, ntc_valid ? (uint32_t)(int32_t)(s->ntc_temperature_c * 1000.0f) : (uint32_t)INT32_MIN);
    put32(reply+44u, s->ntc_age_ms);
    put32(reply+48u, (battery_valid ? 1u : 0u) | (ntc_valid ? 2u : 0u) |
        (WTK_NTC_FACTORY_CALIBRATION ? 4u : 0u) | (product->view.calibration_active_valid ? 8u : 0u) |
        (result_valid ? 16u : 0u) | (telemetry->lead_active ? 32u : 0u) | (telemetry->uid != NULL ? 128u : 0u));
    put16(reply+52u, s->battery_raw);
    put16(reply+54u, s->ntc_raw);
    put32(reply+56u, telemetry->uart_errors);
    put32(reply+60u, telemetry->uart_overflows);
    static const char git[] = WTK_GIT_COMMIT;
    for (unsigned i = 0u; i < 12u && i < sizeof(git)-1u; ++i) reply[64u+i] = (uint8_t)git[i];
    if (telemetry->uid != NULL) memcpy(reply+76u, telemetry->uid, 12u);
    put_float(reply+88u, r->resistance_ohms, result_valid);
    put_float(reply+92u, r->reactance_ohms, result_valid);
    put_float(reply+96u, r->capacitance_f, capacitance_valid);
    put_float(reply+100u, r->inductance_h, inductance_valid);
    reply[104] = (uint8_t)((result_valid ? 1u : 0u) | (capacitance_valid ? 2u : 0u) |
                           (inductance_valid ? 4u : 0u));
    reply[105] = r->frequency;
    reply[106] = UINT8_MAX;
    reply[107] = UINT8_MAX;
#if WTK_PRODUCT_FACTORY_PROVISIONED
    reply[106] = r->range_id;
    reply[107] = r->return_channel;
#if WTK_ENABLE_MEASUREMENT_TRIMMED
    reply[121] = r->trimmed_applied ? 7u : 0u;
#if WTK_ENABLE_FREQUENCY_SURVEY
    reply[122] = r->frequency_attempted_mask;
#endif
#endif
#endif
    put32(reply+108u, telemetry->lead_sequence);
    reply[112] = (uint8_t)app_settings_service_status(product->settings_service);
    reply[113] = (uint8_t)app_settings_service_last_error(product->settings_service);
    reply[114] = telemetry->k1_commanded;
    reply[115] = telemetry->range_state;
    put32(reply+116u, telemetry->feedback_dropped);
    reply[120] = (uint8_t)telemetry->feedback_error;
    put16(reply+124u, (uint16_t)(WTK_INTERNAL_FLASH_BYTES / 1024u));
}
