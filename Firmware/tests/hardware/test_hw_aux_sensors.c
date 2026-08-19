#include "hardware/hw_aux_sensors.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct
{
    uint16_t vmid_raw;
    uint16_t hi_raw;
    uint16_t lo_raw;
    uint16_t bat_raw;
    uint16_t ntc_raw;
    uint16_t scripted_raw[8][HW_AUX_SENSOR_AVERAGE_COUNT];
    uint8_t scripted_count[8];
    uint8_t scripted_index[8];
    bsp_status_t poll_failure;
    uint8_t fail_poll_index;
    uint8_t poll_count;
    uint8_t start_count[8];
    bsp_adc_channel_t current_channel;
} fake_adc_t;

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static uint16_t raw_from_voltage(float voltage)
{
    const float raw = (voltage * (float)BSP_ADC_RAW_MAX) / BSP_ADC_VDDA_NOMINAL_V;
    return (uint16_t)(raw + 0.5f);
}

static uint16_t raw_for_terminal(float vmid_v, float terminal_v)
{
    const float sense_v = vmid_v + ((terminal_v - vmid_v) / HW_RESIDUAL_TRANSFER_K);
    return raw_from_voltage(sense_v);
}

static bsp_status_t fake_start(bsp_adc_channel_t channel, uint32_t now_ms, void *user_data)
{
    (void)now_ms;
    fake_adc_t *fake = (fake_adc_t *)user_data;
    uint8_t channel_number = 0u;
    if (bsp_adc_channel_number(channel, &channel_number) != BSP_STATUS_OK)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    fake->current_channel = channel;
    fake->start_count[channel_number]++;
    return BSP_STATUS_OK;
}

static bsp_status_t fake_poll(uint16_t *raw, uint32_t now_ms, void *user_data)
{
    (void)now_ms;
    fake_adc_t *fake = (fake_adc_t *)user_data;
    uint8_t channel_number = 0u;
    fake->poll_count++;
    if ((fake->fail_poll_index != 0u) && (fake->poll_count == fake->fail_poll_index))
    {
        return fake->poll_failure;
    }
    if (bsp_adc_channel_number(fake->current_channel, &channel_number) != BSP_STATUS_OK)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (fake->scripted_index[channel_number] < fake->scripted_count[channel_number])
    {
        *raw = fake->scripted_raw[channel_number][fake->scripted_index[channel_number]];
        fake->scripted_index[channel_number]++;
        return BSP_STATUS_OK;
    }

    switch (fake->current_channel)
    {
    case BSP_ADC_CHANNEL_VMID:
        *raw = fake->vmid_raw;
        break;
    case BSP_ADC_CHANNEL_OV_HI:
        *raw = fake->hi_raw;
        break;
    case BSP_ADC_CHANNEL_OV_LO:
        *raw = fake->lo_raw;
        break;
    case BSP_ADC_CHANNEL_BAT:
        *raw = fake->bat_raw;
        break;
    case BSP_ADC_CHANNEL_NTC:
        *raw = fake->ntc_raw;
        break;
    case BSP_ADC_CHANNEL_INVALID:
    default:
        return BSP_STATUS_INVALID_ARG;
    }
    return BSP_STATUS_OK;
}

static void script_channel(fake_adc_t *fake,
                           bsp_adc_channel_t channel,
                           uint16_t s0,
                           uint16_t s1,
                           uint16_t s2,
                           uint16_t s3)
{
    uint8_t channel_number = 0u;
    if ((fake == NULL) || (bsp_adc_channel_number(channel, &channel_number) != BSP_STATUS_OK))
    {
        return;
    }
    fake->scripted_raw[channel_number][0] = s0;
    fake->scripted_raw[channel_number][1] = s1;
    fake->scripted_raw[channel_number][2] = s2;
    fake->scripted_raw[channel_number][3] = s3;
    fake->scripted_count[channel_number] = HW_AUX_SENSOR_AVERAGE_COUNT;
    fake->scripted_index[channel_number] = 0u;
}

static void fake_cancel(void *user_data)
{
    fake_adc_t *fake = (fake_adc_t *)user_data;
    fake->current_channel = BSP_ADC_CHANNEL_INVALID;
}

static hw_aux_adc_io_t fake_io(fake_adc_t *fake)
{
    return (hw_aux_adc_io_t){
        .start = fake_start,
        .poll = fake_poll,
        .cancel = fake_cancel,
        .user_data = fake,
    };
}

