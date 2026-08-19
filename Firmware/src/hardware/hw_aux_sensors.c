#include "hardware/hw_aux_sensors.h"

#include <stddef.h>

typedef enum
{
    SENSOR_GROUP_NONE = 0,
    SENSOR_GROUP_RESIDUAL,
    SENSOR_GROUP_BATTERY,
    SENSOR_GROUP_NTC,
} sensor_group_t;

enum
{
    HW_AUX_SENSOR_FAULT_ADC_RUNTIME = 1u,
};

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (now_ms - deadline_ms) < 0x80000000u;
}

static uint16_t mean4(uint32_t sum)
{
    return (uint16_t)((sum + 2u) / HW_AUX_SENSOR_AVERAGE_COUNT);
}

static sensor_group_t active_group(const hw_aux_sensors_t *sensors)
{
    if (sensors == NULL)
    {
        return SENSOR_GROUP_NONE;
    }
    if (sensors->channel_phase < 3u)
    {
        return SENSOR_GROUP_RESIDUAL;
    }
    if (sensors->channel_phase == 3u)
    {
        return SENSOR_GROUP_BATTERY;
    }
    if (sensors->channel_phase == 4u)
    {
        return SENSOR_GROUP_NTC;
    }
    return SENSOR_GROUP_NONE;
}

static bsp_adc_channel_t phase_channel(uint8_t phase)
{
    switch (phase)
    {
    case 0u:
        return BSP_ADC_CHANNEL_VMID;
    case 1u:
        return BSP_ADC_CHANNEL_OV_HI;
    case 2u:
        return BSP_ADC_CHANNEL_OV_LO;
    case 3u:
        return BSP_ADC_CHANNEL_BAT;
    case 4u:
        return BSP_ADC_CHANNEL_NTC;
    default:
        return BSP_ADC_CHANNEL_INVALID;
    }
}

static void reset_group(hw_aux_sensors_t *sensors)
{
    sensors->accum_vmid = 0u;
    sensors->accum_hi = 0u;
    sensors->accum_lo = 0u;
    sensors->accum_battery = 0u;
    sensors->accum_ntc = 0u;
    sensors->sample_index = 0u;
    sensors->group_saturated = false;
}

static void begin_group(hw_aux_sensors_t *sensors, sensor_group_t group, uint32_t now_ms)
{
    reset_group(sensors);
    sensors->group_timestamp_ms = now_ms;
    sensors->state = HW_AUX_SENSORS_START;

    switch (group)
    {
    case SENSOR_GROUP_RESIDUAL:
        sensors->channel_phase = 0u;
        break;
    case SENSOR_GROUP_BATTERY:
        sensors->channel_phase = 3u;
        break;
    case SENSOR_GROUP_NTC:
        sensors->channel_phase = 4u;
        break;
    case SENSOR_GROUP_NONE:
    default:
        sensors->state = HW_AUX_SENSORS_IDLE;
        sensors->channel_phase = 255u;
        break;
    }
}

static void invalidate_active_group(hw_aux_sensors_t *sensors)
{
    sensors->fault_mask |= HW_AUX_SENSOR_FAULT_ADC_RUNTIME;
    sensors->state = sensors->paused ? HW_AUX_SENSORS_PAUSED : HW_AUX_SENSORS_IDLE;
    sensors->current_channel = BSP_ADC_CHANNEL_INVALID;
    sensors->channel_phase = 255u;
    reset_group(sensors);
}

static void publish_residual(hw_aux_sensors_t *sensors)
{
    const uint16_t vmid_raw = mean4(sensors->accum_vmid);
    const uint16_t hi_raw = mean4(sensors->accum_hi);
    const uint16_t lo_raw = mean4(sensors->accum_lo);
    const float vmid_v = bsp_adc_raw_to_voltage(vmid_raw);
    const float hi_v = bsp_adc_raw_to_voltage(hi_raw);
    const float lo_v = bsp_adc_raw_to_voltage(lo_raw);
    const bool vmid_saturated = hw_residual_raw_is_saturated(vmid_raw);
    const bool vmid_valid = !vmid_saturated && (vmid_v >= HW_AUX_VMID_MIN_V) && (vmid_v <= HW_AUX_VMID_MAX_V);

    sensors->snapshot.vmid_raw = vmid_raw;
    sensors->snapshot.vmid_v = vmid_v;
    sensors->snapshot.vmid_valid = vmid_valid;
    sensors->snapshot.ov_hi_raw = hi_raw;
    sensors->snapshot.ov_lo_raw = lo_raw;
    sensors->snapshot.safe_hi_v = hw_residual_terminal_voltage(vmid_v, hi_v);
    sensors->snapshot.safe_lo_v = hw_residual_terminal_voltage(vmid_v, lo_v);
    sensors->snapshot.residual_diff_v =
        hw_residual_differential_voltage(sensors->snapshot.safe_hi_v, sensors->snapshot.safe_lo_v);

    const hw_residual_policy_input_t input = {
        .valid = vmid_valid,
        .saturated = sensors->group_saturated,
        .safe_hi_v = sensors->snapshot.safe_hi_v,
        .safe_lo_v = sensors->snapshot.safe_lo_v,
        .residual_diff_v = sensors->snapshot.residual_diff_v,
    };
    sensors->snapshot.residual_state = hw_residual_policy_evaluate(&sensors->residual_policy, &input);
    sensors->snapshot.residual_safe_count = sensors->residual_policy.consecutive_safe_count;
    sensors->snapshot.timestamp_ms = sensors->group_timestamp_ms;
    sensors->residual_timestamp_ms = sensors->group_timestamp_ms;
    sensors->snapshot.residual_age_ms = 0u;
    sensors->residual_published = true;
    sensors->next_residual_due_ms = sensors->group_timestamp_ms + HW_AUX_RESIDUAL_SWEEP_PERIOD_MS;
}

