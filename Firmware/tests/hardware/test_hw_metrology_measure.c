#include "hardware/hw_metrology_measure.h"
#include "hardware/hw_aux_sensors.h"
#include "measurement/measurement_calibration.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    hw_k1_state_t k1;
    hw_range_id_t range_id;
    hw_safety_range_state_t range_state;
    bool range_ready;
    bool range_fail;
    bool quiet;
    bool aux_paused;
    bool aux_resumed;
    bool adc_fail;
    bool restore_fail;
    bool restore_transient;
    bool dma_complete;
    bool dma_error;
    bool capture_started;
    bool adc_stopped;
    hw_excitation_mode_t exc;
    bool exc_fail;
    bool exc_off_fail;
    bool exc_dma_error;
    bool k1_request_fail;
    bool k1_safe_fail;
    bool range_disable_fail;
    hw_charger_state_t charger;
    uint32_t fault_mask;
    uint32_t k1_request_calls;
    uint32_t permit_issue_calls;
    uint32_t permit_validate_calls;
    uint32_t restore_calls;
    uint32_t resume_calls;
    uint32_t resume_at_ms;
    uint32_t adc_faults;
    uint32_t k1_faults;
    uint32_t range_faults;
    uint32_t metrology_faults;
    bool permit_issue_ok;
    bool permit_validate_ok;
    hw_metrology_measure_state_t seen[96];
    uint32_t seen_count;
    hw_measure_permit_issue_input_t issue_input;
    hw_measure_permit_validate_input_t validate_input;
    hw_aux_sensors_t *sensors;
    hw_range_t *real_range;
    hw_k1_t *real_k1;
    bool range_enabled;
    bool address_while_enabled;
    bool address_while_driven;
    uint8_t address;
    bool freeze_sensors;
    bool off_while_range_enabled;
    uint32_t now_ms;
    bool synthetic_resistor;
    bool clip_hg;
    bool clip_1x;
    bool capture_timeout;
    uint32_t capture_complete_ms;
    hw_excitation_freq_t requested_frequency;
    hw_excitation_amp_t requested_amplitude;
#if WTK_ENABLE_CONTINUOUS_SURVEY
    bool timed_capture;
    uint8_t power_variant;
    uint32_t power_started_ms;
    uint32_t power_begin_calls;
    bool power_during_capture;
#endif
} fake_io_t;

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static bsp_clock_summary_t production_clock(void)
{
    bsp_clock_summary_t summary = {
        .source = BSP_CLOCK_SOURCE_HSE_PLL,
        .hse_ready = true,
        .sysclk_hz = 72000000u,
        .hclk_hz = 72000000u,
        .pclk1_hz = 36000000u,
        .pclk2_hz = 72000000u,
        .tim_apb1_hz = 72000000u,
        .tim_apb2_hz = 72000000u,
        .adc_hz = 12000000u,
        .systick_hz = 1000u,
    };
    return summary;
}

static void record_state(fake_io_t *fake, hw_metrology_measure_state_t state)
{
    if ((fake->seen_count < 96u) &&
        ((fake->seen_count == 0u) || (fake->seen[fake->seen_count - 1u] != state)))
    {
        fake->seen[fake->seen_count] = state;
        fake->seen_count++;
    }
}

static bsp_status_t fake_k1_safe(void *user)
{
    if (((fake_io_t *)user)->real_k1 != NULL) { return hw_k1_force_safe(((fake_io_t *)user)->real_k1); }

    fake_io_t *fake = (fake_io_t *)user;
    if (fake->k1_safe_fail)
    {
        return BSP_STATUS_ERROR;
    }
    fake->k1 = HW_K1_STATE_SAFE;
    return BSP_STATUS_OK;
}

static bsp_status_t fake_k1_request_measure(const hw_safety_result_t *permission, void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    fake->k1_request_calls++;
    if (fake->real_k1 != NULL) { return hw_k1_request_measure(fake->real_k1, permission); }
    if (fake->k1_request_fail || (permission == NULL) || !permission->measure_allowed)
    {
        return BSP_STATUS_ERROR;
    }
    fake->k1 = HW_K1_STATE_MEASURE;
    return BSP_STATUS_OK;
}

static hw_k1_state_t fake_k1_state(void *user)
{
    return ((fake_io_t *)user)->k1;
}

static bsp_status_t fake_range_request(hw_range_id_t id, uint32_t now_ms, void *user)
{
    if (((fake_io_t *)user)->real_range != NULL) { return hw_range_request(((fake_io_t *)user)->real_range, id, now_ms); }

    (void)now_ms;
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->range_fail)
    {
        return BSP_STATUS_ERROR;
    }
    fake->range_id = id;
    fake->range_ready = true;
    fake->range_state = HW_RANGE_READY;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_range_step(uint32_t now_ms, void *user)
{
    if (((fake_io_t *)user)->real_range != NULL) { return hw_range_step(((fake_io_t *)user)->real_range, now_ms); }

    (void)now_ms;
    (void)user;
    return BSP_STATUS_BUSY;
}

static bool fake_range_ready(void *user)
{
    if (((fake_io_t *)user)->real_range != NULL) { return hw_range_is_ready(((fake_io_t *)user)->real_range); }

    return ((fake_io_t *)user)->range_ready;
}

static hw_range_id_t fake_range_current_id(void *user)
{
    if (((fake_io_t *)user)->real_range != NULL) { return hw_range_get_current(((fake_io_t *)user)->real_range); }

    return ((fake_io_t *)user)->range_id;
}

static hw_safety_range_state_t fake_range_safety_state(void *user)
{
    if (((fake_io_t *)user)->real_range != NULL) { return hw_range_safety_state(((fake_io_t *)user)->real_range); }

    return ((fake_io_t *)user)->range_state;
}

static bsp_status_t fake_range_disable(void *user)
{
    if (((fake_io_t *)user)->real_range != NULL) { return hw_range_force_disabled(((fake_io_t *)user)->real_range); }

    fake_io_t *fake = (fake_io_t *)user;
    if (fake->range_disable_fail)
    {
        return BSP_STATUS_ERROR;
    }
    fake->range_ready = false;
    fake->range_state = HW_RANGE_DISABLED;
    return BSP_STATUS_OK;
}

static void fake_quiet(bool requested, void *user)
{
    ((fake_io_t *)user)->quiet = requested;
}

static void fake_aux_pause(void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->sensors != NULL)
    {
        hw_aux_sensors_pause(fake->sensors);
    }
    fake->aux_paused = true;
    fake->aux_resumed = false;
}

static void fake_aux_resume(uint32_t now_ms, void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->sensors != NULL)
    {
        hw_aux_sensors_resume(fake->sensors, now_ms);
    }
    fake->aux_paused = false;
    fake->aux_resumed = true;
    fake->resume_calls++;
    fake->resume_at_ms = now_ms;
}

static bsp_status_t fake_adc_acquire(uint32_t now_ms, void *user)
{
    (void)now_ms;
    return ((fake_io_t *)user)->adc_fail ? BSP_STATUS_ERROR : BSP_STATUS_OK;
}

static bsp_status_t fake_adc_start(uint32_t *raw_words,
                                   uint32_t word_count,
                                   const hw_metrology_adc_profile_t *profile,
                                   void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->synthetic_resistor)
    {
        if ((profile->sample_rate_hz != 640u) || (word_count != HW_METROLOGY_RAW_WORD_COUNT))
        {
            return BSP_STATUS_ERROR;
        }
        /* Independent divider: 1k DUT / (1k RREF + 1k DUT), ADC quantization only. */
        for (uint32_t n = 0u; n < HW_METROLOGY_SAMPLES_PER_BLOCK; n++)
        {
            const float wave = cosf((float)n * 0.09817477042f);
            const uint16_t source = (uint16_t)((1.65f + 0.141421356f * wave) * 4095.0f / 3.3f + 0.5f);
            uint16_t ret = (uint16_t)((1.65f + 0.070710678f * wave) * 4095.0f / 3.3f + 0.5f);
            uint16_t hg = (uint16_t)((1.65f + (1.0f + 68000.0f / 4700.0f) * 0.070710678f * wave) * 4095.0f / 3.3f + 0.5f);
            if (fake->clip_1x) { ret = (n % 2u == 0u) ? 0u : 4095u; }
            if (fake->clip_hg) { hg = (n % 2u == 0u) ? 0u : 4095u; }
            raw_words[3u * n] = hw_metrology_pack_word(source, ret);
            raw_words[3u * n + 1u] = hw_metrology_pack_word(source, hg);
            raw_words[3u * n + 2u] = hw_metrology_pack_word(2048u, 2048u);
        }
    }
    fake->capture_started = true;
#if WTK_ENABLE_CONTINUOUS_SURVEY
    if (fake->timed_capture)
    {
        for (uint32_t i = 0u; i < word_count; i++) raw_words[i] = hw_metrology_pack_word(2048u, 2048u);
        fake->dma_complete = false;
        fake->adc_stopped = false;
    }
#endif
    fake->capture_complete_ms = fake->now_ms + profile->capture_us / 1000u;
    if (!fake->synthetic_resistor && !fake->dma_error && !fake->capture_timeout
#if WTK_ENABLE_CONTINUOUS_SURVEY
        && !fake->timed_capture
#endif
       )
    {
        fake->dma_complete = true;
    }
    return BSP_STATUS_OK;
}

static void fake_adc_stop(void *user)
{
    ((fake_io_t *)user)->adc_stopped = true;
}