static void run_conversions(hw_aux_sensors_t *sensors, uint32_t now_ms, uint8_t conversions)
{
    for (uint8_t i = 0u; i < conversions; i++)
    {
        (void)hw_aux_sensors_step(sensors, now_ms);
        (void)hw_aux_sensors_step(sensors, now_ms);
    }
}

static fake_adc_t safe_fake_adc(void)
{
    const float vmid_v = 1.65f;
    return (fake_adc_t){
        .vmid_raw = raw_from_voltage(vmid_v),
        .hi_raw = raw_for_terminal(vmid_v, 0.0f),
        .lo_raw = raw_for_terminal(vmid_v, 0.0f),
        .bat_raw = raw_from_voltage(1.80f),
        .ntc_raw = raw_from_voltage(1.65f),
        .poll_failure = BSP_STATUS_OK,
    };
}

static int test_residual_sequence_and_policy(void)
{
    int failures = 0;
    fake_adc_t fake = safe_fake_adc();
    hw_aux_sensors_t sensors;
    hw_aux_adc_io_t io = fake_io(&fake);
    (void)hw_aux_sensors_init(&sensors, &io, 0u);

    run_conversions(&sensors, 0u, 11u);
    failures += expect_true(hw_aux_sensors_residual_state(&sensors, 0u) == HW_RESIDUAL_UNKNOWN,
                            "residual not published before complete sweep");
    run_conversions(&sensors, 0u, 1u);
    failures += expect_true(fake.start_count[1] == 4u, "VMID sampled exactly four times");
    failures += expect_true(fake.start_count[4] == 4u, "OV_HI sampled exactly four times");
    failures += expect_true(fake.start_count[5] == 4u, "OV_LO sampled exactly four times");
    failures += expect_true(hw_aux_sensors_residual_state(&sensors, 0u) == HW_RESIDUAL_UNKNOWN,
                            "first safe sweep still requires hysteresis count");

    for (uint8_t sweep = 1u; sweep < HW_RESIDUAL_REQUIRED_SAFE_COUNT; sweep++)
    {
        run_conversions(&sensors, (uint32_t)(sweep * HW_AUX_RESIDUAL_SWEEP_PERIOD_MS), 12u);
    }
    failures += expect_true(hw_aux_sensors_residual_state(&sensors, 80u) == HW_RESIDUAL_SAFE,
                            "eight safe residual sweeps grant SAFE");
    failures += expect_true(hw_aux_sensors_residual_state(&sensors, 200u) == HW_RESIDUAL_UNKNOWN,
                            "stale residual becomes UNKNOWN");

    hw_aux_sensors_snapshot_t snapshot;
    hw_aux_sensors_snapshot(&sensors, 80u, &snapshot);
    failures += expect_true(snapshot.residual_safe_count == HW_RESIDUAL_REQUIRED_SAFE_COUNT,
                            "safe count reaches required count");
    failures += expect_true((snapshot.safe_hi_v > -0.10f) && (snapshot.safe_hi_v < 0.10f),
                            "residual conversion uses transfer ratio");
    hw_aux_sensors_snapshot(&sensors, 200u, &snapshot);
    failures += expect_true((snapshot.residual_state == HW_RESIDUAL_UNKNOWN) && (snapshot.residual_safe_count == 0u),
                            "stale residual snapshot normalizes state and count");

    return failures;
}

static int test_residual_invalid_and_saturated(void)
{
    int failures = 0;
    fake_adc_t fake = safe_fake_adc();
    hw_aux_sensors_t sensors;
    hw_aux_adc_io_t io = fake_io(&fake);

    fake.hi_raw = 4079u;
    (void)hw_aux_sensors_init(&sensors, &io, 0u);
    run_conversions(&sensors, 0u, 12u);
    failures += expect_true(hw_aux_sensors_residual_state(&sensors, 0u) == HW_RESIDUAL_SATURATED,
                            "one saturated OV sample marks residual saturated");

    fake = safe_fake_adc();
    script_channel(&fake, BSP_ADC_CHANNEL_VMID, 0u, 2731u, 2731u, 2731u);
    io = fake_io(&fake);
    (void)hw_aux_sensors_init(&sensors, &io, 0u);
    run_conversions(&sensors, 0u, 12u);
    failures += expect_true(hw_aux_sensors_residual_state(&sensors, 0u) == HW_RESIDUAL_SATURATED,
                            "one saturated VMID sample marks residual saturated");

    fake = safe_fake_adc();
    fake.vmid_raw = raw_from_voltage(1.20f);
    io = fake_io(&fake);
    (void)hw_aux_sensors_init(&sensors, &io, 0u);
    run_conversions(&sensors, 0u, 12u);
    failures += expect_true(hw_aux_sensors_residual_state(&sensors, 0u) == HW_RESIDUAL_UNKNOWN,
                            "VMID outside valid window marks residual unknown");

    return failures;
}

