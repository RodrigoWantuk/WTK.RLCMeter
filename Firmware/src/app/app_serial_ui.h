#ifndef WTK_APP_SERIAL_UI_H
#define WTK_APP_SERIAL_UI_H

#include "app/app_pc_link_protocol.h"
#include "app/app_product.h"

enum
{
    APP_SERIAL_UI_STATUS = 0x30u,
    APP_SERIAL_UI_OK_CLICK = 0x31u,
    APP_SERIAL_UI_RESPONSE = 0xB0u,
    APP_SERIAL_UI_API = 1u,
    APP_SERIAL_UI_REQUEST_BYTES = 12u,
    APP_SERIAL_UI_RESPONSE_BYTES = 16u,
};

typedef enum
{
    APP_SERIAL_UI_ACCEPTED = 0,
    APP_SERIAL_UI_BAD_REQUEST,
    APP_SERIAL_UI_STALE_REQUEST,
    APP_SERIAL_UI_SESSION_MISMATCH,
    APP_SERIAL_UI_NOT_READY,
    APP_SERIAL_UI_SAFETY_BLOCKED,
    APP_SERIAL_UI_WOKE_ONLY,
} app_serial_ui_status_t;

typedef struct { uint32_t last_request; } app_serial_ui_t;

/* Caller decodes CRC/framing first. can_activate additionally requires actual
 * idle SAFE hardware, fresh power/residual state and no physical-button overlap.
 * A reply acknowledges a UI event, never a completed/qualified measurement. */
void app_serial_ui_handle(app_serial_ui_t *control, app_product_t *product,
    const app_pc_link_frame_t *frame, bool can_activate, uint32_t blocker_flags,
    uint32_t now_ms, uint8_t reply[APP_SERIAL_UI_RESPONSE_BYTES]);

#endif
