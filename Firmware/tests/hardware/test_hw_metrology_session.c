#include "hardware/hw_metrology_session.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    hw_k1_state_t k1;
    bool range_ready;
    bool range_fail;
    bool quiet;
    bool aux_paused;
    bool aux_resumed;
    bool adc_fail;
    bool restore_fail;
    bool dma_complete;
    bool dma_error;
    bool capture_started;
    bool adc_stopped;
    hw_excitation_mode_t exc;
    bool exc_fail;
    bool exc_dma_error;
    uint32_t k1_measure_calls;
    uint32_t runtime_faults;
    uint32_t k1_faults;
    uint32_t range_faults;
    uint32_t metrology_faults;
    uint32_t restore_calls;
    uint32_t resume_calls;
    bool restore_transient;
    bool exc_off_fail;
    bool k1_safe_fail;
    bool range_disable_fail;
    hw_metrology_session_state_t seen[64];
    uint32_t seen_count;
    bool k1_unsafe_during_run;
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

static void record_state(fake_io_t *fake, hw_metrology_session_state_t state)
{
    if (fake->seen_count < 64u)
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

static hw_k1_state_t fake_k1_state(void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->k1 != HW_K1_STATE_SAFE)
    {
        fake->k1_unsafe_during_run = true;
        fake->k1_measure_calls++;
    }
    return fake->k1;
}

static bsp_status_t fake_range_request(hw_range_id_t id, uint32_t now_ms, void *user)
{
    (void)id;
    (void)now_ms;
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->range_fail)
    {
        return BSP_STATUS_ERROR;
    }
    fake->range_ready = true;
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

static bsp_status_t fake_range_disable(void *user)
{
    fake_io_t *fake = (fake_io_t *)user;
    if (fake->range_disable_fail)
    {
        return BSP_STATUS_ERROR;
    }
    fake->range_ready = false;
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
    (void)now_ms;
    fake_io_t *fake = (fake_io_t *)user;
    fake->aux_paused = false;
    fake->aux_resumed = true;
    fake->resume_calls++;
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
    (void)user;
    return HW_CHARGER_ABSENT;
}