static bsp_status_t fake_adc_restore(uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_io_t *fake = (fake_io_t *)user;
    fake->restore_calls++;
    if (fake->restore_fail)
    {
        return BSP_STATUS_ERROR;
    }
    if (fake->restore_transient && (fake->restore_calls == 1u))
    {
        return BSP_STATUS_ERROR;
    }
    return BSP_STATUS_OK;
}

static bool fake_dma_complete(void *user)
{
    const fake_io_t *fake = (const fake_io_t *)user;
#if WTK_ENABLE_CONTINUOUS_SURVEY
    if (fake->timed_capture)
        return fake->capture_started && !fake->adc_stopped && !fake->capture_timeout && !fake->dma_error &&
            (fake->now_ms - fake->capture_complete_ms) < 0x80000000u;
#endif
    return fake->dma_complete || (fake->synthetic_resistor && fake->capture_started &&
                                  !fake->capture_timeout && !fake->dma_error &&
                                  fake->now_ms >= fake->capture_complete_ms);
}

static bool fake_dma_error(void *user)
{
    return ((fake_io_t *)user)->dma_error;
}

static bsp_status_t fake_exc_off(void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->exc_off_fail)
    {
        return BSP_STATUS_ERROR;
    }
    fake->off_while_range_enabled |= fake->range_enabled && fake->k1 != HW_K1_STATE_SAFE;
    fake->exc = HW_EXCITATION_MODE_OFF;
    return BSP_STATUS_OK;
}

static bsp_status_t fake_exc_neutral(void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->exc_fail)
    {
        return BSP_STATUS_ERROR;
    }
    fake->exc = HW_EXCITATION_MODE_NEUTRAL;
    return BSP_STATUS_OK;
}

static bsp_status_t fake_exc_sine(hw_excitation_freq_t frequency, hw_excitation_amp_t amplitude, void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->exc_fail || (fake->exc != HW_EXCITATION_MODE_NEUTRAL))
    {
        return BSP_STATUS_ERROR;
    }
    fake->requested_frequency = frequency;
    fake->requested_amplitude = amplitude;
    fake->exc = HW_EXCITATION_MODE_SINE;
    return BSP_STATUS_OK;
}

static bsp_status_t fake_exc_dc_pilot(void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->exc_fail || (fake->exc != HW_EXCITATION_MODE_NEUTRAL))
    {
        return BSP_STATUS_ERROR;
    }
    fake->exc = HW_EXCITATION_MODE_DC_PILOT;
    return BSP_STATUS_OK;
}

static hw_excitation_mode_t fake_exc_mode(void *user)
{
    return ((fake_io_t *)user)->exc;
}

static bool fake_exc_dma_error(void *user)
{
    return ((fake_io_t *)user)->exc_dma_error;
}

static hw_charger_state_t fake_charger(void *user)
{
    return ((fake_io_t *)user)->charger;
}

static uint32_t fake_fault_mask(void *user)
{
    const fake_io_t *fake = (const fake_io_t *)user;
    return fake->fault_mask | ((fake->sensors != NULL) ? hw_aux_sensors_fault_mask(fake->sensors) : 0u);
}

