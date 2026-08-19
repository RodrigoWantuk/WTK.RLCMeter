#ifndef WTK_HW_AUX_SENSORS_H
#define WTK_HW_AUX_SENSORS_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_adc.h"
#include "bsp/bsp_status.h"
#include "hardware/hw_power.h"
#include "hardware/hw_residual.h"
#include "hardware/hw_safety.h"

enum
{
    HW_AUX_SENSOR_AVERAGE_COUNT = 4u,
    HW_AUX_RESIDUAL_SWEEP_PERIOD_MS = 10u,
    HW_AUX_BATTERY_SWEEP_PERIOD_MS = 500u,
    HW_AUX_NTC_SWEEP_PERIOD_MS = 1000u,
    HW_AUX_RESIDUAL_MAX_AGE_MS = 50u,
    HW_AUX_BATTERY_MAX_AGE_MS = 2000u,
    HW_AUX_NTC_MAX_AGE_MS = 5000u,
};

#define HW_AUX_VMID_MIN_V (1.350f)
#define HW_AUX_VMID_MAX_V (1.950f)

typedef bsp_status_t (*hw_aux_adc_start_fn)(bsp_adc_channel_t channel, uint32_t now_ms, void *user_data);
typedef bsp_status_t (*hw_aux_adc_poll_fn)(uint16_t *raw, uint32_t now_ms, void *user_data);
typedef void (*hw_aux_adc_cancel_fn)(void *user_data);

typedef struct
{
    hw_aux_adc_start_fn start;
    hw_aux_adc_poll_fn poll;
    hw_aux_adc_cancel_fn cancel;
    void *user_data;
} hw_aux_adc_io_t;

typedef enum
{
    HW_AUX_SENSORS_IDLE = 0,
    HW_AUX_SENSORS_START,
    HW_AUX_SENSORS_POLL,
    HW_AUX_SENSORS_PAUSED,
} hw_aux_sensors_state_t;

typedef struct
{
    uint32_t timestamp_ms;

    uint16_t vmid_raw;
    float vmid_v;
    bool vmid_valid;

    uint16_t ov_hi_raw;
    uint16_t ov_lo_raw;
    float safe_hi_v;
    float safe_lo_v;
    float residual_diff_v;
    hw_residual_state_t residual_state;
    uint8_t residual_safe_count;
    uint32_t residual_age_ms;

    uint16_t battery_raw;
    float battery_v;
    hw_battery_state_t battery_state;
    uint32_t battery_age_ms;

    uint16_t ntc_raw;
    float ntc_voltage_v;
    float ntc_resistance_ohm;
    bool ntc_valid;
    uint32_t ntc_age_ms;
} hw_aux_sensors_snapshot_t;

typedef struct
{
    hw_aux_adc_io_t io;
    hw_residual_policy_t residual_policy;
    hw_aux_sensors_snapshot_t snapshot;
    hw_aux_sensors_state_t state;
    bsp_adc_channel_t current_channel;
    bsp_status_t last_adc_status;
    uint32_t fault_mask;
    uint32_t next_residual_due_ms;
    uint32_t next_battery_due_ms;
    uint32_t next_ntc_due_ms;
    uint32_t group_timestamp_ms;
    uint32_t residual_timestamp_ms;
    uint32_t battery_timestamp_ms;
    uint32_t ntc_timestamp_ms;
    uint32_t accum_vmid;
    uint32_t accum_hi;
    uint32_t accum_lo;
    uint32_t accum_battery;
    uint32_t accum_ntc;
    uint8_t sample_index;
    uint8_t channel_phase;
    bool group_saturated;
    bool paused;
    bool residual_published;
    bool battery_published;
    bool ntc_published;
} hw_aux_sensors_t;

bsp_status_t hw_aux_sensors_init(hw_aux_sensors_t *sensors, const hw_aux_adc_io_t *io, uint32_t now_ms);
bsp_status_t hw_aux_sensors_step(hw_aux_sensors_t *sensors, uint32_t now_ms);
void hw_aux_sensors_pause(hw_aux_sensors_t *sensors);
void hw_aux_sensors_resume(hw_aux_sensors_t *sensors, uint32_t now_ms);
bool hw_aux_sensors_is_idle(const hw_aux_sensors_t *sensors);
void hw_aux_sensors_snapshot(const hw_aux_sensors_t *sensors,
                             uint32_t now_ms,
                             hw_aux_sensors_snapshot_t *snapshot);
hw_residual_state_t hw_aux_sensors_residual_state(const hw_aux_sensors_t *sensors, uint32_t now_ms);
hw_battery_state_t hw_aux_sensors_battery_state(const hw_aux_sensors_t *sensors, uint32_t now_ms);
bsp_adc_channel_t hw_aux_sensors_current_channel(const hw_aux_sensors_t *sensors);
bsp_status_t hw_aux_sensors_last_adc_status(const hw_aux_sensors_t *sensors);
uint32_t hw_aux_sensors_fault_mask(const hw_aux_sensors_t *sensors);
const char *hw_aux_sensors_state_string(hw_aux_sensors_state_t state);

#endif
