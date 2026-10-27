/* Synthetic snapshot serialization; this does not sample physical sensors. */
#include <assert.h>
#include <math.h>
#include <string.h>
#include "app/app_serial_ui.h"
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8u) | ((uint32_t)p[2]<<16u) | ((uint32_t)p[3]<<24u);
}
int main(void)
{
    app_product_t product = {0};
    product.view.state = UI_PRODUCT_STATE_RESULT;
    product.view.has_measurement_result = true;
    product.view.calibration_active_valid = true;
    product.view.measurement_result = (ui_product_measurement_t){.derived_valid = true,
        .resistance_ohms = 2.0f, .reactance_ohms = -16000.0f,
        .capacitance_f = 9.99e-9f, .capacitance_valid = true};
    hw_aux_sensors_snapshot_t sensors = {.battery_v = 3.942f, .battery_state = HW_BATTERY_OK,
        .battery_age_ms = 100u, .ntc_temperature_c = 23.5f, .ntc_temperature_valid = true, .ntc_age_ms = 120u};
    const uint8_t uid[12] = {1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u};
    app_serial_telemetry_t source = {.sensors = &sensors, .uid = uid, .now_ms = 12345u,
        .uart_errors = 2u, .uart_overflows = 3u, .blocker_flags = HW_SAFETY_BLOCK_RANGE};
    uint8_t payload[12] = {1u,APP_SERIAL_UI_TELEMETRY,0u,0u,1u};
    app_pc_link_frame_t frame = {.type = (app_pc_link_frame_type_t)APP_SERIAL_UI_TELEMETRY,
        .payload = payload, .payload_length = sizeof(payload), .sequence = 1u};
    app_serial_ui_t control = {0};
    uint8_t out[APP_SERIAL_UI_TELEMETRY_BYTES];
    app_serial_ui_telemetry(&control, &product, &frame, &source, out);
    assert(out[2] == APP_SERIAL_UI_ACCEPTED && !product.request_click);
    assert(get32(out+12u) == 12345u && get32(out+16u) == HW_SAFETY_BLOCK_RANGE);
    assert(((uint16_t)out[32] | ((uint16_t)out[33]<<8u)) == 3942u);
    assert(get32(out+40u) == 23500u && (get32(out+48u) & 19u) == 19u);
    assert(get32(out+56u) == 2u && get32(out+60u) == 3u && memcmp(out+76u,uid,12u) == 0);
    assert((out[104] & 3u) == 3u);
    app_serial_ui_telemetry(&control, &product, &frame, &source, out);
    assert(out[2] == APP_SERIAL_UI_STALE_REQUEST);
    payload[4] = 2u; frame.sequence = 2u;
    sensors.battery_age_ms = UINT32_MAX;
    sensors.ntc_temperature_c = NAN;
    product.view.measurement_result.resistance_ohms = NAN;
    app_serial_ui_telemetry(&control, &product, &frame, &source, out);
    assert(out[2] == APP_SERIAL_UI_ACCEPTED && (get32(out+48u) & 19u) == 0u);
    assert(out[32] == 255u && out[33] == 255u && get32(out+40u) == UINT32_C(0x80000000));
    assert(get32(out+88u) == UINT32_C(0x7fc00000) && out[104] == 0u);
    payload[1] = APP_SERIAL_UI_OK_CLICK;
    frame.type = (app_pc_link_frame_type_t)APP_SERIAL_UI_OK_CLICK;
    app_serial_ui_telemetry(&control, &product, &frame, &source, out);
    assert(out[2] == APP_SERIAL_UI_BAD_REQUEST && !product.request_click);
    product.initialized = true;
    product.view.state = UI_PRODUCT_STATE_STARTUP;
    assert(!app_product_serial_idle(&product));
    product.view.state = UI_PRODUCT_STATE_READY;
    assert(app_product_serial_idle(&product));
    product.settings_save_requested = true;
    assert(!app_product_serial_idle(&product));
    product.view.state = UI_PRODUCT_STATE_FAULT;
    assert(app_product_serial_idle(&product)); /* Diagnostics remain reachable after a fault. */
    return 0;
}