static bsp_status_t fake_permit_issue_input(hw_measure_permit_issue_input_t *input, void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    fake->permit_issue_calls++;
    if (input == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *input = fake->issue_input;
    if (fake->sensors != NULL)
    {
        hw_aux_sensors_snapshot_t snapshot;
        hw_aux_sensors_snapshot(fake->sensors, fake->now_ms, &snapshot);
        input->range = fake_range_safety_state(user);
        input->range_id = fake_range_current_id(user);
        input->k1_state = fake_k1_state(user);
        input->charger = fake->charger;
        input->residual = snapshot.residual_state;
        input->residual_age_ms = snapshot.residual_age_ms;
        input->battery = snapshot.battery_state;
        input->battery_age_ms = snapshot.battery_age_ms;
        input->safety_fault_mask = fake->fault_mask | hw_aux_sensors_fault_mask(fake->sensors);
    }
    return BSP_STATUS_OK;
}

static bsp_status_t fake_permit_validate_input(hw_measure_permit_validate_input_t *input, void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    fake->permit_validate_calls++;
    if (input == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *input = fake->validate_input;
    if (fake->sensors != NULL)
    {
        input->range = fake_range_safety_state(user);
        input->range_id = fake_range_current_id(user);
        input->k1_state = fake_k1_state(user);
        input->charger = fake->charger;
        input->safety_fault_mask = fake_fault_mask(user);
    }
    return BSP_STATUS_OK;
}

static void fake_latch_adc(void *user)
{
    ((fake_io_t *)user)->adc_faults++;
}

static void fake_latch_k1(void *user)
{
    ((fake_io_t *)user)->k1_faults++;
}

static void fake_latch_range(void *user)
{
    ((fake_io_t *)user)->range_faults++;
}

static void fake_latch_metrology(void *user)
{
    ((fake_io_t *)user)->metrology_faults++;
}

static void bind_default_permit_inputs(fake_io_t *fake, hw_range_id_t range_id)
{
    fake->issue_input = (hw_measure_permit_issue_input_t){
        .charger = HW_CHARGER_ABSENT,
        .residual = HW_RESIDUAL_SAFE,
        .residual_age_ms = 0u,
        .battery = HW_BATTERY_OK,
        .battery_age_ms = 0u,
        .range = HW_RANGE_READY,
        .range_id = range_id,
        .k1_state = HW_K1_STATE_SAFE,
        .safety_fault_mask = 0u,
    };
    fake->validate_input = (hw_measure_permit_validate_input_t){
        .charger = HW_CHARGER_ABSENT,
        .range = HW_RANGE_READY,
        .range_id = range_id,
        .k1_state = HW_K1_STATE_SAFE,
        .safety_fault_mask = 0u,
    };
}

#if WTK_ENABLE_CONTINUOUS_SURVEY
static bsp_status_t fake_power_begin(uint32_t now_ms, void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    fake->power_started_ms = now_ms;
    fake->power_begin_calls++;
    if (fake->capture_started && !fake->adc_stopped) fake->power_during_capture = true;
    if (fake->sensors != NULL) return hw_aux_sensors_power_begin(fake->sensors, now_ms);
    return BSP_STATUS_OK;
}

static bsp_status_t fake_power_status(hw_metrology_power_sample_t *sample, void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->sensors != NULL)
    {
        const bsp_status_t status = hw_aux_sensors_power_status(fake->sensors);
        if (status != BSP_STATUS_OK) return status;
        *sample = (hw_metrology_power_sample_t){.timestamp_ms = fake->sensors->battery_timestamp_ms,
            .battery = fake->sensors->snapshot.battery_state, .vmid_valid = fake->sensors->snapshot.vmid_valid};
        return BSP_STATUS_OK;
    }
    if (fake->power_variant == 8u || fake->now_ms - fake->power_started_ms < 2u) return BSP_STATUS_BUSY;
    if (fake->power_variant == 1u) return BSP_STATUS_ERROR;
    *sample = (hw_metrology_power_sample_t){.timestamp_ms = fake->power_started_ms,
        .battery = HW_BATTERY_OK, .vmid_valid = true};
    if (fake->power_variant == 2u) sample->battery = HW_BATTERY_CRITICAL;
    if (fake->power_variant == 3u) sample->battery = HW_BATTERY_UNKNOWN;
    if (fake->power_variant == 4u) sample->vmid_valid = false;
    if (fake->power_variant == 5u) sample->timestamp_ms = fake->now_ms - 1001u;
    if (fake->power_variant == 6u) sample->timestamp_ms = fake->now_ms + 1u;
    if (fake->power_variant == 7u) sample->timestamp_ms = fake->power_started_ms - 1u;
    return BSP_STATUS_OK;
}
#endif

static void bind_io(hw_metrology_measure_io_t *io, fake_io_t *fake)
{
    *io = (hw_metrology_measure_io_t){0};
    io->k1_force_safe = fake_k1_safe;
    io->k1_request_measure = fake_k1_request_measure;
    io->k1_commanded_state = fake_k1_state;
    io->range_request = fake_range_request;
    io->range_step = fake_range_step;
    io->range_is_ready = fake_range_ready;
    io->range_current_id = fake_range_current_id;
    io->range_safety_state = fake_range_safety_state;
    io->range_force_disabled = fake_range_disable;
    io->quiet_request = fake_quiet;
    io->aux_pause = fake_aux_pause;
    io->aux_resume = fake_aux_resume;
    io->adc_acquire = fake_adc_acquire;
    io->adc_start_capture = fake_adc_start;
    io->adc_stop = fake_adc_stop;
    io->adc_restore = fake_adc_restore;
#if WTK_ENABLE_CONTINUOUS_SURVEY
    io->power_begin = fake_power_begin;
    io->power_status = fake_power_status;
#endif
    io->adc_dma_complete = fake_dma_complete;
    io->adc_dma_error = fake_dma_error;
    io->excitation_off = fake_exc_off;
    io->excitation_neutral = fake_exc_neutral;
    io->excitation_sine = fake_exc_sine;
    io->excitation_dc_pilot = fake_exc_dc_pilot;
    io->excitation_mode = fake_exc_mode;
    io->excitation_dma_error = fake_exc_dma_error;
    io->charger_state = fake_charger;
    io->safety_fault_mask = fake_fault_mask;
    io->permit_issue_input = fake_permit_issue_input;
    io->permit_validate_input = fake_permit_validate_input;
    io->latch_k1_io_fault = fake_latch_k1;
    io->latch_range_io_fault = fake_latch_range;
    io->latch_adc_runtime_fault = fake_latch_adc;
    io->latch_metrology_runtime_fault = fake_latch_metrology;
    io->user = fake;
}

static int run_until_idle(hw_metrology_measure_t *measure, fake_io_t *fake, uint32_t *now_ms)
{
    for (uint32_t i = 0u; i < 12000u; i++)
    {
        record_state(fake, hw_metrology_measure_state(measure));
        fake->now_ms = *now_ms;
        if (fake->sensors != NULL && !fake->freeze_sensors)
        {
            for (uint8_t j = 0u; j < 8u; j++)
            {
                (void)hw_aux_sensors_step(fake->sensors, *now_ms);
            }
        }
        (void)hw_metrology_measure_step(measure, *now_ms);
        *now_ms += 1u;
        if ((hw_metrology_measure_state(measure) == HW_METROLOGY_MEASURE_DONE) ||
            (hw_metrology_measure_state(measure) == HW_METROLOGY_MEASURE_IDLE))
        {
            return 0;
        }
    }
    return 1;
}

static bool saw_state(const fake_io_t *fake, hw_metrology_measure_state_t state)
{
    for (uint32_t i = 0u; i < fake->seen_count; i++)
    {
        if (fake->seen[i] == state)
        {
            return true;
        }
    }
    return false;
}

static hw_metrology_measure_request_t make_request(const bsp_clock_summary_t *clock,
                                                   hw_range_id_t range,
                                                   hw_excitation_amp_t amp)
{
    return (hw_metrology_measure_request_t){
        .clock_summary = clock,
        .clock_init_status = BSP_STATUS_OK,
        .frequency = HW_EXCITATION_FREQ_1KHZ,
        .amplitude = amp,
        .range_id = range,
    };
}

/* Real auxiliary conversion/policy and metrology/DSP composition. Only peripheral
 * observations are injected; no calibration set or READY snapshot is fabricated. */
typedef struct
{
    bsp_adc_channel_t channel;
    bool grounded_terminals;
    bool invalid_vmid;
    bool invalid_ntc;
    bool fail_adc;
} preflight_adc_t;

static uint16_t preflight_code(float voltage)
{
    return (uint16_t)(voltage * 4095.0f / 3.3f + 0.5f);
}

static bsp_status_t preflight_adc_start(bsp_adc_channel_t channel, uint32_t now_ms, void *user)
{
    (void)now_ms;
    ((preflight_adc_t *)user)->channel = channel;
    return BSP_STATUS_OK;
}

static bsp_status_t preflight_adc_poll(uint16_t *raw, uint32_t now_ms, void *user)
{
    (void)now_ms;
    const preflight_adc_t *adc = (const preflight_adc_t *)user;
    if (adc->fail_adc) { return BSP_STATUS_ERROR; }
    switch (adc->channel)
    {
    case BSP_ADC_CHANNEL_VMID:
        *raw = adc->invalid_vmid ? 0u : preflight_code(1.65f);
        break;
    case BSP_ADC_CHANNEL_OV_HI:
    case BSP_ADC_CHANNEL_OV_LO:
        /* Rev.1 floating passive terminals naturally bias to VMID.
         * A separate grounded-terminal observation tests common-mode rejection. */
        *raw = preflight_code(adc->grounded_terminals ?
                             1.65f - 1.65f / HW_RESIDUAL_TRANSFER_K : 1.65f);
        break;
    case BSP_ADC_CHANNEL_BAT:
        /* 5 V -> 10k -> VBAT -> (33k || board 200k) -> GND. */
        *raw = preflight_code(3.6954087f / 2.0f);
        break;
    case BSP_ADC_CHANNEL_NTC:
        *raw = adc->invalid_ntc ? 0u : preflight_code(1.65f);
        break;
    default: return BSP_STATUS_INVALID_ARG;
    }
    return BSP_STATUS_OK;
}

static void preflight_adc_cancel(void *user)
{
    ((preflight_adc_t *)user)->channel = BSP_ADC_CHANNEL_INVALID;
}

static bsp_status_t preflight_range_enable(bool high, void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    fake->range_enabled = high;
    fake->range_ready = high;
    return BSP_STATUS_OK;
}

static bsp_status_t preflight_range_address(uint8_t address, void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    fake->address_while_enabled |= fake->range_enabled;
    fake->address_while_driven |= fake->exc == HW_EXCITATION_MODE_SINE;
    fake->address = address;
    return BSP_STATUS_OK;
}

static bsp_status_t preflight_k1_write(bool high, void *user)
{
    if (high && ((fake_io_t *)user)->k1_request_fail) { return BSP_STATUS_ERROR; }
    ((fake_io_t *)user)->k1 = high ? HW_K1_STATE_MEASURE : HW_K1_STATE_SAFE;
    return BSP_STATUS_OK;
}

static int test_first_measurement_preflight(void)
{
    int failures = 0;
    /* baseline, HG clipped, both paths clipped, floating passive fixture,
     * invalid VMID, charger, conversion fault, DMA timeout, API cancellation,
     * invalid NTC (metadata only), unknown charger, stale residual evidence,
     * cancellation after K1 command but before auxiliary pause. */
    for (uint32_t scenario = 0u; scenario < 16u; scenario++)
    {
        preflight_adc_t adc = {0};
        adc.grounded_terminals = scenario == 3u;
        adc.invalid_vmid = scenario == 4u;
        adc.fail_adc = scenario == 6u;
        adc.invalid_ntc = scenario == 9u;
        const hw_aux_adc_io_t aux_io = {preflight_adc_start, preflight_adc_poll,
                                       preflight_adc_cancel, &adc};
        hw_aux_sensors_t sensors;
        failures += expect_true(hw_aux_sensors_init(&sensors, &aux_io, 0u) == BSP_STATUS_OK,
                                "preflight POWER_ON sensor initialization");
        uint32_t now = 0u;
        for (; now < 200u; now++)
        {
            for (uint8_t j = 0u; j < 8u; j++) { (void)hw_aux_sensors_step(&sensors, now); }
        }
        hw_aux_sensors_snapshot_t snapshot;
        hw_aux_sensors_snapshot(&sensors, now, &snapshot);
        if (!adc.fail_adc)
        {
            failures += expect_true(snapshot.battery_state == HW_BATTERY_OK &&
                                    fabsf(snapshot.battery_v - 3.6954087f) < 0.003f,
                                    "bench fixture produces legitimate battery OK");
            failures += expect_true(snapshot.ntc_valid != adc.invalid_ntc,
                                    "nominal/invalid NTC correctly represented");
        }
        fake_io_t fake = {0};
        fake.k1 = HW_K1_STATE_SAFE;
        fake.exc = HW_EXCITATION_MODE_OFF;
        fake.charger = (scenario == 5u) ? HW_CHARGER_PRESENT :
                       ((scenario == 10u) ? HW_CHARGER_UNKNOWN : HW_CHARGER_ABSENT);
        bind_default_permit_inputs(&fake, HW_RANGE_ID_1K);
        fake.sensors = &sensors;
        hw_range_t range;
        hw_k1_t relay;
        const hw_range_io_t range_io = {preflight_range_enable, preflight_range_address, &fake};
        const hw_k1_io_t relay_io = {preflight_k1_write, &fake};
        failures += expect_true(hw_range_init(&range, &range_io) == BSP_STATUS_OK &&
                                hw_k1_init(&relay, &relay_io) == BSP_STATUS_OK,
                                "safe range and relay boot");
        fake.real_range = &range;
        fake.real_k1 = &relay;
        fake.synthetic_resistor = true;
        fake.clip_hg = scenario == 1u || scenario == 2u;
        fake.clip_1x = scenario == 2u;
        fake.capture_timeout = scenario == 7u;
        fake.k1_request_fail = scenario == 13u;
        fake.adc_fail = scenario == 14u;
        if (scenario == 11u)
        {
            /* Stop peripheral observations, age existing evidence without inventing a snapshot. */
            fake.freeze_sensors = true;
            now += 25u;
        }
        hw_metrology_measure_io_t io = {0};
        bind_io(&io, &fake);
        uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
        hw_metrology_measure_t measure;
        failures += expect_true(hw_metrology_measure_init(&measure, &io, raw,
                                HW_METROLOGY_RAW_WORD_COUNT) == BSP_STATUS_OK, "preflight init");
        const bsp_clock_summary_t clock = production_clock();
        hw_metrology_measure_request_t request = make_request(&clock, HW_RANGE_ID_1K,
                                                               HW_EXCITATION_AMP_100MVRMS);
        request.frequency = HW_EXCITATION_FREQ_10HZ;
        failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_BUSY,
                                "10 Hz / 100 mVrms / 1k request accepted");
        uint32_t canceled_at_ms = 0u;
        if (scenario == 8u || scenario == 12u || scenario == 15u)
        {
            for (uint32_t step = 0u; step < 1400u; step++)
            {
                fake.now_ms = now;
                for (uint8_t j = 0u; j < 8u; j++) { (void)hw_aux_sensors_step(&sensors, now); }
                (void)hw_metrology_measure_step(&measure, now++);
                if (scenario == 12u ? fake.k1 == HW_K1_STATE_MEASURE : fake.capture_started) { break; }
            }
            failures += expect_true(scenario == 12u ? !fake.aux_paused : fake.capture_started,
                                    "cancel during acquisition or before auxiliary pause");
            canceled_at_ms = now;
            if (scenario == 15u) { fake.fault_mask = 1u; }
            else { (void)hw_metrology_measure_abort(&measure); }
        }
        failures += expect_true(run_until_idle(&measure, &fake, &now) == 0, "preflight terminates");
        failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF &&
                                !fake.range_ready && !fake.quiet && !fake.aux_paused,
                                "all paths return K1 SAFE / excitation OFF / range OFF / ADC restored");
        failures += expect_true(!fake.off_while_range_enabled, "abort isolates RREF before ground-going OFF");
        failures += expect_true(!fake.address_while_enabled && fake.address == 2u,
                                "1k address selected only while RANGE_EN disabled");
        if (scenario == 8u || scenario == 12u)
        {
            failures += expect_true(now >= canceled_at_ms + HW_METROLOGY_MEASURE_K1_RELEASE_GUARD_MS,
                                    "cancel respects release guard even before auxiliary pause");
        }
        if (scenario == 13u)
        {
            failures += expect_true(fake.k1_faults > 0u, "actual K1 callback failure latches fault");
        }
        if (scenario == 7u)
        {
            failures += expect_true(hw_metrology_measure_error(&measure) == HW_METROLOGY_MEASURE_ERR_TIMEOUT,
                                    "DMA timeout is explicit");
        }
        const bool captured = scenario < 3u || scenario == 9u;
        if (!captured)
        {
            failures += expect_true(!hw_metrology_measure_dumpable(&measure), "failure cannot publish result");
            if (scenario != 7u && scenario != 8u && scenario != 15u)
            {
                failures += expect_true(!fake.capture_started, "sensor/interlock block prevents acquisition");
            }
            if (scenario == 3u)
            {
                failures += expect_true(snapshot.residual_state == HW_RESIDUAL_UNSAFE &&
                                        fake.k1_request_calls == 0u,
                                        "grounded terminal common-mode departure remains BLOCKED, no bypass");
            }
            continue;
        }
        failures += expect_true(saw_state(&fake, HW_METROLOGY_MEASURE_PERMIT_VALIDATE) &&
                                saw_state(&fake, HW_METROLOGY_MEASURE_K1_REQUEST) &&
                                saw_state(&fake, HW_METROLOGY_MEASURE_ADC_DMA_WAIT) &&
                                saw_state(&fake, HW_METROLOGY_MEASURE_K1_RELEASE_GUARD),
                                "production authorization/acquisition/release transitions reached");
        failures += expect_true(hw_metrology_measure_dumpable(&measure),
                                "real sensor evidence survives authorization before aux pause");
        const hw_metrology_block_t *block = hw_metrology_measure_block(&measure);
        if (block == NULL || !block->valid) { continue; }
        failures += expect_true(block->excitation_frequency_hz == 10u &&
                                block->requested_amplitude_mvrms == 100u && block->range_id == HW_RANGE_ID_1K &&
                                block->sample_rate_hz == 640u && fake.adc_stopped &&
                                fake.requested_frequency == request.frequency &&
                                fake.requested_amplitude == request.amplitude, "actual condition and cleanup provenance");
        const measurement_cal_key_t key = {MEASUREMENT_CAL_HARDWARE_REV1, MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                         HW_RANGE_ID_1K, request.frequency, request.amplitude};
        measurement_calibrated_result_t result;
        const bsp_status_t status = measurement_cal_process_block(block, NULL, &key, true, &result);
        if (scenario == 2u)
        {
            failures += expect_true(status != BSP_STATUS_OK, "both clipped return paths rejected");
            continue;
        }
        failures += expect_true(status == BSP_STATUS_OK && result.result.status == MEASUREMENT_STATUS_OK &&
                                isfinite(result.result.impedance.z_ohms.re) &&
                                fabsf(result.result.impedance.z_ohms.re - 1000.0f) < 10.0f &&
                                fabsf(result.result.impedance.z_ohms.im) < 5.0f &&
                                result.result.derived.interpretation == MEASUREMENT_INTERPRET_RESISTIVE,
                                "synthetic nominal resistor passes production DSP");
        failures += expect_true(result.provenance.source == MEASUREMENT_CAL_SOURCE_IDEAL &&
                                result.provenance.status == MEASUREMENT_CAL_RESOLVE_MISSING &&
                                result.provenance.uncalibrated, "blank diagnostic result is explicitly ideal/unqualified");
        failures += expect_true(result.result.selected_channel ==
                                (scenario == 1u ? MEASUREMENT_RETURN_1X : MEASUREMENT_RETURN_HG),
                                "clipped HG falls back to valid 1X");
        (void)printf("SYNTHETIC preflight %u: R=%.3f X=%.3f source=IDEAL calibration=MISSING path=%s\n",
                     (unsigned)scenario, (double)result.result.impedance.z_ohms.re,
                     (double)result.result.impedance.z_ohms.im, scenario == 1u ? "RET_1X" : "RET_HG");
        failures += expect_true(measurement_cal_process_block(block, NULL, &key, false, &result) != BSP_STATUS_OK,
                                "ordinary no-fallback path rejects missing calibration");
    }
    return failures;
}

