#include "app/app_calibration_wizard.h"
#include "app/app_io_workspace.h"

#include <stdio.h>
#include <string.h>

#define TEST_MID_V (1.65f)
#define TEST_ADC_SCALE (3.3f / 4095.0f)
#define TEST_HG_NOMINAL (15.4680851064f)

typedef struct
{
    app_calibration_service_t *service;
    hw_metrology_block_t block;
    uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    uint32_t start_count;
    bool active;
    bool done;
    bool dumpable;
    bool abort_called;
} fake_phase05_t;

static app_io_workspace_t g_workspace;

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int expect_u32(uint32_t actual, uint32_t expected, const char *message)
{
    if (actual != expected)
    {
        (void)fprintf(stderr,
                      "FAIL: %s (got %lu expected %lu)\n",
                      message,
                      (unsigned long)actual,
                      (unsigned long)expected);
        return 1;
    }
    return 0;
}

static uint16_t volts_to_raw(float volts)
{
    int raw = (int)((volts / TEST_ADC_SCALE) + 0.5f);
    if (raw < 0)
    {
        raw = 0;
    }
    if (raw > 4095)
    {
        raw = 4095;
    }
    return (uint16_t)raw;
}

static float waveform_value(float vmid_v, measurement_complex_t phasor, float cos_ref, float sin_ref)
{
    return vmid_v + (phasor.re * cos_ref) - (phasor.im * sin_ref);
}

static void step_reference(uint16_t samples_per_cycle, float *cos_ref, float *sin_ref)
{
    const float cos_step = (samples_per_cycle == 64u) ? 0.9951847267f : 0.9238795325f;
    const float sin_step = (samples_per_cycle == 64u) ? 0.0980171403f : 0.3826834324f;
    const float next_cos = (*cos_ref * cos_step) - (*sin_ref * sin_step);
    const float next_sin = (*sin_ref * cos_step) + (*cos_ref * sin_step);
    *cos_ref = next_cos;
    *sin_ref = next_sin;
}

static void make_block(hw_metrology_block_t *block,
                       uint32_t *raw,
                       const app_cal_workflow_request_t *request,
                       measurement_complex_t vexc,
                       measurement_complex_t ret_1x,
                       measurement_complex_t ret_hg_raw)
{
    (void)memset(block, 0, sizeof(*block));
    (void)memset(raw, 0, sizeof(uint32_t) * HW_METROLOGY_RAW_WORD_COUNT);

    hw_metrology_adc_profile_t profile;
    (void)hw_metrology_adc_profile(request->key.frequency, &profile);
    hw_excitation_freq_profile_t freq_profile;
    (void)hw_excitation_freq_profile(request->key.frequency, &freq_profile);

    block->valid = true;
    block->mode = HW_METROLOGY_MODE_DUT_MEASURE;
    block->dut_measure = true;
    block->excitation_frequency_hz = freq_profile.frequency_hz;
    block->requested_amplitude_mvrms = hw_excitation_amplitude_mvrms(request->key.amplitude);
    block->range_id = request->key.range_id;
    block->adc_clock_hz = 12000000u;
    block->sample_time_cycles_x2 = HW_METROLOGY_ADC_SAMPLE_TIME_CYCLES_X2;
    block->sample_rate_hz = profile.sample_rate_hz;
    block->samples_per_cycle = profile.samples_per_cycle;
    block->cycles_per_block = profile.cycles_per_block;
    block->sample_count = HW_METROLOGY_SAMPLES_PER_BLOCK;
    block->words_per_sample = HW_METROLOGY_WORDS_PER_SAMPLE;
    block->raw_words = raw;
    block->dma_complete = true;

    float cos_ref = 1.0f;
    float sin_ref = 0.0f;
    for (uint32_t n = 0u; n < HW_METROLOGY_SAMPLES_PER_BLOCK; n++)
    {
        raw[(3u * n) + 0u] =
            hw_metrology_pack_word(volts_to_raw(waveform_value(TEST_MID_V, vexc, cos_ref, sin_ref)),
                                   volts_to_raw(waveform_value(TEST_MID_V, ret_1x, cos_ref, sin_ref)));
        raw[(3u * n) + 1u] =
            hw_metrology_pack_word(volts_to_raw(waveform_value(TEST_MID_V, vexc, cos_ref, sin_ref)),
                                   volts_to_raw(waveform_value(TEST_MID_V, ret_hg_raw, cos_ref, sin_ref)));
        raw[(3u * n) + 2u] =
            hw_metrology_pack_word(volts_to_raw(TEST_MID_V), volts_to_raw(TEST_MID_V));
        step_reference(block->samples_per_cycle, &cos_ref, &sin_ref);
    }
    hw_metrology_analyze_block(raw, HW_METROLOGY_SAMPLES_PER_BLOCK, block);
}

