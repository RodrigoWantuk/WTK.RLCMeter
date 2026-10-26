#ifndef WTK_APP_PRODUCT_SCHEDULING_H
#define WTK_APP_PRODUCT_SCHEDULING_H

#include "hardware/hw_measure_permit.h"
#include "app/app_io_workspace.h"

/* A held capture retains the same exclusive workspace lease. This must not
 * turn the shared allocator into a generally reentrant acquire operation. */
static inline bsp_status_t app_product_measurement_workspace_prepare(app_io_workspace_t *workspace,
                                                                    bool continuing)
{
    if (continuing)
        return app_io_workspace_owner(workspace) == APP_IO_WORKSPACE_OWNER_METROLOGY ?
            BSP_STATUS_OK : BSP_STATUS_ERROR;
    return app_io_workspace_acquire(workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
}

/* Application scheduling only, never a relay permission. Disabled idle ranges
   are prepared by the hardware FSM. During its owned session, range transitions
   and invalidated residual ADC evidence are checked by that same FSM, rather
   than aborting it merely because it is acquiring/restoring the ADC. */
static inline bool app_product_schedule_allowed(const hw_safety_result_t *safety,
                                               bool range_disabled,
                                               bool residual_refresh_pending,
                                               bool measurement_active)
{
    if (safety->measure_allowed) return true;
    uint32_t expected = (range_disabled || measurement_active) ? HW_SAFETY_BLOCK_RANGE : 0u;
    if (measurement_active || residual_refresh_pending) expected |= HW_SAFETY_BLOCK_SENSOR_INVALID;
    return safety->blocker_flags != 0u && (safety->blocker_flags & ~expected) == 0u;
}

/* Wait for real refreshed evidence while K1 remains SAFE. The caller bounds
   this wait; hw_measure_permit_issue still decides permission afterward. */
static inline bool app_product_permit_refresh_pending(const hw_measure_permit_issue_input_t *input,
                                                      uint32_t now_ms, uint32_t deadline_ms)
{
    return (now_ms - deadline_ms) >= 0x80000000u &&
        input->charger == HW_CHARGER_ABSENT && input->safety_fault_mask == 0u &&
        input->k1_state == HW_K1_STATE_SAFE && input->range == HW_RANGE_READY &&
        (input->battery == HW_BATTERY_OK || input->battery == HW_BATTERY_LOW) &&
        input->battery_age_ms <= HW_MEASURE_PERMIT_BATTERY_MAX_AGE_MS &&
        (input->residual == HW_RESIDUAL_UNKNOWN ||
         (input->residual == HW_RESIDUAL_SAFE &&
          input->residual_age_ms > HW_MEASURE_PERMIT_RESIDUAL_MAX_AGE_MS));
}

#endif