static void publish_battery(hw_aux_sensors_t *sensors)
{
    const uint16_t raw = mean4(sensors->accum_battery);
    const float adc_v = bsp_adc_raw_to_voltage(raw);
    const float battery_v = hw_battery_vbat_from_adc_voltage(adc_v);

    sensors->snapshot.battery_raw = raw;
    sensors->snapshot.battery_v = battery_v;
    sensors->snapshot.battery_state = hw_battery_state_from_voltage(battery_v, true);
    sensors->snapshot.timestamp_ms = sensors->group_timestamp_ms;
    sensors->battery_timestamp_ms = sensors->group_timestamp_ms;
    sensors->snapshot.battery_age_ms = 0u;
    sensors->battery_published = true;
    sensors->next_battery_due_ms = sensors->group_timestamp_ms + HW_AUX_BATTERY_SWEEP_PERIOD_MS;
}

static void publish_ntc(hw_aux_sensors_t *sensors)
{
    const uint16_t raw = mean4(sensors->accum_ntc);
    const bool saturated = hw_residual_raw_is_saturated(raw);
    bool valid = false;
    const float adc_v = bsp_adc_raw_to_voltage(raw);
    const float resistance = saturated ? 0.0f : hw_ntc_resistance_from_voltage(adc_v, BSP_ADC_VDDA_NOMINAL_V, &valid);

    sensors->snapshot.ntc_raw = raw;
    sensors->snapshot.ntc_voltage_v = adc_v;
    sensors->snapshot.ntc_resistance_ohm = resistance;
    sensors->snapshot.ntc_valid = valid && !saturated;
    sensors->snapshot.timestamp_ms = sensors->group_timestamp_ms;
    sensors->ntc_timestamp_ms = sensors->group_timestamp_ms;
    sensors->snapshot.ntc_age_ms = 0u;
    sensors->ntc_published = true;
    sensors->next_ntc_due_ms = sensors->group_timestamp_ms + HW_AUX_NTC_SWEEP_PERIOD_MS;
}

static void complete_sample(hw_aux_sensors_t *sensors, uint16_t raw)
{
    switch (sensors->channel_phase)
    {
    case 0u:
        sensors->accum_vmid += raw;
        break;
    case 1u:
        sensors->accum_hi += raw;
        sensors->group_saturated = sensors->group_saturated || hw_residual_raw_is_saturated(raw);
        break;
    case 2u:
        sensors->accum_lo += raw;
        sensors->group_saturated = sensors->group_saturated || hw_residual_raw_is_saturated(raw);
        break;
    case 3u:
        sensors->accum_battery += raw;
        break;
    case 4u:
        sensors->accum_ntc += raw;
        break;
    default:
        break;
    }

    sensors->sample_index++;
    if (sensors->sample_index < HW_AUX_SENSOR_AVERAGE_COUNT)
    {
        sensors->state = HW_AUX_SENSORS_START;
        return;
    }

    sensors->sample_index = 0u;
    if (active_group(sensors) == SENSOR_GROUP_RESIDUAL)
    {
        if (sensors->channel_phase < 2u)
        {
            sensors->channel_phase++;
            sensors->state = HW_AUX_SENSORS_START;
            return;
        }
        publish_residual(sensors);
    }
    else if (active_group(sensors) == SENSOR_GROUP_BATTERY)
    {
        publish_battery(sensors);
    }
    else if (active_group(sensors) == SENSOR_GROUP_NTC)
    {
        publish_ntc(sensors);
    }

    sensors->current_channel = BSP_ADC_CHANNEL_INVALID;
    sensors->channel_phase = 255u;
    sensors->state = sensors->paused ? HW_AUX_SENSORS_PAUSED : HW_AUX_SENSORS_IDLE;
}

