#ifndef WTK_APP_CAL_CAPTURE_SERVICE_H
#define WTK_APP_CAL_CAPTURE_SERVICE_H

#include "app/app_calibration_session.h"
#include "app/app_pc_link_protocol.h"

enum
{
    APP_CAL_CAPTURE_API = 1u,
    APP_CAL_CAPTURE_IDENTIFY = 0x50u,
    APP_CAL_CAPTURE_STATUS = 0x51u,
    APP_CAL_CAPTURE_START = 0x52u,
    APP_CAL_CAPTURE_RESULT = 0x53u,
    APP_CAL_CAPTURE_CANCEL = 0x54u,
    APP_CAL_CAPTURE_RESPONSE = 0x80u,
    APP_CAL_CAPTURE_OBSERVATION_BYTES = 188u,
    APP_CAL_CAPTURE_CHUNK_BYTES = 96u,
    APP_CAL_CAPTURE_FRAME_BYTES = APP_PC_LINK_HEADER_SIZE + APP_PC_LINK_MAX_PAYLOAD_BYTES,
    APP_CAL_CAPTURE_RX_TIMEOUT_MS = 250u,
    APP_CAL_CAPTURE_TIMEOUT_MS = 20000u,
    APP_CAL_CAPTURE_ADC_NOMINAL_3V3 = 1u,
};

typedef enum
{
    APP_CAL_CAPTURE_OK = 0,
    APP_CAL_CAPTURE_BAD_FRAME,
    APP_CAL_CAPTURE_BAD_COMMAND,
    APP_CAL_CAPTURE_BAD_PAYLOAD,
    APP_CAL_CAPTURE_STALE_REQUEST,
    APP_CAL_CAPTURE_BUSY,
    APP_CAL_CAPTURE_UNSUPPORTED,
    APP_CAL_CAPTURE_SAFETY_BLOCKED,
    APP_CAL_CAPTURE_NOT_READY,
    APP_CAL_CAPTURE_CANCELED,
    APP_CAL_CAPTURE_TIMEOUT,
    APP_CAL_CAPTURE_ACQUISITION_ERROR,
} app_cal_capture_error_t;

typedef struct
{
    uint32_t safety_faults;
    uint32_t safety_blocks;
    int32_t temperature_mC;
    bool temperature_valid;
    bool factory_allowed; /* Preflight only: hardware still issues/validates its permit. */
    bool transfer_safe;   /* SAFE, excitation OFF, range disabled, quiet released. */
} app_cal_capture_snapshot_t;

typedef struct
{
    void (*snapshot)(app_cal_capture_snapshot_t *snapshot, void *user);
    bsp_status_t (*try_write_byte)(uint8_t byte, void *user);
    void *user;
    uint8_t device_uid[12];
    bool synthetic;
} app_cal_capture_io_t;

typedef struct
{
    app_calibration_session_t session;
    app_cal_capture_io_t io;
    const bsp_clock_summary_t *clock;
    bsp_status_t clock_status;
    uint8_t rx[APP_CAL_CAPTURE_FRAME_BYTES];
    uint8_t tx[2][APP_CAL_CAPTURE_FRAME_BYTES]; /* Two bounded replies allow emergency CANCEL. */
    uint8_t observation[APP_CAL_CAPTURE_OBSERVATION_BYTES];
    measurement_complex_t mean[6];
    uint16_t rx_size;
    uint16_t rx_expected;
    uint16_t tx_size[2];
    uint16_t tx_offset;
    uint8_t tx_head;
    uint8_t tx_count;
    uint8_t mean_count;
    uint32_t last_request;
    uint32_t capture_id;
    uint32_t rx_deadline;
    uint32_t capture_deadline;
    uint32_t protocol_errors;
    uint32_t clip_mask;
    uint32_t permit_issue_ms;
    uint32_t permit_validate_ms;
    app_cal_capture_error_t capture_status;
    bool result_valid;
    bool cancel_requested;
} app_cal_capture_service_t;

bsp_status_t app_cal_capture_init(app_cal_capture_service_t *service,
    app_calibration_service_t *calibration, const app_cal_session_io_t *capture_io,
    const app_cal_capture_io_t *io, const bsp_clock_summary_t *clock, bsp_status_t clock_status);
void app_cal_capture_receive_byte(app_cal_capture_service_t *service, uint8_t byte, uint32_t now_ms);
void app_cal_capture_step(app_cal_capture_service_t *service, uint32_t now_ms);

#endif