#if WTK_ENABLE_BENCH_SUPPLY
static int test_external_bench_supply(void)
{
    int failures = 0;
    for (uint32_t scenario = 0u; scenario < 6u; scenario++)
    {
        uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT] = {0};
        fake_io_t fake = {0};
        fake.k1 = HW_K1_STATE_SAFE;
        fake.exc = HW_EXCITATION_MODE_OFF;
        fake.range_id = HW_RANGE_ID_10K;
        fake.range_state = HW_RANGE_READY;
        bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
        fake.issue_input.battery = HW_BATTERY_UNKNOWN;
        fake.issue_input.battery_age_ms = UINT32_MAX;
        fake.issue_input.bench_supply_confirmed = scenario != 1u;
        fake.validate_input.bench_supply_confirmed = scenario != 1u;
        if (scenario == 5u) { fake.issue_input.residual = HW_RESIDUAL_UNSAFE; }
        bsp_clock_summary_t clock = production_clock();
        hw_metrology_measure_io_t io;
        bind_io(&io, &fake);
        hw_metrology_measure_t measure;
        (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
        hw_metrology_measure_request_t request = make_request(&clock, HW_RANGE_ID_10K,
            scenario == 2u ? HW_EXCITATION_AMP_500MVRMS : HW_EXCITATION_AMP_100MVRMS);
        uint32_t now = 0u;
        (void)hw_metrology_measure_start(&measure, &request, now);
        if (scenario == 3u || scenario == 4u)
        {
            for (; now < 2000u; now++)
            {
                (void)hw_metrology_measure_step(&measure, now);
                if (hw_metrology_measure_state(&measure) == HW_METROLOGY_MEASURE_ADC_DMA_WAIT) { break; }
            }
            failures += expect_true(now < 2000u && fake.k1 == HW_K1_STATE_MEASURE, "bench reaches active capture before fault");
            if (scenario == 3u) { fake.validate_input.bench_supply_confirmed = false; }
            else { fake.charger = HW_CHARGER_PRESENT; }
        }
        failures += expect_true(run_until_idle(&measure, &fake, &now) == 0, "bench transaction finishes bounded cleanup");
        failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF && !fake.quiet,
                                "bench returns K1/excitation/quiet to safe state");
        failures += expect_true(hw_metrology_measure_dumpable(&measure) == (scenario == 0u),
                                "only confirmed safe 100mV bench capture publishes data");
        if (scenario == 1u || scenario == 2u || scenario == 5u)
        {
            failures += expect_true(fake.k1_request_calls == 0u, "rejected bench capture never energizes K1");
        }
        if (scenario == 2u)
        {
            failures += expect_true(hw_metrology_measure_error(&measure) == HW_METROLOGY_MEASURE_ERR_FORBIDDEN_AMPLITUDE,
                                    "bench rejects 500mV before relay operation");
        }
    }
    return failures;
}
#endif

static int test_30ms_relay_guard(void)
{
    int failures = 0;
    uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT] = {0};
    fake_io_t fake = {0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_id = HW_RANGE_ID_10K;
    fake.range_state = HW_RANGE_READY;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
    bsp_clock_summary_t clock = production_clock();
    hw_metrology_measure_io_t io;
    bind_io(&io, &fake);
    hw_metrology_measure_t measure;
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    const hw_metrology_measure_request_t request = make_request(&clock, HW_RANGE_ID_10K, HW_EXCITATION_AMP_100MVRMS);
    uint32_t now = UINT32_MAX - 20u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    for (uint32_t step = 0u; step < 100u; step++, now++)
    {
        (void)hw_metrology_measure_step(&measure, now);
        if (hw_metrology_measure_state(&measure) == HW_METROLOGY_MEASURE_K1_OPERATE_GUARD) { break; }
    }
    failures += expect_true(measure.state == HW_METROLOGY_MEASURE_K1_OPERATE_GUARD &&
        fake.k1 == HW_K1_STATE_MEASURE, "reach relay guard before acquiring ADC");
    failures += expect_true(HW_METROLOGY_MEASURE_K1_OPERATE_GUARD_MS == 30u &&
        measure.block.k1_operate_guard_ms == 30u, "30ms guard is frozen into capture metadata");
    (void)hw_metrology_measure_step(&measure, measure.wait_deadline_ms - 1u);
    failures += expect_true(measure.state == HW_METROLOGY_MEASURE_K1_OPERATE_GUARD &&
        !fake.capture_started, "no ADC capture before complete relay guard including wrap");
    now = measure.wait_deadline_ms;
    (void)hw_metrology_measure_step(&measure, now);
    failures += expect_true(measure.state == HW_METROLOGY_MEASURE_ADC_ACQUIRE && !fake.capture_started,
        "at 30ms proceed to ADC acquisition; excitation settling still follows");
    (void)hw_metrology_measure_abort(&measure);
    failures += expect_true(run_until_idle(&measure, &fake, &now) == 0 && fake.k1 == HW_K1_STATE_SAFE,
        "aborting after guard returns safe");
    return failures;
}