static measurement_complex_t ret_for_standard(const app_cal_workflow_request_t *request)
{
    const measurement_complex_t source = measurement_complex(0.100f, 0.0f);
    switch (request->standard.type)
    {
    case APP_CAL_STANDARD_OPEN:
        return source;
    case APP_CAL_STANDARD_SHORT:
        return measurement_complex(0.002f, 0.0f);
    case APP_CAL_STANDARD_LOAD:
    default:
        return measurement_complex(0.050f, 0.0f);
    }
}

static bsp_status_t fake_start_capture(const hw_metrology_measure_request_t *request,
                                       uint32_t now_ms,
                                       void *user)
{
    (void)request;
    (void)now_ms;
    fake_phase05_t *fake = (fake_phase05_t *)user;
    if ((fake == NULL) || (fake->service == NULL))
    {
        return BSP_STATUS_ERROR;
    }
    const app_calibration_workflow_t *workflow =
        app_calibration_service_workflow_const(fake->service);
    const app_cal_workflow_request_t *cal_request = &workflow->request;
    const measurement_complex_t source = measurement_complex(0.100f, 0.0f);
    const measurement_complex_t ret = ret_for_standard(cal_request);
    const measurement_complex_t ret_hg_raw =
        measurement_complex(ret.re * TEST_HG_NOMINAL, ret.im * TEST_HG_NOMINAL);
    make_block(&fake->block, fake->raw, cal_request, source, ret, ret_hg_raw);
    fake->start_count++;
    fake->active = true;
    fake->done = false;
    fake->dumpable = true;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_step_capture(uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_phase05_t *fake = (fake_phase05_t *)user;
    if (fake == NULL)
    {
        return BSP_STATUS_ERROR;
    }
    fake->active = false;
    fake->done = true;
    return BSP_STATUS_OK;
}

static bool fake_capture_active(void *user)
{
    return ((const fake_phase05_t *)user)->active;
}

static bool fake_capture_done(void *user)
{
    return ((const fake_phase05_t *)user)->done;
}

static bool fake_capture_dumpable(void *user)
{
    return ((const fake_phase05_t *)user)->dumpable;
}

static const hw_metrology_block_t *fake_capture_block(void *user)
{
    return &((fake_phase05_t *)user)->block;
}

static hw_metrology_measure_error_t fake_capture_error(void *user)
{
    (void)user;
    return HW_METROLOGY_MEASURE_ERR_PERMIT;
}

static void fake_capture_acknowledge(void *user)
{
    fake_phase05_t *fake = (fake_phase05_t *)user;
    fake->active = false;
    fake->done = false;
    fake->dumpable = false;
}

static bsp_status_t fake_capture_abort(void *user)
{
    fake_phase05_t *fake = (fake_phase05_t *)user;
    fake->abort_called = true;
    fake->active = false;
    fake->done = true;
    fake->dumpable = false;
    return BSP_STATUS_BUSY;
}

static app_cal_session_io_t make_io(fake_phase05_t *fake)
{
    return (app_cal_session_io_t){
        .start_capture = fake_start_capture,
        .step_capture = fake_step_capture,
        .capture_active = fake_capture_active,
        .capture_done = fake_capture_done,
        .capture_dumpable = fake_capture_dumpable,
        .capture_block = fake_capture_block,
        .capture_error = fake_capture_error,
        .capture_acknowledge = fake_capture_acknowledge,
        .capture_abort = fake_capture_abort,
        .user = fake,
    };
}

static void init_service(app_calibration_service_t *service)
{
    app_calibration_service_init(service);
    app_io_workspace_init(&g_workspace);
    app_calibration_service_attach_workspace(service, &g_workspace);
    service->storage_available = true;
}

static hw_safety_result_t safety_allowed(void)
{
    return (hw_safety_result_t){
        .measure_allowed = true,
        .primary_blocker = HW_SAFETY_MEASURE_ALLOWED,
    };
}

static int test_condition_enumeration(void)
{
    int failures = 0;
    failures += expect_u32(app_calibration_wizard_total_condition_count(), 33u,
                           "Rev.1 calibratable condition count");
    failures += expect_u32(app_calibration_wizard_condition_count(HW_RANGE_ID_10R), 3u,
                           "10R excludes all 500 mVrms calibration keys");
    failures += expect_u32(app_calibration_wizard_condition_count(HW_RANGE_ID_1K), 6u,
                           "1K covers all frequencies and amplitudes");
    failures += expect_u32(app_calibration_wizard_condition_count(HW_RANGE_ID_1M), 6u,
                           "1M calibration covers all frequencies and amplitudes");

    measurement_cal_key_t key;
    failures += expect_true(app_calibration_wizard_condition_key(HW_RANGE_ID_10R, 0u, &key) == BSP_STATUS_OK,
                            "10R first key exists");
    failures += expect_u32((uint32_t)key.frequency, (uint32_t)HW_EXCITATION_FREQ_100HZ,
                           "10R first key frequency");
    failures += expect_u32((uint32_t)key.amplitude, (uint32_t)HW_EXCITATION_AMP_100MVRMS,
                           "10R first key amplitude");
    failures += expect_true(app_calibration_wizard_condition_key(HW_RANGE_ID_10R, 3u, &key) != BSP_STATUS_OK,
                            "10R fourth key is forbidden");
    return failures;
}

static int test_safety_blocks_capture_without_starting_phase05(void)
{
    int failures = 0;
    app_calibration_service_t service;
    init_service(&service);
    fake_phase05_t fake = {.service = &service};
    app_cal_session_io_t io = make_io(&fake);
    app_calibration_wizard_t wizard;
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    failures += expect_true(app_calibration_wizard_init(&wizard, &service, &io, NULL) == BSP_STATUS_OK,
                            "wizard init");
    failures += expect_true(app_calibration_wizard_start(&wizard, APP_CAL_WIZARD_MODE_MANDATORY,
                                                         1u, 25000, true) == BSP_STATUS_OK,
                            "wizard start");
    app_calibration_wizard_confirm(&wizard);
    hw_safety_result_t safety = {.measure_allowed = false, .primary_blocker = HW_SAFETY_BLOCKED_CHARGER};
    app_calibration_wizard_confirm(&wizard);
    app_calibration_wizard_step(&wizard, &safety, &clock, BSP_STATUS_OK, 2u);
    app_cal_wizard_snapshot_t snapshot;
    app_calibration_wizard_snapshot(&wizard, &snapshot);
    failures += expect_u32((uint32_t)snapshot.state, (uint32_t)APP_CAL_WIZARD_SAFETY_BLOCKED,
                           "wizard surfaces safety block");
    failures += expect_u32(fake.start_count, 0u, "blocked wizard starts no capture");
    return failures;
}

static int test_full_wizard_batches_all_conditions_before_save(void)
{
    int failures = 0;
    app_calibration_service_t service;
    init_service(&service);
    fake_phase05_t fake = {.service = &service};
    app_cal_session_io_t io = make_io(&fake);
    app_calibration_wizard_t wizard;
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    hw_safety_result_t safety = safety_allowed();
    failures += expect_true(app_calibration_wizard_init(&wizard, &service, &io, NULL) == BSP_STATUS_OK,
                            "wizard init full");
    failures += expect_true(app_calibration_wizard_start(&wizard, APP_CAL_WIZARD_MODE_MANUAL,
                                                         7u, 25125, true) == BSP_STATUS_OK,
                            "wizard start full");
    app_calibration_wizard_confirm(&wizard);
    app_calibration_wizard_confirm(&wizard);

    for (uint32_t now = 3u; now < 2000u; now++)
    {
        app_calibration_wizard_step(&wizard, &safety, &clock, BSP_STATUS_OK, now);
        app_cal_wizard_snapshot_t snapshot;
        app_calibration_wizard_snapshot(&wizard, &snapshot);
        if ((snapshot.state == APP_CAL_WIZARD_WAIT_OPEN_FIXTURE) ||
            (snapshot.state == APP_CAL_WIZARD_WAIT_SHORT_FIXTURE) ||
            (snapshot.state == APP_CAL_WIZARD_WAIT_LOAD_FIXTURE))
        {
            app_calibration_wizard_confirm(&wizard);
        }
        else if (snapshot.state == APP_CAL_WIZARD_RANGE_COMPLETE)
        {
            app_calibration_wizard_step(&wizard, &safety, &clock, BSP_STATUS_OK, now + 1u);
        }
        else if (snapshot.state == APP_CAL_WIZARD_CONFIRM_SAVE)
        {
            break;
        }
        else if ((snapshot.state == APP_CAL_WIZARD_FAILED) ||
                 (snapshot.state == APP_CAL_WIZARD_SAFETY_BLOCKED))
        {
            break;
        }
    }

    app_cal_wizard_snapshot_t snapshot;
    app_calibration_wizard_snapshot(&wizard, &snapshot);
    failures += expect_u32((uint32_t)snapshot.state, (uint32_t)APP_CAL_WIZARD_CONFIRM_SAVE,
                           "full wizard reaches save confirmation");
    failures += expect_u32(snapshot.solved_count, 33u, "all 33 records solved");
    failures += expect_u32(snapshot.total_conditions, 33u, "snapshot records total condition count");
    failures += expect_u32(fake.start_count, 33u * 3u * APP_CAL_WORKFLOW_REQUIRED_ACCEPTED,
                           "one capture batch per condition and standard");
    const measurement_cal_set_t *candidate = app_calibration_service_candidate_set_const(&service);
    failures += expect_true(candidate != NULL, "candidate remains dirty before explicit save");
    failures += expect_u32(candidate == NULL ? 0u : candidate->record_count, 33u,
                           "candidate contains one solved record per condition");
    failures += expect_true(app_calibration_service_active_valid(&service) == false,
                            "candidate does not become active before save");
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_condition_enumeration();
    failures += test_safety_blocks_capture_without_starting_phase05();
    failures += test_full_wizard_batches_all_conditions_before_save();
    failures += expect_true(app_calibration_wizard_context_size_bytes() < 2048u,
                            "wizard context remains compact");
    return failures == 0 ? 0 : 1;
}
