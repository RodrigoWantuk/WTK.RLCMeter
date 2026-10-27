#include "app/app_serial_ui.h"

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0u; i < 4u; ++i) p[i] = (uint8_t)(value >> (8u * i));
}

void app_serial_ui_handle(app_serial_ui_t *control, app_product_t *product,
    const app_pc_link_frame_t *frame, bool can_activate, uint32_t blocker_flags,
    uint32_t now_ms, uint8_t reply[APP_SERIAL_UI_RESPONSE_BYTES])
{
    if (control == NULL || product == NULL || frame == NULL || reply == NULL) return;
    const uint32_t id = frame->payload != NULL && frame->payload_length >= 8u ? get32(frame->payload + 4u) : 0u;
    app_serial_ui_status_t status = APP_SERIAL_UI_BAD_REQUEST;
    if (frame->payload_length == APP_SERIAL_UI_REQUEST_BYTES && frame->payload != NULL &&
        frame->flags == 0u && frame->payload[0] == APP_SERIAL_UI_API &&
        frame->payload[1] == (uint8_t)frame->type && frame->payload[2] == 0u && frame->payload[3] == 0u &&
        (frame->type == (app_pc_link_frame_type_t)APP_SERIAL_UI_STATUS || frame->type == (app_pc_link_frame_type_t)APP_SERIAL_UI_OK_CLICK) &&
        id != 0u && frame->sequence == (uint16_t)id)
    {
        status = APP_SERIAL_UI_STALE_REQUEST;
        if (id > control->last_request)
        {
            control->last_request = id; /* Consume rejected activations too. No deferred retry. */
            status = APP_SERIAL_UI_ACCEPTED;
            if (frame->type == (app_pc_link_frame_type_t)APP_SERIAL_UI_OK_CLICK)
            {
                if (get32(frame->payload + 8u) != product->session_sequence)
                    status = APP_SERIAL_UI_SESSION_MISMATCH;
                else if (!product->initialized ||
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