#if WTK_ENABLE_MEASURE_SESSION_HOLD
static int run_until_hold(hw_metrology_measure_t *measure, fake_io_t *fake, uint32_t *now)
{
    for (uint32_t i = 0u; i < 200u; i++)
    {
        fake->now_ms = *now;
        (void)hw_metrology_measure_step(measure, (*now)++);
        if (measure->state == HW_METROLOGY_MEASURE_SESSION_HOLD) return 0;
        if (measure->state == HW_METROLOGY_MEASURE_IDLE || measure->state == HW_METROLOGY_MEASURE_DONE) break;
    }
    return 1;
}

static int test_bounded_relay_session(void)
{
    int failures = 0;
    for (uint8_t wrap = 0u; wrap < 2u; wrap++)
    {
        uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT] = {0};
        fake_io_t fake = {.k1 = HW_K1_STATE_SAFE, .charger = HW_CHARGER_ABSENT};
        hw_metrology_measure_io_t io;
        hw_metrology_measure_t measure;
        hw_range_t range;
        const hw_range_io_t range_io = {
            .write_enable = preflight_range_enable, .write_address = preflight_range_address, .user_data = &fake};
        (void)hw_range_init(&range, &range_io);
        fake.real_range = &range;
        bind_default_permit_inputs(&fake, HW_RANGE_ID_1K);
        bind_io(&io, &fake);
        (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
        const bsp_clock_summary_t clock = production_clock();
        hw_metrology_measure_request_t request = make_request(&clock, HW_RANGE_ID_1K, HW_EXCITATION_AMP_100MVRMS);
        request.session_hold = true;
        uint32_t now = (wrap == 0u) ? 0u : UINT32_MAX - 80u;
        failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_BUSY,
                                "session begins with ordinary safe entry");
        for (uint8_t i = 0u; i < 4u; i++)
        {
            failures += expect_true(run_until_hold(&measure, &fake, &now) == 0, "capture reaches supervised HOLD");
            failures += expect_true(fake.k1 == HW_K1_STATE_MEASURE && fake.exc == HW_EXCITATION_MODE_NEUTRAL &&
                                       fake.quiet && fake.aux_paused && measure.adc_owned,
                                    "held capture owns neutral K1 ADC quiet window");
            failures += expect_true(hw_metrology_measure_result_ready(&measure) &&
                                       !hw_metrology_measure_dumpable(&measure), "held result is DSP-only, never UART dumpable");
            hw_metrology_measure_acknowledge(&measure);
            failures += expect_true(hw_metrology_measure_active(&measure) && hw_metrology_measure_k1_owned(),
                                    "DSP acknowledgement cannot release session ownership");
            if (i < 3u)
            {
                request.range_id = (i == 0u) ? HW_RANGE_ID_100R : HW_RANGE_ID_10R;
                request.frequency = (i == 2u) ? HW_EXCITATION_FREQ_10KHZ : HW_EXCITATION_FREQ_1KHZ;
                failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_BUSY,
                                        "continue same hardware-owned authorization");
            }
        }
        failures += expect_true(fake.k1_request_calls == 1u && fake.permit_validate_calls == 1u &&
                                   measure.permit.consumed && fake.restore_calls == 0u && fake.resume_calls == 0u,
                                "four conditions use one K1 entry and no stale permit reissue or aux resume");
        failures += expect_true(!fake.address_while_enabled && !fake.address_while_driven,
                                "actual range service changes address only disabled and neutral");
        (void)hw_metrology_measure_finish_session(&measure);
        failures += expect_true(run_until_idle(&measure, &fake, &now) == 0, "session final cleanup completes");
        failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF &&
                                   !fake.quiet && fake.restore_calls == 1u && fake.resume_calls == 1u &&
                                   !hw_metrology_measure_k1_owned() && !range.enabled,
                                "final cleanup restores ADC once and leaves all outputs SAFE");
    }
    return failures;
}

static int test_session_capture_limit_and_range_timeout(void)
{
    int failures = 0;
    for (uint8_t variant = 0u; variant < 3u; variant++)
    {
        uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT] = {0};
        fake_io_t fake = {.k1 = HW_K1_STATE_SAFE, .charger = HW_CHARGER_ABSENT};
        hw_metrology_measure_io_t io;
        hw_metrology_measure_t measure;
        bind_default_permit_inputs(&fake, HW_RANGE_ID_1K);
        bind_io(&io, &fake);
        (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
        const bsp_clock_summary_t clock = production_clock();
        hw_metrology_measure_request_t request = make_request(&clock, HW_RANGE_ID_1K, HW_EXCITATION_AMP_100MVRMS);
        request.session_hold = true;
        if (variant == 2u) request.frequency = HW_EXCITATION_FREQ_10HZ;
        uint32_t now = 0u;
        (void)hw_metrology_measure_start(&measure, &request, now);
        if (variant == 2u)
        {
            failures += expect_true(run_until_idle(&measure, &fake, &now) == 0 &&
                                       fake.k1 == HW_K1_STATE_SAFE && fake.restore_calls == 1u && !fake.quiet,
                                    "slow 10-Hz request retains one-capture SAFE teardown");
            continue;
        }
        const uint8_t captures = (variant == 0u) ? (uint8_t)HW_METROLOGY_SESSION_MAX_CAPTURES : 1u;
        for (uint8_t i = 0u; i < captures; i++)
        {
            failures += expect_true(run_until_hold(&measure, &fake, &now) == 0, "bounded capture reaches HOLD");
            hw_metrology_measure_acknowledge(&measure);
            if (i + 1u < captures) (void)hw_metrology_measure_start(&measure, &request, now);
        }
        if (variant == 0u)
        {
            failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_ERROR,
                                    "seventh capture cannot extend session authorization");
        }
        else
        {
            request.range_id = HW_RANGE_ID_100R;
            (void)hw_metrology_measure_start(&measure, &request, now);
            (void)hw_metrology_measure_step(&measure, now++);
            fake.range_ready = false;
            fake.range_state = HW_RANGE_TRANSITIONING;
        }
        failures += expect_true(run_until_idle(&measure, &fake, &now) == 0 &&
                                   fake.k1 == HW_K1_STATE_SAFE && !fake.quiet && fake.restore_calls == 1u,
                                "capture limit or unsettled range triggers complete safe cleanup");
        if (variant == 1u)
            failures += expect_true(measure.error == HW_METROLOGY_MEASURE_ERR_RANGE,
                                    "bounded neutral range transition cannot wait forever");
        failures += expect_true(fake.k1_request_calls == 1u, "bounded failure never reenergizes relay");
    }
    return failures;
}

#if WTK_ENABLE_MEASUREMENT_TRIMMED
static int test_statistical_continuation_boundaries(void)
{
    int failures = 0;
    for (uint8_t variant = 0u; variant < 6u; variant++)
    {
        uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT] = {0};
        fake_io_t fake = {.k1 = HW_K1_STATE_SAFE, .charger = HW_CHARGER_ABSENT};
        hw_metrology_measure_io_t io;
        hw_metrology_measure_t measure;
        bind_default_permit_inputs(&fake, HW_RANGE_ID_1K);
        bind_io(&io, &fake);
        (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
        const bsp_clock_summary_t clock = production_clock();
        hw_metrology_measure_request_t request = make_request(&clock, HW_RANGE_ID_1K, HW_EXCITATION_AMP_100MVRMS);
        request.session_hold = true;
        uint32_t now = UINT32_MAX - 80u;
        if (variant == 5u)
        {
            request.statistical_capture = true;
            failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_INVALID_ARG &&
                                    fake.k1_request_calls == 0u, "statistical role cannot initiate a relay session");
            continue;
        }
        (void)hw_metrology_measure_start(&measure, &request, now);
        for (uint8_t i = 0u; i < HW_METROLOGY_SESSION_MAX_CAPTURES; i++)
        {
            failures += expect_true(run_until_hold(&measure, &fake, &now) == 0, "search budget remains six captures");
            hw_metrology_measure_acknowledge(&measure);
            if (i + 1u < HW_METROLOGY_SESSION_MAX_CAPTURES)
                (void)hw_metrology_measure_start(&measure, &request, now);
        }
        const uint32_t connected_deadline = measure.session_deadline_ms;
        const uint32_t battery_deadline = measure.session_battery_deadline_ms;
        request.statistical_capture = true;
        request.frequency = HW_EXCITATION_FREQ_10KHZ;
        failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_BUSY &&
                                measure.state == HW_METROLOGY_MEASURE_RANGE_REQUEST,
                                "first statistical continuation may return to another primary condition");
        failures += expect_true(run_until_hold(&measure, &fake, &now) == 0, "first statistical capture completes");
        hw_metrology_measure_acknowledge(&measure);
        if (variant == 0u)
        {
            for (uint8_t i = 1u; i < HW_METROLOGY_SESSION_MAX_STATISTICAL_CAPTURES; i++)
            {
                failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_BUSY &&
                                        measure.state == HW_METROLOGY_MEASURE_ADC_ACQUIRE,
                                        "same-condition repeat skips redundant range transition");
                failures += expect_true(run_until_hold(&measure, &fake, &now) == 0, "bounded repeat reaches HOLD");
                hw_metrology_measure_acknowledge(&measure);
            }
            failures += expect_true(measure.session_capture_count == 14u && measure.session_statistical_count == 8u,
                                    "six search plus eight statistical windows have explicit separate bounds");
        }
        if (variant == 1u) request.amplitude = HW_EXCITATION_AMP_500MVRMS;
        if (variant == 2u) request.statistical_capture = false;
        if (variant == 3u) now = measure.session_deadline_ms;
        if (variant == 4u) fake.charger = HW_CHARGER_PRESENT;
        failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_ERROR,
                                "budget lock deadline and charger reject further statistical continuations");
        failures += expect_true(measure.session_deadline_ms == connected_deadline &&
                                measure.session_battery_deadline_ms == battery_deadline,
                                "statistical role never refreshes authorization time");
        failures += expect_true(run_until_idle(&measure, &fake, &now) == 0 &&
                                fake.k1 == HW_K1_STATE_SAFE && !fake.quiet && fake.restore_calls == 1u &&
                                fake.k1_request_calls == 1u, "all statistical rejection paths clean up without relay reentry");
    }
    return failures;
}
#endif