static int test_battery_and_ntc(void)
{
    int failures = 0;
    fake_adc_t fake = safe_fake_adc();
    hw_aux_sensors_t sensors;
    hw_aux_adc_io_t io = fake_io(&fake);
    (void)hw_aux_sensors_init(&sensors, &io, 0u);

    sensors.next_residual_due_ms = 1000u;
    sensors.next_battery_due_ms = 0u;
    sensors.next_ntc_due_ms = 1000u;
    fake.bat_raw = raw_from_voltage(1.80f);
    run_conversions(&sensors, 0u, 4u);
    failures += expect_true(hw_aux_sensors_battery_state(&sensors, 0u) == HW_BATTERY_OK,
                            "battery OK after four samples");
    failures += expect_true(hw_aux_sensors_battery_state(&sensors, 2500u) == HW_BATTERY_UNKNOWN,
                            "stale battery becomes UNKNOWN");
    hw_aux_sensors_snapshot_t snapshot;
    hw_aux_sensors_snapshot(&sensors, 2500u, &snapshot);
    failures += expect_true(snapshot.battery_state == HW_BATTERY_UNKNOWN,
                            "stale battery snapshot normalizes to UNKNOWN");

    sensors.next_battery_due_ms = 500u;
    sensors.next_residual_due_ms = 3000u;
    sensors.next_ntc_due_ms = 3000u;
    fake.bat_raw = raw_from_voltage(1.70f);
    run_conversions(&sensors, 500u, 4u);
    failures += expect_true(hw_aux_sensors_battery_state(&sensors, 500u) == HW_BATTERY_LOW,
                            "battery low threshold");

    sensors.next_battery_due_ms = 1000u;
    sensors.next_residual_due_ms = 3000u;
    sensors.next_ntc_due_ms = 3000u;
    fake.bat_raw = raw_from_voltage(1.60f);
    run_conversions(&sensors, 1000u, 4u);
    failures += expect_true(hw_aux_sensors_battery_state(&sensors, 1000u) == HW_BATTERY_CRITICAL,
                            "battery critical threshold");

    sensors.next_battery_due_ms = 1500u;
    sensors.next_residual_due_ms = 3000u;
    sensors.next_ntc_due_ms = 3000u;
    fake.bat_raw = raw_from_voltage(2.20f);
    run_conversions(&sensors, 1500u, 4u);
    failures += expect_true(hw_aux_sensors_battery_state(&sensors, 1500u) == HW_BATTERY_UNKNOWN,
                            "battery above 4.35 V is implausible");

    fake = safe_fake_adc();
    io = fake_io(&fake);
    (void)hw_aux_sensors_init(&sensors, &io, 0u);
    sensors.next_residual_due_ms = 3000u;
    sensors.next_battery_due_ms = 0u;
    sensors.next_ntc_due_ms = 3000u;
    script_channel(&fake, BSP_ADC_CHANNEL_BAT, 0u, 2979u, 2979u, 2979u);
    run_conversions(&sensors, 0u, 4u);
    failures += expect_true(hw_aux_sensors_battery_state(&sensors, 0u) == HW_BATTERY_UNKNOWN,
                            "one low-rail battery sample invalidates publication");

    fake = safe_fake_adc();
    io = fake_io(&fake);
    (void)hw_aux_sensors_init(&sensors, &io, 0u);
    sensors.next_residual_due_ms = 3000u;
    sensors.next_battery_due_ms = 0u;
    sensors.next_ntc_due_ms = 3000u;
    script_channel(&fake, BSP_ADC_CHANNEL_BAT, 4095u, 1614u, 1614u, 1614u);
    run_conversions(&sensors, 0u, 4u);
    failures += expect_true(hw_aux_sensors_battery_state(&sensors, 0u) == HW_BATTERY_UNKNOWN,
                            "one high-rail battery sample invalidates publication");

    sensors.next_residual_due_ms = 3000u;
    sensors.next_battery_due_ms = 3000u;
    sensors.next_ntc_due_ms = 1000u;
    fake.ntc_raw = raw_from_voltage(1.65f);
    run_conversions(&sensors, 1000u, 4u);
    hw_aux_sensors_snapshot(&sensors, 1000u, &snapshot);
    failures += expect_true(snapshot.ntc_valid && (snapshot.ntc_resistance_ohm > 99000.0f) &&
                                (snapshot.ntc_resistance_ohm < 101000.0f),
                            "NTC top-resistor equation returns about 100k");
    failures += expect_true(snapshot.ntc_temperature_valid && (snapshot.ntc_temperature_c > 24.9f) &&
                                (snapshot.ntc_temperature_c < 25.1f),
                            "NTC LUT returns about 25 C at 100k");

    fake.ntc_raw = 0u;
    sensors.next_ntc_due_ms = 2000u;
    run_conversions(&sensors, 2000u, 4u);
    hw_aux_sensors_snapshot(&sensors, 2000u, &snapshot);
    failures += expect_true(!snapshot.ntc_valid, "NTC rail/open/short guard invalidates telemetry");
    hw_aux_sensors_snapshot(&sensors, 8000u, &snapshot);
    failures += expect_true(snapshot.ntc_age_ms > HW_AUX_NTC_MAX_AGE_MS, "NTC stale age is exposed");
    failures += expect_true(!snapshot.ntc_valid && !snapshot.ntc_temperature_valid,
                            "stale NTC snapshot normalizes validity flags");

    return failures;
}