static sensor_group_t next_due_group(const hw_aux_sensors_t *sensors, uint32_t now_ms)
{
    if (deadline_reached(now_ms, sensors->next_residual_due_ms))
    {
        return SENSOR_GROUP_RESIDUAL;
    }
    if (deadline_reached(now_ms, sensors->next_battery_due_ms))
    {
        return SENSOR_GROUP_BATTERY;
    }
    if (deadline_reached(now_ms, sensors->next_ntc_due_ms))
    {
        return SENSOR_GROUP_NTC;
    }
    return SENSOR_GROUP_NONE;
}

static void invalidate_stale_publications(hw_aux_sensors_t *sensors, uint32_t now_ms)
{
    if (sensors->residual_published &&
        ((now_ms - sensors->residual_timestamp_ms) > HW_AUX_RESIDUAL_MAX_AGE_MS))
    {
        sensors->residual_published = false;
        sensors->snapshot.residual_state = HW_RESIDUAL_UNKNOWN;
        sensors->snapshot.residual_safe_count = 0u;
        hw_residual_policy_init(&sensors->residual_policy);
    }
    if (sensors->battery_published &&
        ((now_ms - sensors->battery_timestamp_ms) > HW_AUX_BATTERY_MAX_AGE_MS))
    {
        sensors->battery_published = false;
        sensors->snapshot.battery_state = HW_BATTERY_UNKNOWN;
    }
    if (sensors->ntc_published &&
        ((now_ms - sensors->ntc_timestamp_ms) > HW_AUX_NTC_MAX_AGE_MS))
    {
        sensors->ntc_published = false;
        sensors->snapshot.ntc_valid = false;
    }
}