static void fake_latch_adc(void *user)
{
    ((fake_io_t *)user)->runtime_faults++;
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

static void bind_io(hw_metrology_session_io_t *io, fake_io_t *fake)
{
    io->k1_force_safe = fake_k1_safe;
    io->k1_commanded_state = fake_k1_state;
    io->range_request = fake_range_request;
    io->range_step = fake_range_step;
    io->range_is_ready = fake_range_ready;
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
    io->latch_k1_io_fault = fake_latch_k1;
    io->latch_range_io_fault = fake_latch_range;
    io->latch_adc_runtime_fault = fake_latch_adc;
    io->latch_metrology_runtime_fault = fake_latch_metrology;
    io->user = fake;
}

static int run_until_idle(hw_metrology_session_t *session, fake_io_t *fake, uint32_t *now_ms)
{
    for (uint32_t i = 0u; i < 4000u; i++)
    {
        record_state(fake, hw_metrology_session_state(session));
        const bsp_status_t status = hw_metrology_session_step(session, *now_ms);
        if (fake->k1 != HW_K1_STATE_SAFE)
        {
            fake->k1_unsafe_during_run = true;
        }
        *now_ms += 1u;
        if ((hw_metrology_session_state(session) == HW_METROLOGY_SESSION_DONE) ||
            (hw_metrology_session_state(session) == HW_METROLOGY_SESSION_IDLE))
        {
            (void)status;
            return 0;
        }
    }
    return 1;
}

static hw_metrology_session_request_t make_request(const bsp_clock_summary_t *clock,
                                                   bsp_status_t clock_status,
                                                   hw_range_id_t range,
                                                   hw_excitation_amp_t amp)
{
    hw_metrology_session_request_t request = {
        .clock_summary = clock,
        .clock_init_status = clock_status,
        .frequency = HW_EXCITATION_FREQ_1KHZ,
        .amplitude = amp,
        .range_id = range,
    };
    return request;
}

int main(void)
{
    int failures = 0;
    uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    memset(raw, 0, sizeof(raw));
    for (uint32_t n = 0u; n < HW_METROLOGY_SAMPLES_PER_BLOCK; n++)
    {
        raw[(3u * n) + 0u] = hw_metrology_pack_word(2048u, 2048u);
        raw[(3u * n) + 1u] = hw_metrology_pack_word(2048u, 2048u);
        raw[(3u * n) + 2u] = hw_metrology_pack_word(2048u, 2048u);
    }

    bsp_clock_summary_t clock = production_clock();
    hw_metrology_session_io_t io;
    fake_io_t fake = {0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    bind_io(&io, &fake);

    hw_metrology_session_t session;
    failures += expect_true(hw_metrology_session_init(&session, &io, raw, HW_METROLOGY_RAW_WORD_COUNT) ==
                                BSP_STATUS_OK,
                            "init");

    hw_metrology_session_request_t request = make_request(&clock, BSP_STATUS_OK, HW_RANGE_ID_10K, HW_EXCITATION_AMP_100MVRMS);
    uint32_t now = 0u;
    failures += expect_true(hw_metrology_session_start(&session, &request, now) == BSP_STATUS_BUSY, "start");
    failures += expect_true(run_until_idle(&session, &fake, &now) == 0, "success completes");
    failures += expect_true(hw_metrology_session_state(&session) == HW_METROLOGY_SESSION_DONE, "DONE");
    failures += expect_true(hw_metrology_session_dumpable(&session), "dumpable");
    failures += expect_true(!hw_metrology_session_k1_left_safe(&session), "K1 stayed SAFE");
    failures += expect_true(!fake.k1_unsafe_during_run, "K1 never MEASURE");
    failures += expect_true(fake.exc == HW_EXCITATION_MODE_OFF, "exc OFF after success");
    failures += expect_true(!fake.quiet, "quiet released");
    failures += expect_true(fake.aux_resumed, "aux resumed");
    failures += expect_true(!fake.range_ready, "range disabled");
    failures += expect_true(fake.capture_started, "capture started");

    bool saw_quiet = false;
    bool saw_pause = false;
    bool saw_neutral = false;
    bool saw_sine = false;
    for (uint32_t i = 0u; i < fake.seen_count; i++)
    {
        saw_quiet = saw_quiet || (fake.seen[i] == HW_METROLOGY_SESSION_QUIET_ENTER);
        saw_pause = saw_pause || (fake.seen[i] == HW_METROLOGY_SESSION_AUX_PAUSE);
        saw_neutral = saw_neutral || (fake.seen[i] == HW_METROLOGY_SESSION_EXC_NEUTRAL);
        saw_sine = saw_sine || (fake.seen[i] == HW_METROLOGY_SESSION_EXC_SINE_START);
    }
    failures += expect_true(saw_quiet && saw_pause && saw_neutral && saw_sine, "success state sequence");
    hw_metrology_session_acknowledge(&session);

    clock.source = BSP_CLOCK_SOURCE_HSI;
    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    bind_io(&io, &fake);
    (void)hw_metrology_session_init(&session, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    request = make_request(&clock, BSP_STATUS_OK, HW_RANGE_ID_10K, HW_EXCITATION_AMP_100MVRMS);
    failures += expect_true(hw_metrology_session_start(&session, &request, 0u) == BSP_STATUS_ERROR, "invalid clock");
    failures += expect_true(hw_metrology_session_error(&session) == HW_METROLOGY_SESSION_ERR_CLOCK, "clock error");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE, "clock fail K1 SAFE");
    failures += expect_true(fake.exc == HW_EXCITATION_MODE_OFF, "clock fail exc OFF");

    clock = production_clock();
    request = make_request(&clock, BSP_STATUS_OK, HW_RANGE_ID_10R, HW_EXCITATION_AMP_500MVRMS);
    failures += expect_true(hw_metrology_session_start(&session, &request, 0u) == BSP_STATUS_NOT_SUPPORTED,
                            "forbidden 10r/500m");
    failures += expect_true(hw_metrology_session_error(&session) == HW_METROLOGY_SESSION_ERR_FORBIDDEN_AMPLITUDE,
                            "forbidden error");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE, "forbidden K1 SAFE");
    failures += expect_true(fake.exc == HW_EXCITATION_MODE_OFF, "forbidden exc OFF");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_fail = true;
    bind_io(&io, &fake);
    (void)hw_metrology_session_init(&session, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    request = make_request(&clock, BSP_STATUS_OK, HW_RANGE_ID_10K, HW_EXCITATION_AMP_100MVRMS);
    now = 0u;
    (void)hw_metrology_session_start(&session, &request, now);
    (void)run_until_idle(&session, &fake, &now);
    failures += expect_true(hw_metrology_session_error(&session) == HW_METROLOGY_SESSION_ERR_RANGE, "range fail");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF, "range fail safe");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.adc_fail = true;
    bind_io(&io, &fake);
    (void)hw_metrology_session_init(&session, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_session_start(&session, &request, now);
    (void)run_until_idle(&session, &fake, &now);
    failures += expect_true(hw_metrology_session_error(&session) == HW_METROLOGY_SESSION_ERR_ADC_ACQUIRE,
                            "adc acquire fail");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF, "adc fail safe");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.exc_fail = true;
    bind_io(&io, &fake);
    (void)hw_metrology_session_init(&session, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_session_start(&session, &request, now);
    (void)run_until_idle(&session, &fake, &now);
    failures += expect_true(hw_metrology_session_error(&session) == HW_METROLOGY_SESSION_ERR_EXCITATION,
                            "excitation fail");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF, "exc fail safe");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.dma_error = true;
    bind_io(&io, &fake);
    (void)hw_metrology_session_init(&session, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_session_start(&session, &request, now);
    (void)run_until_idle(&session, &fake, &now);
    failures += expect_true(hw_metrology_session_error(&session) == HW_METROLOGY_SESSION_ERR_DMA, "dma error");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF, "dma fail safe");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    bind_io(&io, &fake);
    fake.dma_complete = false;
    /* Override start so capture never completes. */
    io.adc_start_capture = fake_adc_start;
    (void)hw_metrology_session_init(&session, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_session_start(&session, &request, now);
    /* Force start_capture to leave complete false. */
    fake.dma_complete = false;
    fake.dma_error = false;
    /* Patch: after start, keep complete false by wrapping - fake_adc_start sets complete if !dma_error.
       Set dma_error false and intercept by setting complete false after each start via a second loop. */
    for (uint32_t i = 0u; i < 4000u; i++)
    {
        fake.dma_complete = false;
        (void)hw_metrology_session_step(&session, now);
        now += 1u;
        if ((hw_metrology_session_state(&session) == HW_METROLOGY_SESSION_IDLE) ||
            (hw_metrology_session_state(&session) == HW_METROLOGY_SESSION_DONE))
        {
            break;
        }
    }
    failures += expect_true(hw_metrology_session_error(&session) == HW_METROLOGY_SESSION_ERR_TIMEOUT, "timeout");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF, "timeout safe");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.restore_fail = true;
    bind_io(&io, &fake);
    (void)hw_metrology_session_init(&session, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_session_start(&session, &request, now);
    (void)run_until_idle(&session, &fake, &now);
    failures += expect_true(hw_metrology_session_error(&session) == HW_METROLOGY_SESSION_ERR_AUX_RESTORE,
                            "restore fail");
    failures += expect_true(fake.runtime_faults > 0u, "restore latches ADC runtime");
    failures += expect_true(fake.resume_calls == 0u, "restore fail does not resume");
    failures += expect_true(fake.k1 == HW_K1_STATE_SAFE && fake.exc == HW_EXCITATION_MODE_OFF, "restore fail safe");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.restore_transient = true;
    bind_io(&io, &fake);
    (void)hw_metrology_session_init(&session, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_session_start(&session, &request, now);
    (void)run_until_idle(&session, &fake, &now);
    failures += expect_true(hw_metrology_session_error(&session) == HW_METROLOGY_SESSION_ERR_AUX_RESTORE,
                            "transient restore error");
    failures += expect_true(fake.runtime_faults > 0u, "transient restore latches ADC");
    failures += expect_true(fake.restore_calls >= 2u, "transient restore retried in cleanup");
    failures += expect_true(fake.resume_calls > 0u, "transient restore cleanup resumes aux");
    failures += expect_true(!hw_metrology_session_dumpable(&session), "transient restore not dumpable");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.exc_off_fail = true;
    bind_io(&io, &fake);
    (void)hw_metrology_session_init(&session, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_session_start(&session, &request, now);
    (void)run_until_idle(&session, &fake, &now);
    failures += expect_true(fake.metrology_faults > 0u, "exc off fail latches metrology runtime");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.range_disable_fail = true;
    bind_io(&io, &fake);
    (void)hw_metrology_session_init(&session, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_session_start(&session, &request, now);
    (void)run_until_idle(&session, &fake, &now);
    failures += expect_true(fake.range_faults > 0u, "range disable fail latches range IO");

    fake = (fake_io_t){0};
    fake.k1 = HW_K1_STATE_SAFE;
    fake.exc = HW_EXCITATION_MODE_OFF;
    fake.k1_safe_fail = true;
    bind_io(&io, &fake);
    (void)hw_metrology_session_init(&session, &io, raw, HW_METROLOGY_RAW_WORD_COUNT);
    now = 0u;
    (void)hw_metrology_session_start(&session, &request, now);
    (void)run_until_idle(&session, &fake, &now);
    failures += expect_true(fake.k1_faults > 0u, "k1 safe fail latches K1 IO");

    return (failures == 0) ? 0 : 1;
}
