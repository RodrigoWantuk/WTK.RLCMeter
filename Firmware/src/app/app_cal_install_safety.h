#ifndef WTK_APP_CAL_INSTALL_SAFETY_H
#define WTK_APP_CAL_INSTALL_SAFETY_H

#include "hardware/hw_aux_sensors.h"

/* Scheduling hint only. The writer still requires a fresh SAFE permission.
   Expiry resets the residual policy; its first seven safe sweeps remain
   UNKNOWN even though their sample ages are fresh. */
static inline bool app_cal_install_aux_refresh_pending(const hw_aux_sensors_snapshot_t *state)
{
    return state->residual_age_ms > HW_AUX_RESIDUAL_MAX_AGE_MS ||
        (state->residual_state == HW_RESIDUAL_UNKNOWN && state->vmid_valid &&
         state->residual_safe_count > 0u &&
         state->residual_safe_count < HW_RESIDUAL_REQUIRED_SAFE_COUNT);
}

#endif
