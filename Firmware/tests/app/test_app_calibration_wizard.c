#include "app/app_calibration_wizard.h"
#include "app/app_io_workspace.h"
#include "storage/storage_layout.h"

#include <stdio.h>
#include <string.h>

#define TEST_MID_V (1.65f)
#define TEST_ADC_SCALE (3.3f / 4095.0f)
#define TEST_HG_NOMINAL (15.4680851064f)
#define TEST_CAPACITY_BYTES (2u * 1024u * 1024u)
#define TEST_FLASH_BYTES STORAGE_LAYOUT_MUTABLE_RESERVED_BYTES

typedef struct
{
    app_calibration_service_t *service;
    hw_metrology_block_t block;
    uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    app_cal_workflow_request_t requests[256];
    uint32_t start_count;
    uint32_t ack_count;
    uint8_t abort_delay_steps;
    bool active;
    bool done;
    bool dumpable;
    bool abort_called;
} fake_phase05_t;

typedef struct
{
    uint8_t flash[TEST_FLASH_BYTES];
    uint8_t pending[256];
    uint32_t pending_address;
    size_t pending_size;
    uint32_t read_count;
    uint32_t poll_count;
    bool fail_program;
    enum
    {
        FAKE_NOR_IDLE = 0,
        FAKE_NOR_ERASE_BUSY,
        FAKE_NOR_PROGRAM_BUSY,
    } state;
} fake_nor_t;

static app_io_workspace_t g_workspace;

static uint32_t fake_offset(uint32_t address)
{
    return address - (TEST_CAPACITY_BYTES - TEST_FLASH_BYTES);
}