#if WTK_ENABLE_CONTINUOUS_SURVEY
static int continuous_until_hold(hw_metrology_measure_t *measure, fake_io_t *fake, uint32_t *now)
{
    for (uint32_t i = 0u; i < 2500u; i++)
    {
        fake->now_ms = *now;
        if (fake->sensors != NULL) (void)hw_aux_sensors_step(fake->sensors, *now);
        (void)hw_metrology_measure_step(measure, (*now)++);
        if (measure->state == HW_METROLOGY_MEASURE_SESSION_HOLD) return 0;
        if (!hw_metrology_measure_active(measure)) break;
    }
    return 1;
}

static int test_continuous_power_supervised_survey(void)
{
    int failures = 0;
    for (uint8_t wrap = 0u; wrap < 2u; wrap++)
    {
        uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT] = {0};
        fake_io_t fake = {.k1 = HW_K1_STATE_SAFE, .charger = HW_CHARGER_ABSENT, .timed_capture = true};
        hw_metrology_measure_io_t io;
        hw_metrology_measure_t measure;
        bind_default_permit_inputs(&fake, HW_RANGE_ID_1K);
        bind_io(&io, &fake);
        (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
        const bsp_clock_summary_t clock = production_clock();
        hw_metrology_measure_request_t request = make_request(&clock, HW_RANGE_ID_1K, HW_EXCITATION_AMP_100MVRMS);
        request.session_hold = true;
        request.continuous_survey = true;
        uint32_t now = wrap == 0u ? 0u : UINT32_MAX - 80u;
        preflight_adc_t adc = {0};
        hw_aux_sensors_t sensors;
        const hw_aux_adc_io_t sensor_io = {preflight_adc_start, preflight_adc_poll, preflight_adc_cancel, &adc};
        if (wrap == 0u)
        {
            (void)hw_aux_sensors_init(&sensors, &sensor_io, 0u);
            for (; now < 200u; now++)
                for (uint8_t j = 0u; j < 8u; j++) (void)hw_aux_sensors_step(&sensors, now);
            fake.sensors = &sensors;
        }
        failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_BUSY,
                                "explicit supervised survey starts through ordinary fresh permit");
        uint32_t connected_deadline = 0u;
        uint32_t issue_ms = 0u;
        for (uint8_t i = 0u; i < 9u; i++)
        {
            failures += expect_true(continuous_until_hold(&measure, &fake, &now) == 0,
                                    "coherent timed capture completes despite interleaved power sampling");
            if (i == 0u) {connected_deadline = measure.session_deadline_ms; issue_ms = measure.block.permit_issue_ms;}
            failures += expect_true(measure.error == HW_METROLOGY_MEASURE_OK && fake.k1 == HW_K1_STATE_MEASURE &&
                fake.exc == HW_EXCITATION_MODE_NEUTRAL && fake.quiet && fake.aux_paused &&
                measure.session_deadline_ms == connected_deadline && measure.block.permit_issue_ms == issue_ms,
                "one unchanged authorization owns neutral DUT and quiet across all frequencies and means");
            hw_metrology_measure_acknowledge(&measure);
            if (i == 8u) break;
            request.frequency = i == 0u ? HW_EXCITATION_FREQ_10KHZ : HW_EXCITATION_FREQ_10HZ;
            request.statistical_capture = i >= 2u;
            failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_BUSY,
                                    "10Hz and all six statistical windows continue without another K1 entry");
        }
        failures += expect_true(fake.k1_request_calls == 1u && fake.permit_validate_calls == 1u &&
            measure.session_capture_count == 9u && measure.session_statistical_count == 6u &&
            measure.power_refresh_count > 20u && !fake.power_during_capture && fake.resume_calls == 0u,
            "realistic 400ms windows use one K1/permit and frequent non-overlapping health groups");
        if (wrap == 0u)
        {
            hw_aux_sensors_snapshot_t health;
            hw_aux_sensors_snapshot(&sensors, now, &health);
            failures += expect_true(health.battery_state == HW_BATTERY_OK && health.battery_age_ms < 1000u &&
                                    health.residual_state == HW_RESIDUAL_UNKNOWN && !sensors.residual_published,
                                    "actual auxiliary FSM refreshes power without publishing disconnected SAFE inputs");
        }
        failures += expect_true((now - (connected_deadline - HW_METROLOGY_SURVEY_MAX_CONNECTED_MS)) > 8000u &&
            (now - (connected_deadline - HW_METROLOGY_SURVEY_MAX_CONNECTED_MS)) < 12000u,
            "power checks preserve physical settle deadline rather than restart it indefinitely");
        (void)hw_metrology_measure_finish_session(&measure);
        failures += expect_true(run_until_idle(&measure, &fake, &now) == 0 && fake.k1 == HW_K1_STATE_SAFE &&
            !fake.quiet && !fake.aux_paused && fake.resume_calls == 1u &&
            fake.restore_calls == (uint32_t)measure.power_refresh_count + 1u,
            "only final cleanup releases relay/normal auxiliary ownership once");
    }
    return failures;
}

static int test_continuous_power_failures(void)
{
    int failures = 0;
    for (uint8_t variant = 1u; variant <= 21u; variant++)
    {
        uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT] = {0};
        fake_io_t fake = {.k1 = HW_K1_STATE_SAFE, .charger = HW_CHARGER_ABSENT, .timed_capture = true};
        hw_metrology_measure_io_t io;
        hw_metrology_measure_t measure;
        bind_default_permit_inputs(&fake, HW_RANGE_ID_1K);
        bind_io(&io, &fake);
        (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
        const bsp_clock_summary_t clock = production_clock();
        hw_metrology_measure_request_t request = make_request(&clock, HW_RANGE_ID_1K, HW_EXCITATION_AMP_100MVRMS);
        request.frequency = HW_EXCITATION_FREQ_10HZ;
        request.session_hold = true;
        request.continuous_survey = true;
        uint32_t now = 0u;
        if (variant == 16u) measure.io.power_status = NULL;
        if (variant == 17u) request.session_hold = false;
        const bsp_status_t started = hw_metrology_measure_start(&measure, &request, now);
        if (variant == 16u || variant == 17u)
        {
            failures += expect_true(started == BSP_STATUS_INVALID_ARG && fake.k1_request_calls == 0u,
                                    "missing monitor contract cannot energize continuous K1");
            continue;
        }
        fake.power_variant = variant <= 8u ? variant : 0u;
        bool injected = variant <= 8u;
        for (uint32_t i = 0u; i < 2000u && hw_metrology_measure_active(&measure); i++)
        {
            fake.now_ms = now;
            const hw_metrology_measure_state_t injection_state = (variant == 18u || variant == 20u) ? HW_METROLOGY_MEASURE_POWER_RESTORE :
                (variant == 19u || variant == 21u) ? HW_METROLOGY_MEASURE_POWER_ACQUIRE : HW_METROLOGY_MEASURE_POWER_WAIT;
            if (!injected && measure.state == injection_state)
            {
                injected = true;
                if (variant == 9u) fake.charger = HW_CHARGER_PRESENT;
                if (variant == 10u) fake.dma_error = true;
                if (variant == 11u) (void)hw_metrology_measure_abort(&measure);
                if (variant == 12u) fake.adc_fail = true;
                if (variant == 13u) now = measure.session_deadline_ms;
                if (variant == 14u) now = measure.session_battery_deadline_ms;
                if (variant == 15u) fake.restore_fail = true;
                if (variant == 18u || variant == 19u) (void)hw_metrology_measure_abort(&measure);
                if (variant >= 20u) now = measure.power_deadline_ms;
            }
            (void)hw_metrology_measure_step(&measure, now++);
        }
        failures += expect_true(injected && !hw_metrology_measure_active(&measure) &&
            measure.error != HW_METROLOGY_MEASURE_OK && fake.k1 == HW_K1_STATE_SAFE &&
            fake.exc == HW_EXCITATION_MODE_OFF && !fake.quiet && !hw_metrology_measure_dumpable(&measure),
            "power/timestamp/time/ADC/charger/DMA/cancel failures converge to SAFE without a result");
        failures += expect_true(fake.k1_request_calls == 1u && !fake.power_during_capture,
                                "failure cannot retry relay entry or overlap power with DMA");
    }
    return failures;
}

