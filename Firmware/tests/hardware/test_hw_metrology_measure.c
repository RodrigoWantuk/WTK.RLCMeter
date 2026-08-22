#include "hardware/hw_metrology_measure.h"

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
    if (fake->seen_count < 96u)
    {
        fake->seen[fake->seen_count] = state;
        fake->seen_count++;
    }
}

static bsp_status_t fake_k1_safe(void *user)
{
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
    (void)now_ms;
    (void)user;
    return BSP_STATUS_BUSY;
}

static bool fake_range_ready(void *user)
{
    return ((fake_io_t *)user)->range_ready;
}

static hw_range_id_t fake_range_current_id(void *user)
{
    return ((fake_io_t *)user)->range_id;
}

static hw_safety_range_state_t fake_range_safety_state(void *user)
{
    return ((fake_io_t *)user)->range_state;
}

static bsp_status_t fake_range_disable(void *user)
{
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
    fake->aux_paused = true;
    fake->aux_resumed = false;
}

static void fake_aux_resume(uint32_t now_ms, void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
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
    (void)raw_words;
    (void)word_count;
    (void)profile;
    fake_io_t *fake = (fake_io_t *)user;
    fake->capture_started = true;
    if (!fake->dma_error)
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
    return ((fake_io_t *)user)->dma_complete;
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
    (void)frequency;
    (void)amplitude;
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->exc_fail || (fake->exc != HW_EXCITATION_MODE_NEUTRAL))
    {
        return BSP_STATUS_ERROR;
    }
    fake->exc = HW_EXCITATION_MODE_SINE;
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
    return ((fake_io_t *)user)->fault_mask;
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

static void bind_io(hw_metrology_measure_io_t *io, fake_io_t *fake)
{
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
    io->adc_dma_complete = fake_dma_complete;
    io->adc_dma_error = fake_dma_error;
    io->excitation_off = fake_exc_off;
    io->excitation_neutral = fake_exc_neutral;
    io->excitation_sine = fake_exc_sine;
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

int main(void)
{
    int failures = 0;
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
        if (fake.k1 == HW_K1_STATE_MEASURE)
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

    return (failures == 0) ? 0 : 1;
}