static bsp_status_t fake_nor_read(uint32_t address, void *dst, size_t size, void *user)
{
    fake_nor_t *fake = (fake_nor_t *)user;
    if ((fake == NULL) || (dst == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    const uint32_t offset = fake_offset(address);
    if (((size_t)offset + size) > sizeof(fake->flash))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    (void)memcpy(dst, &fake->flash[offset], size);
    fake->read_count++;
    return BSP_STATUS_OK;
}

static bsp_status_t fake_nor_erase_start(uint32_t address, uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_nor_t *fake = (fake_nor_t *)user;
    if ((fake == NULL) || (fake->state != FAKE_NOR_IDLE))
    {
        return BSP_STATUS_BUSY;
    }
    fake->pending_address = address;
    fake->pending_size = STORAGE_LAYOUT_W25Q_SECTOR_SIZE;
    fake->state = FAKE_NOR_ERASE_BUSY;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_nor_program_start(uint32_t address,
                                           const void *src,
                                           size_t size,
                                           uint32_t now_ms,
                                           void *user)
{
    (void)now_ms;
    fake_nor_t *fake = (fake_nor_t *)user;
    if ((fake == NULL) || (src == NULL) || (size > sizeof(fake->pending)))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (fake->state != FAKE_NOR_IDLE)
    {
        return BSP_STATUS_BUSY;
    }
    fake->pending_address = address;
    fake->pending_size = size;
    (void)memcpy(fake->pending, src, size);
    fake->state = FAKE_NOR_PROGRAM_BUSY;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_nor_poll(uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_nor_t *fake = (fake_nor_t *)user;
    if (fake == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    fake->poll_count++;
    const uint32_t offset = fake_offset(fake->pending_address);
    if (fake->state == FAKE_NOR_ERASE_BUSY)
    {
        if (((size_t)offset + fake->pending_size) > sizeof(fake->flash))
        {
            fake->state = FAKE_NOR_IDLE;
            return BSP_STATUS_INVALID_ARG;
        }
        (void)memset(&fake->flash[offset], 0xFF, fake->pending_size);
        fake->state = FAKE_NOR_IDLE;
        return BSP_STATUS_OK;
    }
    if (fake->state == FAKE_NOR_PROGRAM_BUSY)
    {
        if (fake->fail_program)
        {
            fake->state = FAKE_NOR_IDLE;
            return BSP_STATUS_ERROR;
        }
        if (((size_t)offset + fake->pending_size) > sizeof(fake->flash))
        {
            fake->state = FAKE_NOR_IDLE;
            return BSP_STATUS_INVALID_ARG;
        }
        for (size_t i = 0u; i < fake->pending_size; i++)
        {
            fake->flash[offset + i] &= fake->pending[i];
        }
        fake->state = FAKE_NOR_IDLE;
        return BSP_STATUS_OK;
    }
    return BSP_STATUS_OK;
}

static measurement_cal_store_io_t fake_nor_io(fake_nor_t *fake)
{
    return (measurement_cal_store_io_t){
        .read = fake_nor_read,
        .erase_sector_start = fake_nor_erase_start,
        .program_start = fake_nor_program_start,
        .poll = fake_nor_poll,
        .user = fake,
    };
}

static void init_blank_nor(fake_nor_t *fake)
{
    (void)memset(fake, 0, sizeof(*fake));
    (void)memset(fake->flash, 0xFF, sizeof(fake->flash));
}

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
    if (fake->start_count < (uint32_t)(sizeof(fake->requests) / sizeof(fake->requests[0])))
    {
        fake->requests[fake->start_count] = *cal_request;
    }
    const measurement_complex_t source = measurement_complex(0.100f, 0.0f);
    const measurement_complex_t ret = ret_for_standard(cal_request);
    const measurement_complex_t ret_hg_raw =
        measurement_complex(ret.re * TEST_HG_NOMINAL, ret.im * TEST_HG_NOMINAL);
    make_block(&fake->block, fake->raw, cal_request, source, ret, ret_hg_raw);
    fake->start_count++;
    fake->active = true;
    fake->done = false;
    (void)app_io_workspace_acquire(&g_workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
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
    if (fake->abort_called)
    {
        if (fake->abort_delay_steps > 0u)
        {
            fake->abort_delay_steps--;
            return BSP_STATUS_BUSY;
        }
        fake->active = false;
        fake->done = true;
        fake->dumpable = false;
        return BSP_STATUS_OK;
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
    fake->ack_count++;
    fake->active = false;
    fake->done = false;
    fake->dumpable = false;
    if (app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_METROLOGY)
    {
        (void)app_io_workspace_release(&g_workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
    }
}

static bsp_status_t fake_capture_abort(void *user)
{
    fake_phase05_t *fake = (fake_phase05_t *)user;
    fake->abort_called = true;
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

static int32_t temperature_for_standard(app_cal_standard_type_t standard)
{
    switch (standard)
    {
    case APP_CAL_STANDARD_OPEN:
        return 24000;
    case APP_CAL_STANDARD_SHORT:
        return 25500;
    case APP_CAL_STANDARD_LOAD:
    default:
        return 27000;
    }
}

static void drive_wizard_one_step(app_calibration_wizard_t *wizard,
                                  const hw_safety_result_t *safety,
                                  const bsp_clock_summary_t *clock,
                                  uint32_t now)
{
    app_cal_wizard_snapshot_t snapshot;
    app_calibration_wizard_snapshot(wizard, &snapshot);
    app_calibration_wizard_step(wizard,
                                safety,
                                clock,
                                BSP_STATUS_OK,
                                temperature_for_standard(snapshot.standard),
                                true,
                                now);
}

static bool drive_wizard_to_confirm_save(app_calibration_wizard_t *wizard,
                                         const hw_safety_result_t *safety,
                                         const bsp_clock_summary_t *clock,
                                         uint32_t start_ms,
                                         uint32_t limit_ms)
{
    for (uint32_t now = start_ms; now < limit_ms; now++)
    {
        drive_wizard_one_step(wizard, safety, clock, now);
        app_cal_wizard_snapshot_t snapshot;
        app_calibration_wizard_snapshot(wizard, &snapshot);
        if ((snapshot.state == APP_CAL_WIZARD_WAIT_OPEN_FIXTURE) ||
            (snapshot.state == APP_CAL_WIZARD_WAIT_SHORT_FIXTURE) ||
            (snapshot.state == APP_CAL_WIZARD_WAIT_LOAD_FIXTURE))
        {
            app_calibration_wizard_confirm(wizard);
        }
        else if (snapshot.state == APP_CAL_WIZARD_RANGE_COMPLETE)
        {
            drive_wizard_one_step(wizard, safety, clock, now + 1u);
        }
        else if (snapshot.state == APP_CAL_WIZARD_CONFIRM_SAVE)
        {
            return true;
        }
        else if ((snapshot.state == APP_CAL_WIZARD_FAILED) ||
                 (snapshot.state == APP_CAL_WIZARD_SAFETY_BLOCKED))
        {
            return false;
        }
    }
    return false;
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
    app_calibration_wizard_step(&wizard, &safety, &clock, BSP_STATUS_OK, 24000, true, 2u);
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
        app_calibration_wizard_step(&wizard, &safety, &clock, BSP_STATUS_OK, 25125, true, now);
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
            app_calibration_wizard_step(&wizard, &safety, &clock, BSP_STATUS_OK, 25125, true, now + 1u);
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

static int test_distinct_standard_temperatures_reach_solver(void)
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
                            "wizard init temperature provenance");
    failures += expect_true(app_calibration_wizard_start(&wizard, APP_CAL_WIZARD_MODE_MANUAL,
                                                         8u, 23000, true) == BSP_STATUS_OK,
                            "wizard start temperature provenance");
    app_calibration_wizard_confirm(&wizard);
    app_calibration_wizard_confirm(&wizard);

    for (uint32_t now = 3u; now < 1000u; now++)
    {
        drive_wizard_one_step(&wizard, &safety, &clock, now);
        app_cal_wizard_snapshot_t snapshot;
        app_calibration_wizard_snapshot(&wizard, &snapshot);
        if ((snapshot.state == APP_CAL_WIZARD_WAIT_OPEN_FIXTURE) ||
            (snapshot.state == APP_CAL_WIZARD_WAIT_SHORT_FIXTURE) ||
            (snapshot.state == APP_CAL_WIZARD_WAIT_LOAD_FIXTURE))
        {
            app_calibration_wizard_confirm(&wizard);
        }
        if (snapshot.solved_count > 0u)
        {
            failures += expect_true(snapshot.temperature_span_valid,
                                    "O/S/L temperature span is valid");
            failures += expect_u32((uint32_t)snapshot.open_temperature_mC, 24000u,
                                   "OPEN uses pre-workflow temperature");
            failures += expect_u32((uint32_t)snapshot.short_temperature_mC, 25500u,
                                   "SHORT uses pre-workflow temperature");
            failures += expect_u32((uint32_t)snapshot.load_temperature_mC, 27000u,
                                   "LOAD uses pre-workflow temperature");
            failures += expect_u32((uint32_t)snapshot.temperature_span_mC, 3000u,
                                   "temperature span reports O/S/L spread");
            return failures;
        }
        if (snapshot.state == APP_CAL_WIZARD_FAILED)
        {
            break;
        }
    }
    failures += expect_true(false, "wizard should solve at least one condition");
    return failures;
}

static bsp_status_t complex_fixture(hw_range_id_t range_id,
                                    hw_excitation_freq_t frequency,
                                    measurement_complex_t *z_ohms,
                                    void *user)
{
    (void)range_id;
    (void)user;
    if (z_ohms == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (frequency == HW_EXCITATION_FREQ_1KHZ)
    {
        *z_ohms = measurement_complex(1000.0f, 0.25f);
    }
    else if (frequency == HW_EXCITATION_FREQ_10KHZ)
    {
        *z_ohms = measurement_complex(999.7f, -2.1f);
    }
    else
    {
        *z_ohms = measurement_complex(1000.0f, 0.0f);
    }
    return BSP_STATUS_OK;
}

static int test_complex_load_profile_reaches_capture_request(void)
{
    int failures = 0;
    app_calibration_service_t service;
    init_service(&service);
    fake_phase05_t fake = {.service = &service};
    app_cal_session_io_t io = make_io(&fake);
    const app_cal_fixture_profile_t fixture = {.load_z = complex_fixture, .user = NULL};
    app_calibration_wizard_t wizard;
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    hw_safety_result_t safety = safety_allowed();
    failures += expect_true(app_calibration_wizard_init(&wizard, &service, &io, &fixture) == BSP_STATUS_OK,
                            "wizard init complex fixture");
    failures += expect_true(app_calibration_wizard_start(&wizard, APP_CAL_WIZARD_MODE_MANUAL,
                                                         9u, 25000, true) == BSP_STATUS_OK,
                            "wizard start complex fixture");
    app_calibration_wizard_confirm(&wizard);
    app_calibration_wizard_confirm(&wizard);
    for (uint32_t now = 3u; now < 1200u; now++)
    {
        drive_wizard_one_step(&wizard, &safety, &clock, now);
        app_cal_wizard_snapshot_t snapshot;
        app_calibration_wizard_snapshot(&wizard, &snapshot);
        if ((snapshot.state == APP_CAL_WIZARD_WAIT_OPEN_FIXTURE) ||
            (snapshot.state == APP_CAL_WIZARD_WAIT_SHORT_FIXTURE) ||
            (snapshot.state == APP_CAL_WIZARD_WAIT_LOAD_FIXTURE))
        {
            app_calibration_wizard_confirm(&wizard);
        }
        if ((snapshot.standard == APP_CAL_STANDARD_LOAD) && (snapshot.condition_index >= 3u))
        {
            break;
        }
    }
    bool saw_1k = false;
    bool saw_10k = false;
    for (uint32_t i = 0u; i < fake.start_count; i++)
    {
        if (fake.requests[i].standard.type != APP_CAL_STANDARD_LOAD)
        {
            continue;
        }
        if (fake.requests[i].key.frequency == HW_EXCITATION_FREQ_1KHZ)
        {
            saw_1k = true;
            failures += expect_true((fake.requests[i].standard.z_ohms.re > 999.9f) &&
                                        (fake.requests[i].standard.z_ohms.im > 0.24f),
                                    "1 kHz complex load reaches workflow request");
        }
        if (fake.requests[i].key.frequency == HW_EXCITATION_FREQ_10KHZ)
        {
            saw_10k = true;
            failures += expect_true((fake.requests[i].standard.z_ohms.re < 999.8f) &&
                                        (fake.requests[i].standard.z_ohms.im < -2.0f),
                                    "10 kHz complex load reaches workflow request");
        }
    }
    failures += expect_true(saw_1k, "complex fixture saw 1 kHz LOAD");
    failures += expect_true(saw_10k, "complex fixture saw 10 kHz LOAD");
    return failures;
}

static bsp_status_t failing_fixture(hw_range_id_t range_id,
                                    hw_excitation_freq_t frequency,
                                    measurement_complex_t *z_ohms,
                                    void *user)
{
    (void)range_id;
    (void)frequency;
    (void)z_ohms;
    (void)user;
    return BSP_STATUS_ERROR;
}

static int test_fixture_failure_starts_no_load_capture(void)
{
    int failures = 0;
    app_calibration_service_t service;
    init_service(&service);
    fake_phase05_t fake = {.service = &service};
    app_cal_session_io_t io = make_io(&fake);
    const app_cal_fixture_profile_t fixture = {.load_z = failing_fixture, .user = NULL};
    app_calibration_wizard_t wizard;
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    hw_safety_result_t safety = safety_allowed();
    failures += expect_true(app_calibration_wizard_init(&wizard, &service, &io, &fixture) == BSP_STATUS_OK,
                            "wizard init failing fixture");
    failures += expect_true(app_calibration_wizard_start(&wizard, APP_CAL_WIZARD_MODE_MANUAL,
                                                         10u, 25000, true) == BSP_STATUS_OK,
                            "wizard start failing fixture");
    app_calibration_wizard_confirm(&wizard);
    app_calibration_wizard_confirm(&wizard);
    uint32_t starts_before_load = 0u;
    for (uint32_t now = 3u; now < 1000u; now++)
    {
        drive_wizard_one_step(&wizard, &safety, &clock, now);
        app_cal_wizard_snapshot_t snapshot;
        app_calibration_wizard_snapshot(&wizard, &snapshot);
        if (snapshot.state == APP_CAL_WIZARD_WAIT_LOAD_FIXTURE)
        {
            starts_before_load = fake.start_count;
            app_calibration_wizard_confirm(&wizard);
        }
        else if ((snapshot.state == APP_CAL_WIZARD_WAIT_OPEN_FIXTURE) ||
                 (snapshot.state == APP_CAL_WIZARD_WAIT_SHORT_FIXTURE))
        {
            app_calibration_wizard_confirm(&wizard);
        }
        if (snapshot.state == APP_CAL_WIZARD_FAILED)
        {
            failures += expect_u32((uint32_t)snapshot.error,
                                   (uint32_t)APP_CAL_WIZARD_ERROR_CONDITION,
                                   "fixture failure is a condition failure");
            failures += expect_u32(fake.start_count, starts_before_load,
                                   "fixture failure starts no LOAD capture");
            return failures;
        }
    }
    failures += expect_true(false, "failing fixture should fail before LOAD capture");
    return failures;
}

static int test_candidate_incomplete_retry_does_not_start_invalid_range(void)
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
                            "wizard init incomplete retry");
    failures += expect_true(app_calibration_wizard_start(&wizard, APP_CAL_WIZARD_MODE_MANUAL,
                                                         11u, 25000, true) == BSP_STATUS_OK,
                            "wizard start incomplete retry");
    wizard.state = APP_CAL_WIZARD_FAILED;
    wizard.error = APP_CAL_WIZARD_ERROR_CANDIDATE_INCOMPLETE;
    wizard.range_index = APP_CAL_WIZARD_RANGE_COUNT;
    wizard.condition_index = 0u;
    app_calibration_wizard_confirm(&wizard);
    drive_wizard_one_step(&wizard, &safety, &clock, 12u);
    app_cal_wizard_snapshot_t snapshot;
    app_calibration_wizard_snapshot(&wizard, &snapshot);
    failures += expect_u32((uint32_t)snapshot.state, (uint32_t)APP_CAL_WIZARD_FAILED,
                           "candidate-incomplete retry remains failed");
    failures += expect_u32(fake.start_count, 0u,
                           "candidate-incomplete retry starts no invalid range capture");
    return failures;
}

static int test_full_wizard_save_activates_with_single_external_service_stepper(void)
{
    int failures = 0;
    fake_nor_t nor;
    init_blank_nor(&nor);
    app_calibration_service_t service;
    init_service(&service);
    measurement_cal_store_io_t store_io = fake_nor_io(&nor);
    (void)app_calibration_service_load(&service, &store_io, TEST_CAPACITY_BYTES);
    service.storage_available = true;
    failures += expect_true(!app_calibration_service_active_valid(&service),
                            "blank NOR has no active calibration");
    fake_phase05_t fake = {.service = &service};
    app_cal_session_io_t io = make_io(&fake);
    app_calibration_wizard_t wizard;
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    hw_safety_result_t safety = safety_allowed();
    failures += expect_true(app_calibration_wizard_init(&wizard, &service, &io, NULL) == BSP_STATUS_OK,
                            "wizard init save");
    failures += expect_true(app_calibration_wizard_start(&wizard, APP_CAL_WIZARD_MODE_MANDATORY,
                                                         12u, 25000, true) == BSP_STATUS_OK,
                            "wizard start save");
    app_calibration_wizard_confirm(&wizard);
    app_calibration_wizard_confirm(&wizard);
    failures += expect_true(drive_wizard_to_confirm_save(&wizard, &safety, &clock, 3u, 3000u),
                            "wizard reaches save before commit");
    failures += expect_true(app_calibration_service_active_valid(&service) == false,
                            "no active set before save");

    app_calibration_wizard_confirm(&wizard);
    app_cal_wizard_snapshot_t snapshot;
    app_calibration_wizard_snapshot(&wizard, &snapshot);
    failures += expect_u32((uint32_t)snapshot.state, (uint32_t)APP_CAL_WIZARD_COMMITTING,
                           "wizard enters committing");
    for (uint32_t i = 0u; i < 5u; i++)
    {
        drive_wizard_one_step(&wizard, &safety, &clock, 4000u + i);
    }
    failures += expect_true(app_calibration_service_active_valid(&service) == false,
                            "wizard does not step service/store internally");
    uint32_t external_steps = 0u;
    for (uint32_t now = 4010u; now < 4300u; now++)
    {
        external_steps++;
        (void)app_calibration_service_step(&service, now);
        drive_wizard_one_step(&wizard, &safety, &clock, now);
        if (wizard.state == APP_CAL_WIZARD_COMPLETE)
        {
            break;
        }
    }
    failures += expect_true(external_steps != 0u, "external service stepper ran");
    failures += expect_u32((uint32_t)wizard.state, (uint32_t)APP_CAL_WIZARD_COMPLETE,
                           "wizard observes activated calibration");
    const measurement_cal_set_t *active = app_calibration_service_active_set(&service);
    failures += expect_true((active != NULL) && (active->record_count == 33u),
                            "active set contains all 33 records");
    failures += expect_true(app_io_workspace_owner(&g_workspace) == APP_IO_WORKSPACE_OWNER_FREE,
                            "workspace free after commit activation");

    app_calibration_service_t rebooted;
    init_service(&rebooted);
    failures += expect_true(app_calibration_service_load(&rebooted, &store_io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                            "reboot loads committed wizard calibration");
    const measurement_cal_set_t *loaded = app_calibration_service_active_set(&rebooted);
    failures += expect_true((loaded != NULL) && (loaded->record_count == 33u),
                            "rebooted active set contains all 33 records");
    return failures;
}

static int test_commit_failure_can_retry_with_external_service_stepper(void)
{
    int failures = 0;
    fake_nor_t nor;
    init_blank_nor(&nor);
    app_calibration_service_t service;
    init_service(&service);
    measurement_cal_store_io_t store_io = fake_nor_io(&nor);
    (void)app_calibration_service_load(&service, &store_io, TEST_CAPACITY_BYTES);
    service.storage_available = true;
    fake_phase05_t fake = {.service = &service};
    app_cal_session_io_t io = make_io(&fake);
    app_calibration_wizard_t wizard;
    const bsp_clock_summary_t clock = {.source = BSP_CLOCK_SOURCE_HSE_PLL,
                                       .sysclk_hz = 72000000u,
                                       .hse_ready = true};
    hw_safety_result_t safety = safety_allowed();
    failures += expect_true(app_calibration_wizard_init(&wizard, &service, &io, NULL) == BSP_STATUS_OK,
                            "wizard init retrying commit");
    failures += expect_true(app_calibration_wizard_start(&wizard, APP_CAL_WIZARD_MODE_MANDATORY,
                                                         13u, 25000, true) == BSP_STATUS_OK,
                            "wizard start retrying commit");
    app_calibration_wizard_confirm(&wizard);
    app_calibration_wizard_confirm(&wizard);
    failures += expect_true(drive_wizard_to_confirm_save(&wizard, &safety, &clock, 3u, 3000u),
                            "wizard reaches save before failed commit");

    nor.fail_program = true;
    app_calibration_wizard_confirm(&wizard);
    for (uint32_t now = 4000u; now < 4300u; now++)
    {
        (void)app_calibration_service_step(&service, now);
        drive_wizard_one_step(&wizard, &safety, &clock, now);
        if (wizard.state == APP_CAL_WIZARD_FAILED)
        {
            break;
        }
    }
    failures += expect_u32((uint32_t)wizard.state, (uint32_t)APP_CAL_WIZARD_FAILED,
                           "program failure reaches wizard failed state");
    failures += expect_u32((uint32_t)wizard.error, (uint32_t)APP_CAL_WIZARD_ERROR_COMMIT,
                           "program failure is reported as commit failure");
    failures += expect_true(!app_calibration_service_active_valid(&service),
                            "failed commit does not activate calibration");
    failures += expect_u32((uint32_t)app_io_workspace_owner(&g_workspace),
                           (uint32_t)APP_IO_WORKSPACE_OWNER_CALIBRATION_STORE,
                           "failed commit holds store workspace until terminal state is acknowledged");

    nor.fail_program = false;
    app_calibration_wizard_confirm(&wizard);
    failures += expect_u32((uint32_t)wizard.state, (uint32_t)APP_CAL_WIZARD_COMMITTING,
                           "commit failure confirm restarts commit");
    for (uint32_t now = 4310u; now < 4600u; now++)
    {
        (void)app_calibration_service_step(&service, now);
        drive_wizard_one_step(&wizard, &safety, &clock, now);
        if (wizard.state == APP_CAL_WIZARD_COMPLETE)
        {
            break;
        }
    }
    failures += expect_u32((uint32_t)wizard.state, (uint32_t)APP_CAL_WIZARD_COMPLETE,
                           "retrying commit activates calibration");
    const measurement_cal_set_t *active = app_calibration_service_active_set(&service);
    failures += expect_true((active != NULL) && (active->record_count == 33u),
                            "retry commit active set contains all records");
    failures += expect_u32((uint32_t)app_io_workspace_owner(&g_workspace),
                           (uint32_t)APP_IO_WORKSPACE_OWNER_FREE,
                           "retry commit releases store workspace");
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_condition_enumeration();
    failures += test_safety_blocks_capture_without_starting_phase05();
    failures += test_full_wizard_batches_all_conditions_before_save();
    failures += test_distinct_standard_temperatures_reach_solver();
    failures += test_complex_load_profile_reaches_capture_request();
    failures += test_fixture_failure_starts_no_load_capture();
    failures += test_candidate_incomplete_retry_does_not_start_invalid_range();
    failures += test_full_wizard_save_activates_with_single_external_service_stepper();
    failures += test_commit_failure_can_retry_with_external_service_stepper();
    failures += expect_true(app_calibration_wizard_context_size_bytes() < 2048u,
                            "wizard context remains compact");
    return failures == 0 ? 0 : 1;
}
