#ifndef WTK_APP_LEAD_CALIBRATION_H
#define WTK_APP_LEAD_CALIBRATION_H
#include "app/app_measurement_session.h"
#include "app/app_calibration_service.h"
#include "storage/lead_profile_store.h"

typedef enum {
    APP_LEAD_IDLE=0, APP_LEAD_WAIT_OPEN, APP_LEAD_START, APP_LEAD_WAIT,
    APP_LEAD_DRAIN, APP_LEAD_WAIT_SHORT, APP_LEAD_REVIEW, APP_LEAD_SAVING,
    APP_LEAD_COMPLETE, APP_LEAD_FAILED, APP_LEAD_CANCELING, APP_LEAD_CANCELED
} app_lead_state_t;
typedef struct {
    lead_profile_store_t store;
    app_calibration_service_t *factory;
    const app_measurement_session_io_t *io;
    measurement_lead_profile_t candidate;
    measurement_complex_t open_ratio[3];
    measurement_cal_key_t open_key[3];
    measurement_trimmed_t samples;
    uint8_t uid[12];
    uint32_t started_ms;
    app_lead_state_t state;
    bsp_status_t error;
    uint8_t frequency, open_range, attempts;
    bool short_stage, finish_failed, cancel_requested, range_retry;
} app_lead_calibration_t;
bsp_status_t app_lead_init(app_lead_calibration_t *lead, app_calibration_service_t *factory,
    const app_measurement_session_io_t *io, const storage_async_io_t *flash, uint32_t capacity, const uint8_t uid[12]);
const measurement_lead_profile_t *app_lead_active_profile(const app_lead_calibration_t *lead);
uint32_t app_lead_factory_crc(const app_lead_calibration_t *lead);
bool app_lead_busy(const app_lead_calibration_t *lead);
/* Begin/confirm never bypass the acquisition permit. No LOAD standard is requested. */
bsp_status_t app_lead_begin(app_lead_calibration_t *lead);
void app_lead_confirm(app_lead_calibration_t *lead, uint32_t now_ms);
void app_lead_cancel(app_lead_calibration_t *lead);
void app_lead_step(app_lead_calibration_t *lead,const bsp_clock_summary_t *clock,
    bsp_status_t clock_status,bool storage_allowed,uint32_t now_ms);
bsp_status_t app_lead_set_enabled(app_lead_calibration_t *lead,bool enabled,bool clear,uint32_t now_ms);
#endif