bsp_status_t hw_aux_sensors_init(hw_aux_sensors_t *sensors, const hw_aux_adc_io_t *io, uint32_t now_ms)
{
    if ((sensors == NULL) || (io == NULL) || (io->start == NULL) || (io->poll == NULL) || (io->cancel == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    sensors->io = *io;
    hw_residual_policy_init(&sensors->residual_policy);
    sensors->snapshot = (hw_aux_sensors_snapshot_t){
        .residual_state = HW_RESIDUAL_UNKNOWN,
        .battery_state = HW_BATTERY_UNKNOWN,
    };
    sensors->state = HW_AUX_SENSORS_IDLE;
    sensors->current_channel = BSP_ADC_CHANNEL_INVALID;
    sensors->last_adc_status = BSP_STATUS_OK;
    sensors->fault_mask = 0u;
    sensors->next_residual_due_ms = now_ms;
    sensors->next_battery_due_ms = now_ms;
    sensors->next_ntc_due_ms = now_ms;
    sensors->group_timestamp_ms = now_ms;
    sensors->residual_timestamp_ms = 0u;
    sensors->battery_timestamp_ms = 0u;
    sensors->ntc_timestamp_ms = 0u;
    sensors->channel_phase = 255u;
    sensors->paused = false;
    sensors->residual_published = false;
    sensors->battery_published = false;
    sensors->ntc_published = false;
    reset_group(sensors);
    return BSP_STATUS_OK;
}

bsp_status_t hw_aux_sensors_step(hw_aux_sensors_t *sensors, uint32_t now_ms)
{
    if (sensors == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    if (sensors->paused)
    {
        if (sensors->state == HW_AUX_SENSORS_POLL)
        {
            sensors->io.cancel(sensors->io.user_data);
        }
        sensors->state = HW_AUX_SENSORS_PAUSED;
        sensors->current_channel = BSP_ADC_CHANNEL_INVALID;
        reset_group(sensors);
        return BSP_STATUS_OK;
    }

    invalidate_stale_publications(sensors, now_ms);

    if (sensors->state == HW_AUX_SENSORS_IDLE)
    {
        const sensor_group_t group = next_due_group(sensors, now_ms);
        if (group != SENSOR_GROUP_NONE)
        {
            begin_group(sensors, group, now_ms);
        }
        else
        {
            return BSP_STATUS_OK;
        }
    }

    if (sensors->state == HW_AUX_SENSORS_START)
    {
        const bsp_adc_channel_t channel = phase_channel(sensors->channel_phase);
        const bsp_status_t status = sensors->io.start(channel, now_ms, sensors->io.user_data);
        sensors->last_adc_status = status;
        if (status != BSP_STATUS_OK)
        {
            invalidate_active_group(sensors);
            return status;
        }
        sensors->current_channel = channel;
        sensors->state = HW_AUX_SENSORS_POLL;
        return BSP_STATUS_BUSY;
    }

    if (sensors->state == HW_AUX_SENSORS_POLL)
    {
        uint16_t raw = 0u;
        const bsp_status_t status = sensors->io.poll(&raw, now_ms, sensors->io.user_data);
        sensors->last_adc_status = status;
        if (status == BSP_STATUS_BUSY)
        {
            return BSP_STATUS_BUSY;
        }
        if (status != BSP_STATUS_OK)
        {
            sensors->io.cancel(sensors->io.user_data);
            invalidate_active_group(sensors);
            return status;
        }
        complete_sample(sensors, raw);
        return BSP_STATUS_OK;
    }

    return BSP_STATUS_OK;
}

void hw_aux_sensors_pause(hw_aux_sensors_t *sensors)
{
    if (sensors == NULL)
    {
        return;
    }

    sensors->paused = true;
}

void hw_aux_sensors_resume(hw_aux_sensors_t *sensors, uint32_t now_ms)
{
    if (sensors == NULL)
    {
        return;
    }

    sensors->paused = false;
    sensors->state = HW_AUX_SENSORS_IDLE;
    sensors->current_channel = BSP_ADC_CHANNEL_INVALID;
    sensors->next_residual_due_ms = now_ms;
    sensors->next_battery_due_ms = now_ms;
    sensors->next_ntc_due_ms = now_ms;
    reset_group(sensors);
}

bool hw_aux_sensors_is_idle(const hw_aux_sensors_t *sensors)
{
    return (sensors != NULL) &&
           ((sensors->state == HW_AUX_SENSORS_IDLE) || (sensors->state == HW_AUX_SENSORS_PAUSED));
}

void hw_aux_sensors_snapshot(const hw_aux_sensors_t *sensors,
                             uint32_t now_ms,
                             hw_aux_sensors_snapshot_t *snapshot)
{
    if ((sensors == NULL) || (snapshot == NULL))
    {
        return;
    }

    *snapshot = sensors->snapshot;
    snapshot->residual_age_ms = sensors->residual_published ? (now_ms - sensors->residual_timestamp_ms)
                                                            : HW_AUX_RESIDUAL_MAX_AGE_MS + 1u;
    snapshot->battery_age_ms = sensors->battery_published ? (now_ms - sensors->battery_timestamp_ms)
                                                          : HW_AUX_BATTERY_MAX_AGE_MS + 1u;
    snapshot->ntc_age_ms = sensors->ntc_published ? (now_ms - sensors->ntc_timestamp_ms)
                                                  : HW_AUX_NTC_MAX_AGE_MS + 1u;
}

hw_residual_state_t hw_aux_sensors_residual_state(const hw_aux_sensors_t *sensors, uint32_t now_ms)
{
    if ((sensors == NULL) || !sensors->residual_published ||
        ((now_ms - sensors->residual_timestamp_ms) > HW_AUX_RESIDUAL_MAX_AGE_MS))
    {
        return HW_RESIDUAL_UNKNOWN;
    }
    return sensors->snapshot.residual_state;
}

hw_battery_state_t hw_aux_sensors_battery_state(const hw_aux_sensors_t *sensors, uint32_t now_ms)
{
    if ((sensors == NULL) || !sensors->battery_published ||
        ((now_ms - sensors->battery_timestamp_ms) > HW_AUX_BATTERY_MAX_AGE_MS))
    {
        return HW_BATTERY_UNKNOWN;
    }
    return sensors->snapshot.battery_state;
}

bsp_adc_channel_t hw_aux_sensors_current_channel(const hw_aux_sensors_t *sensors)
{
    return (sensors == NULL) ? BSP_ADC_CHANNEL_INVALID : sensors->current_channel;
}

bsp_status_t hw_aux_sensors_last_adc_status(const hw_aux_sensors_t *sensors)
{
    return (sensors == NULL) ? BSP_STATUS_INVALID_ARG : sensors->last_adc_status;
}

uint32_t hw_aux_sensors_fault_mask(const hw_aux_sensors_t *sensors)
{
    return (sensors == NULL) ? 0u : sensors->fault_mask;
}

const char *hw_aux_sensors_state_string(hw_aux_sensors_state_t state)
{
    switch (state)
    {
    case HW_AUX_SENSORS_IDLE:
        return "IDLE";
    case HW_AUX_SENSORS_START:
    case HW_AUX_SENSORS_POLL:
        return "BUSY";
    case HW_AUX_SENSORS_PAUSED:
        return "PAUSED";
    default:
        return "UNKNOWN";
    }
}