static int test_adc_error_and_pause(void)
{
    int failures = 0;
    fake_adc_t fake = safe_fake_adc();
    fake.fail_poll_index = 3u;
    fake.poll_failure = BSP_STATUS_TIMEOUT;
    hw_aux_sensors_t sensors;
    hw_aux_adc_io_t io = fake_io(&fake);
    (void)hw_aux_sensors_init(&sensors, &io, 0u);

    run_conversions(&sensors, 0u, 3u);
    failures += expect_true(hw_aux_sensors_fault_mask(&sensors) != 0u, "ADC error latches sensor fault");
    failures += expect_true(hw_aux_sensors_residual_state(&sensors, 0u) == HW_RESIDUAL_UNKNOWN,
                            "partial ADC group is discarded");

    fake = safe_fake_adc();
    io = fake_io(&fake);
    (void)hw_aux_sensors_init(&sensors, &io, 0u);
    for (uint8_t sweep = 0u; sweep < HW_RESIDUAL_REQUIRED_SAFE_COUNT; sweep++)
    {
        run_conversions(&sensors, (uint32_t)(sweep * HW_AUX_RESIDUAL_SWEEP_PERIOD_MS), 12u);
    }
    failures += expect_true(hw_aux_sensors_residual_state(&sensors, 80u) == HW_RESIDUAL_SAFE,
                            "residual SAFE is established before pause");
    hw_aux_sensors_pause(&sensors);
    failures += expect_true(hw_aux_sensors_residual_state(&sensors, 80u) == HW_RESIDUAL_UNKNOWN,
                            "pause immediately invalidates residual SAFE");
    failures += expect_true(hw_aux_sensors_is_idle(&sensors), "pause reaches idle/paused");
    hw_aux_sensors_resume(&sensors, 100u);
    failures += expect_true(hw_aux_sensors_is_idle(&sensors), "resume restarts from idle");
    run_conversions(&sensors, 100u, 12u);
    failures += expect_true(hw_aux_sensors_residual_state(&sensors, 100u) == HW_RESIDUAL_UNKNOWN,
                            "one post-resume safe sweep is not enough");
    for (uint8_t sweep = 1u; sweep < HW_RESIDUAL_REQUIRED_SAFE_COUNT; sweep++)
    {
        run_conversions(&sensors, (uint32_t)(100u + (sweep * HW_AUX_RESIDUAL_SWEEP_PERIOD_MS)), 12u);
    }
    failures += expect_true(hw_aux_sensors_residual_state(&sensors, 180u) == HW_RESIDUAL_SAFE,
                            "eight fresh post-resume sweeps grant SAFE");

    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_residual_sequence_and_policy();
    failures += test_residual_invalid_and_saturated();
    failures += test_battery_and_ntc();
    failures += test_adc_error_and_pause();
    return (failures == 0) ? 0 : 1;
}