static int test_continuous_capture_budget_and_mode_lock(void)
{
    int failures = 0;
    for (uint8_t variant = 0u; variant < 2u; variant++)
    {
        uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT] = {0};
        fake_io_t fake = {.k1 = HW_K1_STATE_SAFE, .charger = HW_CHARGER_ABSENT, .timed_capture = true};
        hw_metrology_measure_io_t io;
        hw_metrology_measure_t measure;
        bind_default_permit_inputs(&fake, HW_RANGE_ID_1K);
        bind_io(&io, &fake);
        (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
        const bsp_clock_summary_t clock = production_clock();
        hw_metrology_measure_request_t request = make_request(&clock, HW_RANGE_ID_1K, HW_EXCITATION_AMP_100MVRMS);
        request.session_hold = true;
        request.continuous_survey = true;
        uint32_t now = 0u;
        (void)hw_metrology_measure_start(&measure, &request, now);
        const uint8_t count = variant == 0u ? 20u : 1u;
        for (uint8_t i = 0u; i < count; i++)
        {
            failures += expect_true(continuous_until_hold(&measure, &fake, &now) == 0, "bounded owner reaches held capture");
            hw_metrology_measure_acknowledge(&measure);
            if (i + 1u < count)
            {
                request.statistical_capture = i >= 11u;
                if (request.statistical_capture) request.frequency = HW_EXCITATION_FREQ_10HZ;
                (void)hw_metrology_measure_start(&measure, &request, now);
            }
        }
        if (variant == 0u)
            failures += expect_true(measure.session_capture_count == 20u && measure.session_statistical_count == 8u,
                                    "continuous hardware enforces separate twelve-search/eight-statistical maxima");
        else request.continuous_survey = false;
        failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_ERROR,
                                "twenty-first capture or ownership-mode change cannot refresh authorization");
        failures += expect_true(run_until_idle(&measure, &fake, &now) == 0 && fake.k1 == HW_K1_STATE_SAFE &&
                                fake.k1_request_calls == 1u && !fake.quiet, "budget/mode failure releases only original relay owner");
    }
    return failures;
}
#endif

static int test_held_session_fault_matrix(void)
{
    int failures = 0;
    for (uint8_t fault = 0u; fault < 16u; fault++)
    {
        uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT] = {0};
        fake_io_t fake = {.k1 = HW_K1_STATE_SAFE, .charger = HW_CHARGER_ABSENT};
        hw_metrology_measure_io_t io;
        hw_metrology_measure_t measure;
        bind_default_permit_inputs(&fake, HW_RANGE_ID_1K);
        fake.issue_input.battery_age_ms = (fault == 8u) ? 800u : 0u;
        bind_io(&io, &fake);
        (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
        const bsp_clock_summary_t clock = production_clock();
        hw_metrology_measure_request_t request = make_request(&clock, HW_RANGE_ID_1K, HW_EXCITATION_AMP_100MVRMS);
        request.session_hold = true;
        uint32_t now = 0u;
        (void)hw_metrology_measure_start(&measure, &request, now);
        failures += expect_true(run_until_hold(&measure, &fake, &now) == 0, "fault fixture reaches held capture");
        hw_metrology_measure_acknowledge(&measure);
        switch (fault)
        {
        case 0u: fake.charger = HW_CHARGER_PRESENT; break;
        case 1u: fake.charger = HW_CHARGER_UNKNOWN; break;
        case 2u: fake.fault_mask = 1u; break;
        case 3u: fake.range_state = HW_RANGE_INVALID; break;
        case 4u: fake.range_state = HW_RANGE_DISABLED; break;
        case 5u: fake.exc = HW_EXCITATION_MODE_SINE; break;
        case 6u: fake.dma_error = true; break;
        case 7u: now = measure.session_deadline_ms; break;
        case 8u: now = measure.session_battery_deadline_ms; break;
        case 9u: fake.k1 = HW_K1_STATE_SAFE; break;
        case 10u:
            request.range_id = HW_RANGE_ID_10R; request.amplitude = HW_EXCITATION_AMP_500MVRMS;
            (void)hw_metrology_measure_start(&measure, &request, now); break;
        case 11u: (void)hw_metrology_measure_abort(&measure); break;
        case 12u: fake.range_id = HW_RANGE_ID_100R; break;
        case 13u: (void)hw_metrology_measure_start(&measure, NULL, now); break;
        case 14u: fake.restore_fail = true; (void)hw_metrology_measure_finish_session(&measure); break;
        default: fake.range_state = (hw_safety_range_state_t)255; break;
        }
        failures += expect_true(run_until_idle(&measure, &fake, &now) == 0, "held fault converges through bounded cleanup");
        failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF &&
                                   !fake.quiet && !hw_metrology_measure_dumpable(&measure) &&
                                   !hw_metrology_measure_k1_owned() && measure.error != HW_METROLOGY_MEASURE_OK,
                                "held fault revokes result and all hazardous outputs");
        failures += expect_true(fake.k1_request_calls == 1u && fake.permit_validate_calls == 1u,
                                "fault never reenergizes K1 or reuses consumed permit");
    }
    return failures;
}
#endif

int main(void)
{
    int failures = test_first_measurement_preflight();
    failures += test_30ms_relay_guard();
#if WTK_ENABLE_MEASURE_SESSION_HOLD
    failures += test_bounded_relay_session();
    failures += test_held_session_fault_matrix();
    failures += test_session_capture_limit_and_range_timeout();
#if WTK_ENABLE_CONTINUOUS_SURVEY
    failures += test_continuous_power_supervised_survey();
    failures += test_continuous_power_failures();
    failures += test_continuous_capture_budget_and_mode_lock();
#endif
#if WTK_ENABLE_MEASUREMENT_TRIMMED
    failures += test_statistical_continuation_boundaries();
#endif
#endif
#if WTK_ENABLE_BENCH_SUPPLY
    failures += test_external_bench_supply();
#endif
    uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    memset(raw, 0, sizeof(raw));

    bsp_clock_summary_t clock = production_clock();
    hw_metrology_measure_io_t io;
    fake_io_t fake = {0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_id = HW_RANGE_ID_10K;
    fake.range_state = HW_RANGE_READY;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
    bind_io(&io, &fake);

    hw_metrology_measure_t measure;
    io.charger_state = NULL;
    failures += expect_true(hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT) ==
                                BSP_STATUS_INVALID_ARG, "missing live charger callback must fail closed");
    io.charger_state = fake_charger;
    failures += expect_true(hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT) ==
                                BSP_STATUS_OK,
                            "init");

    hw_metrology_measure_request_t request = make_request(&clock, HW_RANGE_ID_10K, HW_EXCITATION_AMP_100MVRMS);
    uint32_t now = 0u;
    failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_BUSY, "start");
    failures += expect_true(run_until_idle(&measure, &fake, &now) == 0, "success completes");
    failures += expect_true(hw_metrology_measure_state(&measure) == HW_METROLOGY_MEASURE_DONE, "DONE");
    failures += expect_true(hw_metrology_measure_dumpable(&measure), "dumpable");
    failures += expect_true(fake.k1_request_calls == 1u, "single K1 request");
    failures += expect_true(fake.permit_issue_calls == 2u, "issue input for permit and K1 safety");
    failures += expect_true(fake.permit_validate_calls == 1u, "single permit validate");
    failures += expect_true(saw_state(&fake, HW_METROLOGY_MEASURE_PERMIT_ISSUE), "permit issue state");
    failures += expect_true(saw_state(&fake, HW_METROLOGY_MEASURE_PERMIT_VALIDATE), "permit validate state");
    failures += expect_true(saw_state(&fake, HW_METROLOGY_MEASURE_K1_REQUEST), "K1 request state");
    failures += expect_true(saw_state(&fake, HW_METROLOGY_MEASURE_K1_OPERATE_GUARD), "operate guard state");
    failures += expect_true(saw_state(&fake, HW_METROLOGY_MEASURE_K1_RELEASE_GUARD), "release guard state");
    failures += expect_true(saw_state(&fake, HW_METROLOGY_MEASURE_EXC_NEUTRAL_POST), "neutral before K1 safe");
    const hw_metrology_block_t *block = hw_metrology_measure_block(&measure);
    failures += expect_true(block != NULL && block->dut_measure, "dut measure block");
    hw_metrology_measure_acknowledge(&measure);

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_id = HW_RANGE_ID_10K;
    fake.range_state = HW_RANGE_READY;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
    fake.validate_input.k1_state = HW_K1_STATE_MEASURE;
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    (void)run_until_idle(&measure, &fake, &now);
    failures += expect_true(hw_metrology_measure_error(&measure) == HW_METROLOGY_MEASURE_ERR_PERMIT,
                            "permit validate failure");
    failures += expect_true(fake.k1_request_calls == 0u, "no K1 without validate");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_id = HW_RANGE_ID_10K;
    fake.range_state = HW_RANGE_READY;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    uint32_t measure_start_ms = 0u;
    for (uint32_t i = 0u; i < 8000u; i++)
    {
        if (fake.k1 == HW_K1_STATE_MEASURE)
        {
            fake.charger = HW_CHARGER_PRESENT;
            measure_start_ms = now;
        }
        (void)hw_metrology_measure_step(&measure, now);
        now += 1u;
        if (hw_metrology_measure_state(&measure) == HW_METROLOGY_MEASURE_IDLE)
        {
            break;
        }
    }
    failures += expect_true(hw_metrology_measure_error(&measure) == HW_METROLOGY_MEASURE_ERR_ABORT,
                            "charger during measure abort");
    failures += expect_true(measure_start_ms > 0u, "reached measure before charger abort");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_id = HW_RANGE_ID_10K;
    fake.range_state = HW_RANGE_READY;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    for (uint32_t i = 0u; i < 8000u; i++)
    {
        if (fake.k1 == HW_K1_STATE_MEASURE)
        {
            fake.fault_mask = 1u;
        }
        (void)hw_metrology_measure_step(&measure, now);
        now += 1u;
        if (hw_metrology_measure_state(&measure) == HW_METROLOGY_MEASURE_IDLE)
        {
            break;
        }
    }
    failures += expect_true(hw_metrology_measure_error(&measure) == HW_METROLOGY_MEASURE_ERR_ABORT,
                            "fault during measure abort");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_id = HW_RANGE_ID_10K;
    fake.range_state = HW_RANGE_READY;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    for (uint32_t i = 0u; i < 8000u; i++)
    {
        if (fake.k1 == HW_K1_STATE_MEASURE)
        {
            fake.range_state = HW_RANGE_TRANSITIONING;
        }
        (void)hw_metrology_measure_step(&measure, now);
        now += 1u;
        if (hw_metrology_measure_state(&measure) == HW_METROLOGY_MEASURE_IDLE)
        {
            break;
        }
    }
    failures += expect_true(hw_metrology_measure_error(&measure) == HW_METROLOGY_MEASURE_ERR_ABORT,
                            "range loss abort");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_id = HW_RANGE_ID_10K;
    fake.range_state = HW_RANGE_READY;
    fake.restore_transient = true;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    (void)run_until_idle(&measure, &fake, &now);
    failures += expect_true(hw_metrology_measure_error(&measure) == HW_METROLOGY_MEASURE_ERR_AUX_RESTORE,
                            "transient restore error");
    failures += expect_true(fake.adc_faults > 0u, "transient restore latches ADC");
    failures += expect_true(!hw_metrology_measure_dumpable(&measure), "transient restore not dumpable");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_id = HW_RANGE_ID_10K;
    fake.range_state = HW_RANGE_READY;
    fake.k1_request_fail = true;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    (void)run_until_idle(&measure, &fake, &now);
    failures += expect_true(hw_metrology_measure_error(&measure) == HW_METROLOGY_MEASURE_ERR_K1,
                            "K1 request failure");
    failures += expect_true(fake.k1_faults > 0u, "K1 request failure latches K1 IO");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_id = HW_RANGE_ID_10K;
    fake.range_state = HW_RANGE_READY;
    fake.k1_safe_fail = true;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    (void)run_until_idle(&measure, &fake, &now);
    failures += expect_true(fake.k1_faults > 0u, "K1 safe failure latches K1 IO");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_id = HW_RANGE_ID_10K;
    fake.range_state = HW_RANGE_READY;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    uint32_t abort_enter_ms = 0u;
    for (uint32_t i = 0u; i < 8000u; i++)
    {
        if (fake.k1 == HW_K1_STATE_MEASURE && fake.aux_paused && fake.fault_mask == 0u)
        {
            fake.fault_mask = 1u;
            abort_enter_ms = now;
        }
        (void)hw_metrology_measure_step(&measure, now);
        now += 1u;
        if (hw_metrology_measure_state(&measure) == HW_METROLOGY_MEASURE_IDLE)
        {
            break;
        }
    }
    failures += expect_true(fake.resume_calls > 0u, "abort resumes aux");
    failures += expect_true(fake.resume_at_ms >= (abort_enter_ms + HW_METROLOGY_MEASURE_K1_RELEASE_GUARD_MS),
                            "aux resume after release guard");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_id = HW_RANGE_ID_10K;
    fake.range_state = HW_RANGE_READY;
    fake.exc_off_fail = true;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    (void)run_until_idle(&measure, &fake, &now);
    failures += expect_true(fake.metrology_faults > 0u, "excitation off failure latches metrology runtime");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_id = HW_RANGE_ID_10K;
    fake.range_state = HW_RANGE_READY;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_10K);
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    for (uint32_t i = 0u; i < 8000u; i++)
    {
        (void)hw_metrology_measure_step(&measure, now);
        now += 1u;
        if (fake.k1 == HW_K1_STATE_MEASURE)
        {
            break;
        }
    }
    failures += expect_true(fake.k1 == HW_K1_STATE_MEASURE, "public abort setup reached measure");
    failures += expect_true(hw_metrology_measure_abort(&measure) == BSP_STATUS_BUSY, "public abort starts cleanup");
    failures += expect_true(run_until_idle(&measure, &fake, &now) == 0, "public abort cleanup completes");
    failures += expect_true(hw_metrology_measure_error(&measure) == HW_METROLOGY_MEASURE_ERR_ABORT,
                            "public abort error recorded");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE, "public abort leaves K1 safe");
    failures += expect_true(fake.exc == HW_EXCITATION_MODE_OFF, "public abort leaves excitation off");
    failures += expect_true(!fake.quiet, "public abort releases quiet");
    failures += expect_true(!hw_metrology_measure_dumpable(&measure), "public abort not dumpable");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.charger = HW_CHARGER_ABSENT;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_1M);
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    request = make_request(&clock, HW_RANGE_ID_1M, HW_EXCITATION_AMP_100MVRMS);
    request.dc_pilot = true;
    request.ac_screened = false;
    fake.k1 = HW_K1_STATE_MEASURE;
    fake.exc = HW_EXCITATION_MODE_SINE;
    failures += expect_true(hw_metrology_measure_start(&measure, &request, 0u) == BSP_STATUS_INVALID_ARG,
                            "DC requires fresh AC screening");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF,
                            "rejected DC request forces safe outputs");
    fake.k1_safe_fail = true;
    failures += expect_true(hw_metrology_measure_start(&measure, &request, 0u) == BSP_STATUS_ERROR,
                            "failed DC safe command is reported");
    failures += expect_true(fake.k1_faults == 1u, "failed DC safe command latches K1 fault");
    fake.k1_safe_fail = false;
    request.ac_screened = true;
    request.range_id = HW_RANGE_ID_10K;
    failures += expect_true(hw_metrology_measure_start(&measure, &request, 0u) == BSP_STATUS_INVALID_ARG,
                            "DC cannot select lower RREF");
    request.range_id = HW_RANGE_ID_1M;
    now = 0u;
    failures += expect_true(hw_metrology_measure_start(&measure, &request, now) == BSP_STATUS_BUSY,
                            "DC transaction starts");
    failures += expect_true(run_until_idle(&measure, &fake, &now) == 0, "DC transaction terminates");
    failures += expect_true(hw_metrology_measure_dumpable(&measure), "DC capture valid after teardown");
    failures += expect_true(hw_metrology_measure_block(&measure)->mode == HW_METROLOGY_MODE_DC_PILOT,
                            "DC block tagged distinctly");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF &&
                                !fake.quiet && fake.aux_resumed,
                            "DC leaves hardware safe and aux resumed");
    failures += expect_true(fake.permit_validate_calls == 1u, "DC consumes its own permit");

    hw_metrology_measure_acknowledge(&measure);
    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.charger = HW_CHARGER_ABSENT;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_1M);
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    for (uint32_t i = 0u; i < 8000u; i++)
    {
        (void)hw_metrology_measure_step(&measure, now++);
        if (fake.exc == HW_EXCITATION_MODE_DC_PILOT)
        {
            break;
        }
    }
    failures += expect_true(fake.exc == HW_EXCITATION_MODE_DC_PILOT, "DC reaches static bias");
    failures += expect_true(hw_metrology_measure_abort(&measure) == BSP_STATUS_BUSY, "DC abort starts cleanup");
    failures += expect_true(run_until_idle(&measure, &fake, &now) == 0, "DC abort cleanup terminates");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF &&
                                !fake.quiet && !hw_metrology_measure_dumpable(&measure),
                            "DC abort leaves no valid result or active output");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.charger = HW_CHARGER_ABSENT;
    bind_default_permit_inputs(&fake, HW_RANGE_ID_1M);
    bind_io(&io, &fake);
    (void)hw_metrology_measure_init(&measure, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_measure_start(&measure, &request, now);
    for (uint32_t i = 0u; i < 8000u; i++)
    {
        (void)hw_metrology_measure_step(&measure, now++);
        if (fake.exc == HW_EXCITATION_MODE_DC_PILOT)
        {
            break;
        }
    }
    fake.charger = HW_CHARGER_PRESENT;
    failures += expect_true(run_until_idle(&measure, &fake, &now) == 0,
                            "charger during DC converges through cleanup");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF &&
                                !fake.quiet && !hw_metrology_measure_dumpable(&measure),
                            "charger during DC cannot publish result");

    return (failures == 0) ? 0 : 1;
}
