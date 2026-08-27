#include "app/app_calibration_service.h"
#include "app/app_calibration_session.h"
#include "app/app_calibration_campaign.h"
#include "storage/storage_layout.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define TEST_CAPACITY_BYTES (2u * 1024u * 1024u)
#define TEST_MUTABLE_BASE (TEST_CAPACITY_BYTES - STORAGE_LAYOUT_MUTABLE_RESERVED_BYTES)
#define TEST_FLASH_BYTES STORAGE_LAYOUT_MUTABLE_RESERVED_BYTES
#define TEST_MID_V (1.65f)
#define TEST_ADC_SCALE (3.3f / 4095.0f)
#define TEST_HG_NOMINAL (15.4680851064f)

typedef struct
{
    uint8_t flash[TEST_FLASH_BYTES];
    uint8_t pending[256];
    uint32_t pending_address;
    size_t pending_size;
    uint8_t read_count;
    uint8_t busy_polls;
    bool initialized;
    bool fail_program;
    enum
    {
        FAKE_STORE_IDLE = 0,
        FAKE_STORE_ERASE_BUSY,
        FAKE_STORE_PROGRAM_BUSY,
    } state;
} fake_store_t;

typedef struct
{
    const hw_metrology_block_t *blocks[APP_CAL_WORKFLOW_MAX_ATTEMPTS];
    const hw_metrology_block_t *current;
    uint8_t block_count;
    uint8_t start_count;
    uint8_t step_count;
    bool active;
    bool done;
    bool dumpable;
    bool abort_called;
    bool start_error;
    bool fail_capture;
    hw_metrology_measure_error_t error;
} fake_phase05_t;

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

static float abs_f(float value)
{
    return (value < 0.0f) ? -value : value;
}

static int expect_near(float actual, float expected, float tolerance, const char *message)
{
    if (abs_f(actual - expected) > tolerance)
    {
        (void)fprintf(stderr,
                      "FAIL: %s (got %.7g expected %.7g tol %.7g)\n",
                      message,
                      (double)actual,
                      (double)expected,
                      (double)tolerance);
        return 1;
    }
    return 0;
}

static int expect_complex_near(measurement_complex_t actual,
                               measurement_complex_t expected,
                               float tolerance,
                               const char *message)
{
    int failures = 0;
    failures += expect_near(actual.re, expected.re, tolerance, message);
    failures += expect_near(actual.im, expected.im, tolerance, message);
    return failures;
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
                       measurement_complex_t vexc_1,
                       measurement_complex_t ret_1x,
                       measurement_complex_t vexc_2,
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
            hw_metrology_pack_word(volts_to_raw(waveform_value(TEST_MID_V, vexc_1, cos_ref, sin_ref)),
                                   volts_to_raw(waveform_value(TEST_MID_V, ret_1x, cos_ref, sin_ref)));
        raw[(3u * n) + 1u] =
            hw_metrology_pack_word(volts_to_raw(waveform_value(TEST_MID_V, vexc_2, cos_ref, sin_ref)),
                                   volts_to_raw(waveform_value(TEST_MID_V, ret_hg_raw, cos_ref, sin_ref)));
        raw[(3u * n) + 2u] =
            hw_metrology_pack_word(volts_to_raw(TEST_MID_V), volts_to_raw(TEST_MID_V));
        step_reference(block->samples_per_cycle, &cos_ref, &sin_ref);
    }
    hw_metrology_analyze_block(raw, HW_METROLOGY_SAMPLES_PER_BLOCK, block);
}

static measurement_cal_key_t key_for(hw_range_id_t range,
                                     hw_excitation_freq_t frequency,
                                     hw_excitation_amp_t amplitude)
{
    return measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                               MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                               range,
                               frequency,
                               amplitude);
}

static app_cal_workflow_request_t request_for(app_cal_standard_type_t type)
{
    app_cal_workflow_request_t request = {
        .key = key_for(HW_RANGE_ID_1K, HW_EXCITATION_FREQ_1KHZ, HW_EXCITATION_AMP_100MVRMS),
        .standard = {
            .type = type,
            .z_ohms = measurement_complex(1000.0f, 0.0f),
            .z_valid = type == APP_CAL_STANDARD_LOAD,
        },
        .temperature_mC = 25000,
        .temperature_valid = true,
    };
    return request;
}

static app_cal_capture_sample_t sample_for(const app_cal_workflow_request_t *request,
                                           measurement_complex_t z,
                                           uint32_t reject_flags)
{
    app_cal_capture_sample_t sample = {
        .key = request->key,
        .standard_type = request->standard.type,
        .timestamp_ms = 100u,
        .temperature_mC = request->temperature_mC,
        .temperature_valid = request->temperature_valid,
        .source_v = measurement_complex(0.100f, 0.0f),
        .vexc_1_v = measurement_complex(0.100f, 0.0f),
        .vexc_2_v = measurement_complex(0.100f, 0.0f),
        .ret_1x_v = measurement_complex(0.050f, 0.0f),
        .ret_hg_raw_v = measurement_complex(0.050f, 0.0f),
        .ret_hg_reconstructed_v = measurement_complex(0.050f, 0.0f),
        .ret_hg_v = measurement_complex(0.050f, 0.0f),
        .vmid_adc1_v = measurement_complex(0.0f, 0.0f),
        .vmid_adc2_v = measurement_complex(0.0f, 0.0f),
        .open_y_1x = measurement_complex(0.01f, 0.0f),
        .open_y_hg = measurement_complex(0.01f, 0.0f),
        .hg_observed_transfer = measurement_complex(1.0f, 0.0f),
        .z_1x_ohms = z,
        .z_hg_ohms = z,
        .source_peak_v = 0.100f,
        .vexc_1_peak_v = 0.100f,
        .vexc_2_peak_v = 0.100f,
        .ret_1x_peak_v = 0.050f,
        .ret_hg_raw_peak_v = 0.050f,
        .ret_hg_reconstructed_peak_v = 0.050f,
        .ret_hg_peak_v = 0.050f,
        .denominator_1x_peak_v = 0.050f,
        .denominator_hg_peak_v = 0.050f,
        .ret_1x_usable = true,
        .ret_hg_usable = true,
        .open_y_1x_valid = request->standard.type == APP_CAL_STANDARD_OPEN,
        .open_y_hg_valid = request->standard.type == APP_CAL_STANDARD_OPEN,
        .hg_overlap_valid = true,
        .z_1x_valid = true,
        .z_hg_valid = true,
        .clipped = false,
        .reject_flags = reject_flags,
    };
    return sample;
}

static int feed_sample(app_calibration_workflow_t *workflow, const app_cal_capture_sample_t *sample)
{
    measurement_cal_key_t key = {0};
    int failures = 0;
    failures += expect_true(app_calibration_workflow_capture_pending(workflow), "capture should be pending");
    failures += expect_true(app_calibration_workflow_capture_request(workflow, &key) == BSP_STATUS_OK,
                            "capture request should be readable");
    failures += expect_true(app_calibration_workflow_mark_capture_started(workflow) == BSP_STATUS_OK,
                            "capture should start");
    (void)app_calibration_workflow_submit_sample(workflow, sample);
    return failures;
}

static int test_clean_load_completes(void)
{
    int failures = 0;
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    failures += expect_true(app_calibration_workflow_start(&workflow, &request, 7u) == BSP_STATUS_BUSY,
                            "valid load workflow should start");
    const app_cal_capture_sample_t sample = sample_for(&request, measurement_complex(1000.0f, 0.0f),
                                                       APP_CAL_REJECT_NONE);
    for (uint8_t i = 0u; i < APP_CAL_WORKFLOW_REQUIRED_ACCEPTED; i++)
    {
        failures += feed_sample(&workflow, &sample);
    }
    const app_cal_evidence_t *evidence = app_calibration_workflow_evidence(&workflow);
    failures += expect_u32((uint32_t)app_calibration_workflow_state(&workflow),
                           (uint32_t)APP_CAL_WORKFLOW_COMPLETE,
                           "load workflow should complete");
    failures += expect_u32(evidence->accepted, APP_CAL_WORKFLOW_REQUIRED_ACCEPTED,
                           "load accepted count");
    failures += expect_true(evidence->stable, "load evidence should be stable");
    return failures;
}

static int test_open_accepts_phasor_evidence_without_forced_z(void)
{
    int failures = 0;
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_OPEN);
    app_cal_capture_sample_t sample = sample_for(&request, measurement_complex(0.0f, 0.0f), APP_CAL_REJECT_NONE);
    sample.z_1x_valid = false;
    sample.z_hg_valid = false;
    failures += expect_true(app_calibration_workflow_start(&workflow, &request, 1u) == BSP_STATUS_BUSY,
                            "valid open workflow should start");
    for (uint8_t i = 0u; i < APP_CAL_WORKFLOW_REQUIRED_ACCEPTED; i++)
    {
        failures += feed_sample(&workflow, &sample);
    }
    failures += expect_u32((uint32_t)app_calibration_workflow_result(&workflow),
                           (uint32_t)APP_CAL_WORKFLOW_RESULT_OK,
                           "open phasor evidence should complete");
    return failures;
}

static int test_raw_open_block_accepts_denominator_singularity(void)
{
    int failures = 0;
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_OPEN);
    hw_metrology_block_t block;
    static uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    const measurement_complex_t source = measurement_complex(0.100f, 0.0f);
    make_block(&block,
               raw,
               &request,
               source,
               source,
               source,
               measurement_complex(TEST_HG_NOMINAL * source.re, 0.0f));

    app_cal_capture_sample_t sample;
    failures += expect_true(app_calibration_workflow_sample_from_block(&block, &request, &sample) == BSP_STATUS_OK,
                            "OPEN raw block should still produce calibration evidence");
    failures += expect_true(sample.open_y_1x_valid, "OPEN 1X normalized admittance valid");
    failures += expect_true(sample.open_y_hg_valid, "OPEN HG normalized admittance valid");
    failures += expect_complex_near(sample.open_y_1x, measurement_complex(0.0f, 0.0f), 0.0015f, "OPEN 1X y");
    failures += expect_complex_near(sample.open_y_hg, measurement_complex(0.0f, 0.0f), 0.0025f, "OPEN HG y");
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    (void)app_calibration_workflow_start(&workflow, &request, 20u);
    failures += feed_sample(&workflow, &sample);
    failures += expect_u32(app_calibration_workflow_last_reject_flags(&workflow),
                           APP_CAL_REJECT_NONE,
                           "OPEN evidence must not be rejected as final-Z DSP failure");
    return failures;
}

static int test_raw_load_block_extracts_independent_hg_source_path(void)
{
    int failures = 0;
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    hw_metrology_block_t block;
    static uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    make_block(&block,
               raw,
               &request,
               measurement_complex(0.100f, 0.0f),
               measurement_complex(0.050f, 0.0f),
               measurement_complex(0.080f, 0.0f),
               measurement_complex(0.050f * TEST_HG_NOMINAL, 0.0f));

    app_cal_capture_sample_t sample;
    failures += expect_true(app_calibration_workflow_sample_from_block(&block, &request, &sample) == BSP_STATUS_OK,
                            "LOAD raw block should produce evidence");
    failures += expect_true(sample.z_1x_valid, "1X Z valid");
    failures += expect_true(sample.z_hg_valid, "HG Z valid");
    failures += expect_near(sample.denominator_1x_peak_v, 0.050f, 0.0015f, "1X denominator uses VEXC1");
    failures += expect_near(sample.denominator_hg_peak_v, 0.030f, 0.0020f, "HG denominator uses VEXC2");
    failures += expect_near(sample.z_1x_ohms.re, 1000.0f, 45.0f, "1X provisional Z");
    failures += expect_near(sample.z_hg_ohms.re, 1666.7f, 90.0f, "HG provisional Z uses VEXC2");
    return failures;
}

static int test_raw_hg_observed_transfer_preserves_nonideal_gain(void)
{
    int failures = 0;
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    hw_metrology_block_t block;
    static uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    const measurement_complex_t vexc_1 = measurement_complex(0.100f, 0.0f);
    const measurement_complex_t vexc_2 = measurement_complex(0.083f, 0.006f);
    const measurement_complex_t ret_1x = measurement_complex(0.006f, 0.002f);
    const measurement_complex_t h_hg = measurement_complex(12.0f, 1.5f);
    measurement_complex_t t_1x = measurement_complex(0.0f, 0.0f);
    (void)measurement_complex_div(ret_1x, vexc_1, &t_1x);
    const measurement_complex_t ret_hg_raw =
        measurement_complex_mul(measurement_complex_mul(t_1x, vexc_2), h_hg);
    make_block(&block,
               raw,
               &request,
               vexc_1,
               ret_1x,
               vexc_2,
               ret_hg_raw);

    app_cal_capture_sample_t sample;
    failures += expect_true(app_calibration_workflow_sample_from_block(&block, &request, &sample) == BSP_STATUS_OK,
                            "nonideal HG block should produce evidence");
    failures += expect_true(sample.hg_overlap_valid, "HG overlap should be valid");
    failures += expect_complex_near(sample.hg_observed_transfer, h_hg, 0.75f, "observed effective HG transfer");
    return failures;
}

static int test_clipped_hg_does_not_reject_good_1x_path(void)
{
    int failures = 0;
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    hw_metrology_block_t block;
    static uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    make_block(&block,
               raw,
               &request,
               measurement_complex(0.100f, 0.0f),
               measurement_complex(0.050f, 0.0f),
               measurement_complex(0.100f, 0.0f),
               measurement_complex(1.700f, 0.0f));

    app_cal_capture_sample_t sample;
    failures += expect_true(app_calibration_workflow_sample_from_block(&block, &request, &sample) == BSP_STATUS_OK,
                            "HG-clipped block should still decode");
    failures += expect_true(sample.ret_1x_usable, "1X remains usable");
    failures += expect_true(!sample.ret_hg_usable, "HG rejected when raw path clips");
    failures += expect_true(sample.ret_hg_clipped, "HG path clipping preserved");
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    (void)app_calibration_workflow_start(&workflow, &request, 21u);
    failures += feed_sample(&workflow, &sample);
    failures += expect_u32(app_calibration_workflow_last_reject_flags(&workflow),
                           APP_CAL_REJECT_NONE,
                           "good 1X path should not be globally rejected by HG clipping");
    return failures;
}

static int test_clean_short_completes(void)
{
    int failures = 0;
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_SHORT);
    const app_cal_capture_sample_t sample = sample_for(&request, measurement_complex(0.03f, 0.01f),
                                                       APP_CAL_REJECT_NONE);
    failures += expect_true(app_calibration_workflow_start(&workflow, &request, 9u) == BSP_STATUS_BUSY,
                            "valid short workflow should start");
    for (uint8_t i = 0u; i < APP_CAL_WORKFLOW_REQUIRED_ACCEPTED; i++)
    {
        failures += feed_sample(&workflow, &sample);
    }
    failures += expect_u32((uint32_t)app_calibration_workflow_result(&workflow),
                           (uint32_t)APP_CAL_WORKFLOW_RESULT_OK,
                           "short evidence should complete");
    return failures;
}

static int test_unsupported_condition_rejected(void)
{
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    request.key = key_for(HW_RANGE_ID_10R, HW_EXCITATION_FREQ_1KHZ, HW_EXCITATION_AMP_500MVRMS);
    int failures = expect_true(app_calibration_workflow_start(&workflow, &request, 1u) == BSP_STATUS_NOT_SUPPORTED,
                               "10R 500mV calibration acquisition must be rejected");
    failures += expect_u32((uint32_t)app_calibration_workflow_result(&workflow),
                           (uint32_t)APP_CAL_WORKFLOW_RESULT_UNSUPPORTED_CONDITION,
                           "unsupported result");
    return failures;
}

static int test_clipping_rejected_until_bounded_failure(void)
{
    int failures = 0;
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_SHORT);
    app_cal_capture_sample_t sample = sample_for(&request, measurement_complex(0.02f, 0.0f), APP_CAL_REJECT_NONE);
    sample.clipped = true;
    failures += expect_true(app_calibration_workflow_start(&workflow, &request, 2u) == BSP_STATUS_BUSY,
                            "short workflow should start");
    for (uint8_t i = 0u; i < APP_CAL_WORKFLOW_MAX_ATTEMPTS; i++)
    {
        failures += feed_sample(&workflow, &sample);
    }
    const app_cal_evidence_t *evidence = app_calibration_workflow_evidence(&workflow);
    failures += expect_u32((uint32_t)app_calibration_workflow_result(&workflow),
                           (uint32_t)APP_CAL_WORKFLOW_RESULT_TOO_MANY_REJECTS,
                           "clipping should fail bounded");
    failures += expect_u32(evidence->rejected, APP_CAL_WORKFLOW_MAX_ATTEMPTS,
                           "all clipped captures rejected");
    return failures;
}

static int test_unstable_evidence_fails_after_max_attempts(void)
{
    int failures = 0;
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    failures += expect_true(app_calibration_workflow_start(&workflow, &request, 3u) == BSP_STATUS_BUSY,
                            "workflow should start");
    for (uint8_t i = 0u; i < APP_CAL_WORKFLOW_MAX_ATTEMPTS; i++)
    {
        const float z = (i & 1u) ? 1040.0f : 960.0f;
        const app_cal_capture_sample_t sample = sample_for(&request, measurement_complex(z, 0.0f),
                                                           APP_CAL_REJECT_NONE);
        failures += feed_sample(&workflow, &sample);
    }
    failures += expect_u32((uint32_t)app_calibration_workflow_result(&workflow),
                           (uint32_t)APP_CAL_WORKFLOW_RESULT_UNSTABLE,
                           "unstable accepted captures should fail");
    return failures;
}

static int test_1x_weak_hg_good_completes_hg_path_only(void)
{
    int failures = 0;
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    failures += expect_true(app_calibration_workflow_start(&workflow, &request, 10u) == BSP_STATUS_BUSY,
                            "HG-only workflow should start");
    app_cal_capture_sample_t sample = sample_for(&request,
                                                 measurement_complex(1200.0f, 0.0f),
                                                 APP_CAL_REJECT_NONE);
    sample.ret_1x_usable = false;
    sample.z_1x_valid = false;
    sample.ret_1x_peak_v = 0.0005f;
    sample.ret_hg_usable = true;
    sample.z_hg_valid = true;
    for (uint8_t i = 0u; i < APP_CAL_WORKFLOW_REQUIRED_ACCEPTED; i++)
    {
        failures += feed_sample(&workflow, &sample);
    }
    const app_cal_evidence_t *evidence = app_calibration_workflow_evidence(&workflow);
    failures += expect_u32((uint32_t)app_calibration_workflow_result(&workflow),
                           (uint32_t)APP_CAL_WORKFLOW_RESULT_OK,
                           "HG-only stable evidence should complete");
    failures += expect_true(!evidence->ret_1x_evidence_valid, "1X path remains invalid");
    failures += expect_true(evidence->ret_hg_evidence_valid, "HG path carries evidence");
    return failures;
}

static int test_alternating_paths_do_not_fake_stability(void)
{
    int failures = 0;
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    failures += expect_true(app_calibration_workflow_start(&workflow, &request, 11u) == BSP_STATUS_BUSY,
                            "alternating workflow should start");
    for (uint8_t i = 0u; i < APP_CAL_WORKFLOW_MAX_ATTEMPTS; i++)
    {
        app_cal_capture_sample_t sample = sample_for(&request,
                                                     measurement_complex(1000.0f, 0.0f),
                                                     APP_CAL_REJECT_NONE);
        if ((i & 1u) == 0u)
        {
            sample.ret_hg_usable = false;
            sample.z_hg_valid = false;
        }
        else
        {
            sample.ret_1x_usable = false;
            sample.z_1x_valid = false;
        }
        failures += feed_sample(&workflow, &sample);
    }
    const app_cal_evidence_t *evidence = app_calibration_workflow_evidence(&workflow);
    failures += expect_u32((uint32_t)app_calibration_workflow_result(&workflow),
                           (uint32_t)APP_CAL_WORKFLOW_RESULT_UNSTABLE,
                           "alternating incomplete paths should fail unstable");
    failures += expect_true(!evidence->ret_1x_evidence_valid, "1X path below required count");
    failures += expect_true(!evidence->ret_hg_evidence_valid, "HG path below required count");
    return failures;
}

static int test_unstable_open_observable_fails(void)
{
    int failures = 0;
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_OPEN);
    failures += expect_true(app_calibration_workflow_start(&workflow, &request, 12u) == BSP_STATUS_BUSY,
                            "unstable OPEN workflow should start");
    for (uint8_t i = 0u; i < APP_CAL_WORKFLOW_MAX_ATTEMPTS; i++)
    {
        app_cal_capture_sample_t sample = sample_for(&request,
                                                     measurement_complex(0.0f, 0.0f),
                                                     APP_CAL_REJECT_NONE);
        const float y = (i & 1u) ? 0.200f : 0.0001f;
        sample.open_y_1x = measurement_complex(y, 0.0f);
        sample.open_y_hg = measurement_complex(y, 0.0f);
        sample.z_1x_valid = false;
        sample.z_hg_valid = false;
        failures += feed_sample(&workflow, &sample);
    }
    failures += expect_u32((uint32_t)app_calibration_workflow_result(&workflow),
                           (uint32_t)APP_CAL_WORKFLOW_RESULT_UNSTABLE,
                           "unstable OPEN observable should fail");
    return failures;
}

static int test_safety_abort_fails_immediately(void)
{
    int failures = 0;
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    failures += expect_true(app_calibration_workflow_start(&workflow, &request, 4u) == BSP_STATUS_BUSY,
                            "workflow should start");
    failures += expect_true(app_calibration_workflow_mark_capture_started(&workflow) == BSP_STATUS_OK,
                            "capture should start");
    (void)app_calibration_workflow_submit_failure(&workflow, APP_CAL_REJECT_SAFETY_ABORT);
    failures += expect_u32((uint32_t)app_calibration_workflow_result(&workflow),
                           (uint32_t)APP_CAL_WORKFLOW_RESULT_SAFETY_ABORT,
                           "safety abort should terminate");
    return failures;
}

static int test_cancel_during_capture_discards_evidence(void)
{
    int failures = 0;
    app_calibration_workflow_t workflow;
    app_calibration_workflow_init(&workflow);
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    failures += expect_true(app_calibration_workflow_start(&workflow, &request, 5u) == BSP_STATUS_BUSY,
                            "workflow should start");
    failures += expect_true(app_calibration_workflow_mark_capture_started(&workflow) == BSP_STATUS_OK,
                            "capture should start");
    failures += expect_true(app_calibration_workflow_cancel(&workflow) == BSP_STATUS_BUSY,
                            "active capture cancel should wait for safe path");
    app_calibration_workflow_cancel_complete(&workflow);
    failures += expect_u32((uint32_t)app_calibration_workflow_state(&workflow),
                           (uint32_t)APP_CAL_WORKFLOW_CANCELED,
                           "workflow canceled");
    failures += expect_u32(app_calibration_workflow_evidence(&workflow)->accepted, 0u,
                           "canceled evidence discarded");
    return failures;
}

static bsp_status_t fake_read(uint32_t address, void *dst, size_t size, void *user)
{
    fake_store_t *fake = (fake_store_t *)user;
    if ((fake == NULL) || (dst == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!fake->initialized)
    {
        (void)memset(fake->flash, 0xFF, sizeof(fake->flash));
        fake->initialized = true;
    }
    if (address < TEST_MUTABLE_BASE)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    const uint32_t local = address - TEST_MUTABLE_BASE;
    if ((local >= TEST_FLASH_BYTES) || (size > ((size_t)TEST_FLASH_BYTES - (size_t)local)))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    fake->read_count++;
    (void)memcpy(dst, &fake->flash[local], size);
    return BSP_STATUS_OK;
}

static bsp_status_t fake_start(uint32_t address, uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_store_t *fake = (fake_store_t *)user;
    if (fake == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!fake->initialized)
    {
        (void)memset(fake->flash, 0xFF, sizeof(fake->flash));
        fake->initialized = true;
    }
    if ((address < TEST_MUTABLE_BASE) || ((address % STORAGE_LAYOUT_W25Q_SECTOR_SIZE) != 0u) ||
        (fake->state != FAKE_STORE_IDLE))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    fake->pending_address = address;
    fake->pending_size = STORAGE_LAYOUT_W25Q_SECTOR_SIZE;
    fake->busy_polls = 1u;
    fake->state = FAKE_STORE_ERASE_BUSY;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_program(uint32_t address,
                                 const void *src,
                                 size_t size,
                                 uint32_t now_ms,
                                 void *user)
{
    (void)now_ms;
    fake_store_t *fake = (fake_store_t *)user;
    const uint8_t *bytes = (const uint8_t *)src;
    if ((fake == NULL) || (bytes == NULL) || fake->fail_program ||
        (address < TEST_MUTABLE_BASE) || (size == 0u) || (size > sizeof(fake->pending)) ||
        (size > (256u - (address % 256u))) || (fake->state != FAKE_STORE_IDLE))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    const uint32_t local = address - TEST_MUTABLE_BASE;
    if ((local >= TEST_FLASH_BYTES) || (size > ((size_t)TEST_FLASH_BYTES - (size_t)local)))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    for (size_t i = 0u; i < size; i++)
    {
        if ((uint8_t)(fake->flash[local + i] & bytes[i]) != bytes[i])
        {
            return BSP_STATUS_ERROR;
        }
    }
    (void)memcpy(fake->pending, bytes, size);
    fake->pending_address = address;
    fake->pending_size = size;
    fake->busy_polls = 1u;
    fake->state = FAKE_STORE_PROGRAM_BUSY;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_poll(uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_store_t *fake = (fake_store_t *)user;
    if (fake == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (fake->state == FAKE_STORE_IDLE)
    {
        return BSP_STATUS_OK;
    }
    if (fake->busy_polls != 0u)
    {
        fake->busy_polls--;
        return BSP_STATUS_BUSY;
    }
    const uint32_t local = fake->pending_address - TEST_MUTABLE_BASE;
    if (fake->state == FAKE_STORE_ERASE_BUSY)
    {
        (void)memset(&fake->flash[local], 0xFF, fake->pending_size);
    }
    else
    {
        for (size_t i = 0u; i < fake->pending_size; i++)
        {
            fake->flash[local + i] &= fake->pending[i];
        }
    }
    fake->state = FAKE_STORE_IDLE;
    return BSP_STATUS_OK;
}

static measurement_cal_store_io_t fake_io(fake_store_t *fake)
{
    const measurement_cal_store_io_t io = {
        .read = fake_read,
        .erase_sector_start = fake_start,
        .program_start = fake_program,
        .poll = fake_poll,
        .user = fake,
    };
    return io;
}

static measurement_cal_set_t full_cal_set(uint32_t sequence, float marker)
{
    measurement_cal_set_t set;
    measurement_cal_set_init(&set,
                             MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                             sequence);
    for (hw_range_id_t range = HW_RANGE_ID_10R; range <= HW_RANGE_ID_1M; range++)
    {
        for (hw_excitation_freq_t freq = HW_EXCITATION_FREQ_100HZ;
             freq <= HW_EXCITATION_FREQ_10KHZ;
             freq++)
        {
            for (hw_excitation_amp_t amp = HW_EXCITATION_AMP_100MVRMS;
                 amp <= HW_EXCITATION_AMP_500MVRMS;
                 amp++)
            {
                if (measurement_cal_condition_allowed(range, freq, amp))
                {
                    const measurement_cal_key_t key =
                        measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                                            MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                            range,
                                            freq,
                                            amp);
                    measurement_cal_record_t record = measurement_cal_make_ideal_record(&key);
                    record.correction.k.re += marker;
                    record.correction.flags |= MEASUREMENT_CAL_FLAG_QUALIFIED;
                    (void)measurement_cal_set_add_record(&set, &record);
                }
            }
        }
    }
    return set;
}

static const measurement_cal_record_t *find_test_record(const measurement_cal_set_t *set,
                                                        const measurement_cal_key_t *key)
{
    if ((set == NULL) || (key == NULL))
    {
        return NULL;
    }
    for (uint8_t i = 0u; i < set->record_count; i++)
    {
        if (measurement_cal_key_equal(&set->records[i].key, key))
        {
            return &set->records[i];
        }
    }
    return NULL;
}

static bool fake_write_slot_frame(fake_store_t *fake,
                                  measurement_cal_store_slot_t slot,
                                  const measurement_cal_set_t *set)
{
    if ((fake == NULL) || (set == NULL))
    {
        return false;
    }
    if (!fake->initialized)
    {
        (void)memset(fake->flash, 0xFF, sizeof(fake->flash));
        fake->initialized = true;
    }
    uint8_t bytes[MEASUREMENT_CAL_MAX_FRAME_BYTES];
    size_t written = 0u;
    if (!measurement_cal_serialize_set(set, bytes, sizeof(bytes), &written))
    {
        return false;
    }
    storage_partition_t partition;
    if (!storage_layout_partition(TEST_CAPACITY_BYTES,
                                  (slot == MEASUREMENT_CAL_STORE_SLOT_A) ?
                                      STORAGE_PARTITION_CALIBRATION_A :
                                      STORAGE_PARTITION_CALIBRATION_B,
                                  &partition))
    {
        return false;
    }
    const uint32_t local = partition.start - TEST_MUTABLE_BASE;
    if ((local >= TEST_FLASH_BYTES) || (written > partition.size))
    {
        return false;
    }
    (void)memset(&fake->flash[local], 0xFF, partition.size);
    (void)memcpy(&fake->flash[local], bytes, written);
    return true;
}

static bsp_status_t service_commit_to_completion(app_calibration_service_t *service,
                                                 uint32_t start_ms)
{
    bsp_status_t status = BSP_STATUS_BUSY;
    for (uint32_t i = 0u; i < 160u; i++)
    {
        status = app_calibration_service_step(service, start_ms + i);
        if (status != BSP_STATUS_BUSY)
        {
            return status;
        }
    }
    return status;
}

static bsp_status_t fake_phase05_start(const hw_metrology_measure_request_t *request,
                                       uint32_t now_ms,
                                       void *user)
{
    (void)request;
    (void)now_ms;
    fake_phase05_t *fake = (fake_phase05_t *)user;
    if (fake == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (fake->start_error || (fake->start_count >= fake->block_count))
    {
        return BSP_STATUS_ERROR;
    }
    fake->current = fake->blocks[fake->start_count];
    fake->start_count++;
    fake->active = true;
    fake->done = false;
    fake->dumpable = !fake->fail_capture;
    if (!fake->fail_capture)
    {
        fake->error = HW_METROLOGY_MEASURE_OK;
    }
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_phase05_step(uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_phase05_t *fake = (fake_phase05_t *)user;
    if (fake == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    fake->step_count++;
    fake->active = false;
    fake->done = true;
    if (fake->abort_called)
    {
        fake->dumpable = false;
        fake->error = HW_METROLOGY_MEASURE_ERR_ABORT;
    }
    return BSP_STATUS_OK;
}

static bool fake_phase05_active(void *user)
{
    const fake_phase05_t *fake = (const fake_phase05_t *)user;
    return (fake != NULL) && fake->active;
}

static bool fake_phase05_done(void *user)
{
    const fake_phase05_t *fake = (const fake_phase05_t *)user;
    return (fake != NULL) && fake->done;
}

static bool fake_phase05_dumpable(void *user)
{
    const fake_phase05_t *fake = (const fake_phase05_t *)user;
    return (fake != NULL) && fake->dumpable;
}

static const hw_metrology_block_t *fake_phase05_block(void *user)
{
    const fake_phase05_t *fake = (const fake_phase05_t *)user;
    return (fake == NULL) ? NULL : fake->current;
}

static hw_metrology_measure_error_t fake_phase05_error(void *user)
{
    const fake_phase05_t *fake = (const fake_phase05_t *)user;
    return (fake == NULL) ? HW_METROLOGY_MEASURE_ERR_INVALID : fake->error;
}

static void fake_phase05_ack(void *user)
{
    fake_phase05_t *fake = (fake_phase05_t *)user;
    if (fake != NULL)
    {
        fake->done = false;
        fake->dumpable = false;
        fake->current = NULL;
    }
}

static bsp_status_t fake_phase05_abort(void *user)
{
    fake_phase05_t *fake = (fake_phase05_t *)user;
    if (fake == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    fake->abort_called = true;
    return BSP_STATUS_BUSY;
}

static app_cal_session_io_t fake_phase05_io(fake_phase05_t *fake)
{
    const app_cal_session_io_t io = {
        .start_capture = fake_phase05_start,
        .step_capture = fake_phase05_step,
        .capture_active = fake_phase05_active,
        .capture_done = fake_phase05_done,
        .capture_dumpable = fake_phase05_dumpable,
        .capture_block = fake_phase05_block,
        .capture_error = fake_phase05_error,
        .capture_acknowledge = fake_phase05_ack,
        .capture_abort = fake_phase05_abort,
        .user = fake,
    };
    return io;
}

static bsp_clock_summary_t good_clock(void)
{
    const bsp_clock_summary_t clock = {
        .source = BSP_CLOCK_SOURCE_HSE_PLL,
        .sysclk_hz = 72000000u,
        .pclk1_hz = 36000000u,
        .pclk2_hz = 72000000u,
        .adc_hz = 12000000u,
    };
    return clock;
}

static app_cal_session_event_t step_until_event(app_calibration_session_t *session,
                                                app_cal_session_event_t wanted,
                                                uint8_t max_steps)
{
    app_cal_session_event_t event = APP_CAL_SESSION_EVENT_NONE;
    for (uint8_t i = 0u; i < max_steps; i++)
    {
        event = app_calibration_session_step(session, (uint32_t)i);
        if (event == wanted)
        {
            return event;
        }
    }
    return event;
}

static int test_session_runs_open_capture_end_to_end(void)
{
    int failures = 0;
    app_calibration_service_t service;
    app_calibration_service_init(&service);
    app_calibration_session_t session;
    fake_phase05_t fake = {0};
    app_cal_session_io_t io = fake_phase05_io(&fake);
    failures += expect_true(app_calibration_session_init(&session, &service, &io) == BSP_STATUS_OK,
                            "calibration session init");
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_OPEN);
    static hw_metrology_block_t blocks[APP_CAL_WORKFLOW_REQUIRED_ACCEPTED];
    static uint32_t raw[APP_CAL_WORKFLOW_REQUIRED_ACCEPTED][HW_METROLOGY_RAW_WORD_COUNT];
    const measurement_complex_t source = measurement_complex(0.100f, 0.0f);
    for (uint8_t i = 0u; i < APP_CAL_WORKFLOW_REQUIRED_ACCEPTED; i++)
    {
        make_block(&blocks[i],
                   raw[i],
                   &request,
                   source,
                   source,
                   source,
                   measurement_complex(source.re * TEST_HG_NOMINAL, 0.0f));
        fake.blocks[i] = &blocks[i];
    }
    fake.block_count = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED;
    const bsp_clock_summary_t clock = good_clock();
    failures += expect_true(app_calibration_session_start(&session, &request, &clock, BSP_STATUS_OK, 0u) ==
                                BSP_STATUS_BUSY,
                            "calibration session starts");
    failures += expect_u32((uint32_t)app_calibration_session_step(&session, 0u),
                           (uint32_t)APP_CAL_SESSION_EVENT_BEGIN,
                           "session begin event");
    for (uint8_t i = 0u; i < APP_CAL_WORKFLOW_REQUIRED_ACCEPTED; i++)
    {
        failures += expect_u32((uint32_t)step_until_event(&session, APP_CAL_SESSION_EVENT_CAPTURE_BEGIN, 4u),
                               (uint32_t)APP_CAL_SESSION_EVENT_CAPTURE_BEGIN,
                               "session capture begin");
        failures += expect_u32((uint32_t)step_until_event(&session, APP_CAL_SESSION_EVENT_CAPTURE_ACCEPTED, 4u),
                               (uint32_t)APP_CAL_SESSION_EVENT_CAPTURE_ACCEPTED,
                               "session capture accepted");
    }
    failures += expect_u32((uint32_t)step_until_event(&session, APP_CAL_SESSION_EVENT_COMPLETE, 4u),
                           (uint32_t)APP_CAL_SESSION_EVENT_COMPLETE,
                           "session complete event");
    failures += expect_true(!app_calibration_session_active(&session), "session inactive after completion");
    failures += expect_u32(app_calibration_session_evidence(&session)->accepted,
                           APP_CAL_WORKFLOW_REQUIRED_ACCEPTED,
                           "session accepted evidence count");
    return failures;
}

static int test_session_phase05_safety_abort_fails_safely(void)
{
    int failures = 0;
    app_calibration_service_t service;
    app_calibration_service_init(&service);
    app_calibration_session_t session;
    fake_phase05_t fake = {0};
    app_cal_session_io_t io = fake_phase05_io(&fake);
    (void)app_calibration_session_init(&session, &service, &io);
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    static hw_metrology_block_t block;
    static uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    make_block(&block,
               raw,
               &request,
               measurement_complex(0.100f, 0.0f),
               measurement_complex(0.050f, 0.0f),
               measurement_complex(0.100f, 0.0f),
               measurement_complex(0.050f * TEST_HG_NOMINAL, 0.0f));
    fake.blocks[0] = &block;
    fake.block_count = 1u;
    fake.fail_capture = true;
    fake.error = HW_METROLOGY_MEASURE_ERR_PERMIT;
    const bsp_clock_summary_t clock = good_clock();
    failures += expect_true(app_calibration_session_start(&session, &request, &clock, BSP_STATUS_OK, 0u) ==
                                BSP_STATUS_BUSY,
                            "safety-abort session starts");
    failures += expect_u32((uint32_t)app_calibration_session_step(&session, 0u),
                           (uint32_t)APP_CAL_SESSION_EVENT_BEGIN,
                           "abort session begin");
    failures += expect_u32((uint32_t)app_calibration_session_step(&session, 1u),
                           (uint32_t)APP_CAL_SESSION_EVENT_CAPTURE_BEGIN,
                           "abort capture begin");
    failures += expect_u32((uint32_t)app_calibration_session_step(&session, 2u),
                           (uint32_t)APP_CAL_SESSION_EVENT_CAPTURE_REJECTED,
                           "abort capture rejected");
    failures += expect_u32((uint32_t)app_calibration_session_step(&session, 3u),
                           (uint32_t)APP_CAL_SESSION_EVENT_FAILED,
                           "abort session failed terminal");
    failures += expect_u32((uint32_t)app_calibration_workflow_result(app_calibration_service_workflow_const(&service)),
                           (uint32_t)APP_CAL_WORKFLOW_RESULT_SAFETY_ABORT,
                           "abort mapped to safety result");
    return failures;
}

static int test_session_cancel_waits_for_phase05_abort(void)
{
    int failures = 0;
    app_calibration_service_t service;
    app_calibration_service_init(&service);
    app_calibration_session_t session;
    fake_phase05_t fake = {0};
    app_cal_session_io_t io = fake_phase05_io(&fake);
    (void)app_calibration_session_init(&session, &service, &io);
    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    static hw_metrology_block_t block;
    static uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    make_block(&block,
               raw,
               &request,
               measurement_complex(0.100f, 0.0f),
               measurement_complex(0.050f, 0.0f),
               measurement_complex(0.100f, 0.0f),
               measurement_complex(0.050f * TEST_HG_NOMINAL, 0.0f));
    fake.blocks[0] = &block;
    fake.block_count = 1u;
    const bsp_clock_summary_t clock = good_clock();
    (void)app_calibration_session_start(&session, &request, &clock, BSP_STATUS_OK, 0u);
    (void)app_calibration_session_step(&session, 0u);
    (void)app_calibration_session_step(&session, 1u);
    failures += expect_true(app_calibration_session_cancel(&session) == BSP_STATUS_BUSY,
                            "cancel during Phase05 waits");
    failures += expect_true(fake.abort_called, "Phase05 abort requested");
    failures += expect_u32((uint32_t)app_calibration_session_step(&session, 2u),
                           (uint32_t)APP_CAL_SESSION_EVENT_CANCELED,
                           "cancel terminal event");
    failures += expect_true(!app_calibration_session_active(&session), "canceled session inactive");
    return failures;
}

static int test_product_service_owns_store_and_blocks_rescan_while_busy(void)
{
    int failures = 0;
    fake_store_t fake = {0};
    app_calibration_service_t service;
    app_calibration_service_init(&service);
    measurement_cal_store_io_t io = fake_io(&fake);
    (void)app_calibration_service_load(&service, &io, TEST_CAPACITY_BYTES);
    failures += expect_u32((uint32_t)app_calibration_service_status(&service),
                           (uint32_t)APP_CAL_SERVICE_NO_VALID_CALIBRATION,
                           "blank flash should leave no valid calibration");
    failures += expect_true(fake.read_count != 0u, "service store should read slots");

    const app_cal_workflow_request_t request = request_for(APP_CAL_STANDARD_LOAD);
    failures += expect_true(app_calibration_service_start_workflow(&service, &request) == BSP_STATUS_BUSY,
                            "service workflow should start");
    fake.read_count = 0u;
    failures += expect_true(app_calibration_service_load(&service, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_BUSY,
                            "busy service should reject rescan");
    failures += expect_u32(fake.read_count, 0u, "busy rescan must not touch storage");
    return failures;
}

static int test_service_commit_activates_only_after_verified_store_done(void)
{
    int failures = 0;
    fake_store_t fake = {0};
    measurement_cal_set_t old_set = full_cal_set(1u, 0.0f);
    measurement_cal_set_t new_set = full_cal_set(2u, 123.0f);
    failures += expect_true(fake_write_slot_frame(&fake, MEASUREMENT_CAL_STORE_SLOT_A, &old_set),
                            "old active fixture writes");

    app_calibration_service_t service;
    app_calibration_service_init(&service);
    measurement_cal_store_io_t io = fake_io(&fake);
    failures += expect_true(app_calibration_service_load(&service, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                            "service loads old active set");
    const measurement_cal_set_t *active = app_calibration_service_active_set(&service);
    failures += expect_true((active != NULL) && (active->sequence == 1u), "old active visible");

    failures += expect_true(app_calibration_service_candidate_begin(&service) == BSP_STATUS_OK,
                            "candidate begin");
    measurement_cal_set_t *candidate = app_calibration_service_candidate_set(&service);
    failures += expect_true(candidate != NULL, "candidate pointer");
    *candidate = new_set;
    failures += expect_true(app_calibration_service_candidate_commit_start(&service) == BSP_STATUS_BUSY,
                            "commit starts asynchronously");
    active = app_calibration_service_active_set(&service);
    failures += expect_true((active != NULL) && (active->sequence == 1u),
                            "old active remains during commit");

    for (uint32_t i = 0u; i < 120u; i++)
    {
        if (app_calibration_service_step(&service, 100u + i) == BSP_STATUS_OK)
        {
            break;
        }
    }
    active = app_calibration_service_active_set(&service);
    failures += expect_true((active != NULL) && (active->sequence == 2u),
                            "new active only after verify");
    const measurement_cal_key_t marker_key =
        measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                            MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                            HW_RANGE_ID_1K,
                            HW_EXCITATION_FREQ_1KHZ,
                            HW_EXCITATION_AMP_100MVRMS);
    const measurement_cal_record_t *old_record = find_test_record(&old_set, &marker_key);
    const measurement_cal_record_t *active_record = find_test_record(active, &marker_key);
    failures += expect_true((old_record != NULL) && (active_record != NULL),
                            "marker condition remains present");
    failures += expect_near((active_record != NULL) ? active_record->correction.k.re : 0.0f,
                            ((old_record != NULL) ? old_record->correction.k.re : 0.0f) +
                                123.0f,
                            0.001f,
                            "activated coefficients are new candidate");
    const app_calibration_runtime_t *runtime = app_calibration_service_runtime_const(&service);
    failures += expect_true(app_calibration_runtime_active_slot(runtime) == MEASUREMENT_CAL_STORE_SLOT_B,
                            "post-commit active slot updates immediately");
    const measurement_cal_store_slot_info_t *slots = app_calibration_runtime_slots(runtime);
    failures += expect_true((slots != NULL) &&
                                slots[(uint8_t)MEASUREMENT_CAL_STORE_SLOT_B].frame_valid &&
                                (slots[(uint8_t)MEASUREMENT_CAL_STORE_SLOT_B].frame.sequence == 2u),
                            "post-commit slot diagnostics include new active frame");

    failures += expect_true(app_calibration_service_candidate_begin(&service) == BSP_STATUS_OK,
                            "candidate C begin after DONE acknowledge");
    candidate = app_calibration_service_candidate_set(&service);
    failures += expect_true(candidate != NULL, "candidate C pointer");
    measurement_cal_set_init(candidate,
                             MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                             0u);
    failures += expect_true(app_calibration_service_step(&service, 500u) == BSP_STATUS_OK,
                            "ordinary service step after C begin");
    active = app_calibration_service_active_set(&service);
    failures += expect_true((active != NULL) && (active->sequence == 2u),
                            "empty candidate C cannot replace active B");
    return failures;
}

static int test_service_commit_failure_preserves_old_active(void)
{
    int failures = 0;
    fake_store_t fake = {0};
    measurement_cal_set_t old_set = full_cal_set(3u, 0.0f);
    measurement_cal_set_t new_set = full_cal_set(4u, 456.0f);
    failures += expect_true(fake_write_slot_frame(&fake, MEASUREMENT_CAL_STORE_SLOT_A, &old_set),
                            "old active fixture writes for failure");

    app_calibration_service_t service;
    app_calibration_service_init(&service);
    measurement_cal_store_io_t io = fake_io(&fake);
    failures += expect_true(app_calibration_service_load(&service, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                            "service loads old active before failure");
    failures += expect_true(app_calibration_service_candidate_begin(&service) == BSP_STATUS_OK,
                            "candidate begin before failure");
    measurement_cal_set_t *candidate = app_calibration_service_candidate_set(&service);
    *candidate = new_set;
    fake.fail_program = true;
    failures += expect_true(app_calibration_service_candidate_commit_start(&service) == BSP_STATUS_BUSY,
                            "failing commit starts");
    for (uint32_t i = 0u; i < 12u; i++)
    {
        const bsp_status_t status = app_calibration_service_step(&service, 200u + i);
        if ((status != BSP_STATUS_BUSY) && (status != BSP_STATUS_OK))
        {
            break;
        }
    }
    const measurement_cal_set_t *active = app_calibration_service_active_set(&service);
    failures += expect_true((active != NULL) && (active->sequence == 3u),
                            "old active preserved after commit failure");
    failures += expect_true(app_calibration_service_status(&service) == APP_CAL_SERVICE_ERROR,
                            "service reports commit error");
    fake.fail_program = false;
    failures += expect_true(app_calibration_service_candidate_discard(&service) == BSP_STATUS_OK,
                            "explicit discard acknowledges failed store transaction");
    failures += expect_true(app_calibration_service_candidate_begin(&service) == BSP_STATUS_OK,
                            "candidate retry begins");
    candidate = app_calibration_service_candidate_set(&service);
    *candidate = new_set;
    failures += expect_true(app_calibration_service_candidate_commit_start(&service) == BSP_STATUS_BUSY,
                            "retry commit starts");
    failures += expect_true(service_commit_to_completion(&service, 300u) == BSP_STATUS_OK,
                            "retry commit completes");
    active = app_calibration_service_active_set(&service);
    failures += expect_true((active != NULL) && (active->sequence == 4u),
                            "retry candidate becomes active");
    return failures;
}

static int test_dirty_candidate_blocks_rescan_until_discard(void)
{
    int failures = 0;
    fake_store_t fake = {0};
    measurement_cal_set_t old_set = full_cal_set(8u, 0.0f);
    failures += expect_true(fake_write_slot_frame(&fake, MEASUREMENT_CAL_STORE_SLOT_A, &old_set),
                            "old active fixture writes for dirty rescan");

    app_calibration_service_t service;
    app_calibration_service_init(&service);
    measurement_cal_store_io_t io = fake_io(&fake);
    failures += expect_true(app_calibration_service_load(&service, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                            "service loads active before dirty candidate");
    failures += expect_true(app_calibration_service_candidate_begin(&service) == BSP_STATUS_OK,
                            "dirty candidate begin");
    measurement_cal_set_t *candidate = app_calibration_service_candidate_set(&service);
    failures += expect_true(candidate != NULL, "dirty candidate pointer");
    candidate->records[0] = old_set.records[0];
    candidate->record_count = 1u;
    failures += expect_true(app_calibration_service_load(&service, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_BUSY,
                            "dirty candidate blocks rescan");
    failures += expect_u32(candidate->record_count, 1u, "dirty candidate record preserved");
    const measurement_cal_set_t *active = app_calibration_service_active_set(&service);
    failures += expect_true((active != NULL) && (active->sequence == 8u),
                            "active calibration unchanged while rescan is blocked");
    failures += expect_true(app_calibration_service_candidate_discard(&service) == BSP_STATUS_OK,
                            "discard dirty candidate");
    failures += expect_true(app_calibration_service_load(&service, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                            "rescan succeeds after discard");
    return failures;
}

static int test_second_recalibration_alternates_slots_without_aliasing(void)
{
    int failures = 0;
    fake_store_t fake = {0};
    measurement_cal_set_t active_a = full_cal_set(1u, 0.0f);
    measurement_cal_set_t candidate_b = full_cal_set(2u, 111.0f);
    measurement_cal_set_t candidate_c = full_cal_set(3u, 222.0f);
    failures += expect_true(fake_write_slot_frame(&fake, MEASUREMENT_CAL_STORE_SLOT_A, &active_a),
                            "initial A fixture writes");

    app_calibration_service_t service;
    app_calibration_service_init(&service);
    measurement_cal_store_io_t io = fake_io(&fake);
    failures += expect_true(app_calibration_service_load(&service, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                            "service loads A");
    failures += expect_true(app_calibration_service_candidate_begin(&service) == BSP_STATUS_OK,
                            "candidate B begin");
    measurement_cal_set_t *candidate = app_calibration_service_candidate_set(&service);
    *candidate = candidate_b;
    failures += expect_true(app_calibration_service_candidate_commit_start(&service) == BSP_STATUS_BUSY,
                            "commit B starts");
    failures += expect_true(service_commit_to_completion(&service, 100u) == BSP_STATUS_OK,
                            "commit B completes");
    const measurement_cal_set_t *active = app_calibration_service_active_set(&service);
    failures += expect_true((active != NULL) && (active->sequence == 2u),
                            "B active after first recalibration");
    failures += expect_true(app_calibration_runtime_active_slot(
                                app_calibration_service_runtime_const(&service)) ==
                                MEASUREMENT_CAL_STORE_SLOT_B,
                            "B is active in alternate slot");

    failures += expect_true(app_calibration_service_candidate_begin(&service) == BSP_STATUS_OK,
                            "candidate C begin");
    candidate = app_calibration_service_candidate_set(&service);
    *candidate = candidate_c;
    failures += expect_true(app_calibration_service_candidate_commit_start(&service) == BSP_STATUS_BUSY,
                            "commit C starts");
    active = app_calibration_service_active_set(&service);
    failures += expect_true((active != NULL) && (active->sequence == 2u),
                            "B remains active while C writes");
    failures += expect_true(service_commit_to_completion(&service, 300u) == BSP_STATUS_OK,
                            "commit C completes");
    active = app_calibration_service_active_set(&service);
    failures += expect_true((active != NULL) && (active->sequence == 3u),
                            "C active after second recalibration");
    failures += expect_true(app_calibration_runtime_active_slot(
                                app_calibration_service_runtime_const(&service)) ==
                                MEASUREMENT_CAL_STORE_SLOT_A,
                            "C alternates back to slot A");

    app_calibration_service_t rebooted;
    app_calibration_service_init(&rebooted);
    failures += expect_true(app_calibration_service_load(&rebooted, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                            "reboot reloads newest committed C");
    active = app_calibration_service_active_set(&rebooted);
    failures += expect_true((active != NULL) && (active->sequence == 3u),
                            "reboot active matches immediate runtime");
    failures += expect_true(app_calibration_runtime_active_slot(
                                app_calibration_service_runtime_const(&rebooted)) ==
                                app_calibration_runtime_active_slot(
                                    app_calibration_service_runtime_const(&service)),
                            "reboot slot diagnostics match immediate runtime");
    return failures;
}

static app_cal_evidence_t campaign_evidence(app_cal_standard_type_t type,
                                            measurement_cal_key_t key,
                                            measurement_complex_t t,
                                            measurement_complex_t source,
                                            measurement_complex_t load_z)
{
    const measurement_complex_t h_hg = measurement_complex(15.4f, 0.2f);
    measurement_complex_t open_y = t;
    if (type == APP_CAL_STANDARD_OPEN)
    {
        (void)measurement_complex_div(measurement_complex_sub(measurement_complex(1.0f, 0.0f), t),
                                      t,
                                      &open_y);
    }
    app_cal_evidence_t evidence = {
        .key = key,
        .standard = {
            .type = type,
            .z_ohms = load_z,
            .z_valid = type == APP_CAL_STANDARD_LOAD,
        },
        .accepted = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED,
        .attempts = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED,
        .stable = true,
        .ret_1x_consistent = true,
        .ret_hg_consistent = true,
        .ret_1x_evidence_valid = true,
        .ret_hg_evidence_valid = true,
        .hg_overlap_valid = true,
        .temperature = {
            .count = 1u,
            .mean_mC = 24000,
            .min_mC = 24000,
            .max_mC = 24000,
            .valid = true,
        },
        .source_1 = {.count = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED, .mean = source},
        .source_2 = {.count = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED, .mean = source},
        .ret_1x = {.count = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED,
                   .mean = measurement_complex_mul(source, t)},
        .ret_hg_raw = {.count = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED,
                       .mean = measurement_complex_mul(measurement_complex_mul(source, t), h_hg)},
        .ret_hg_reconstructed = {.count = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED,
                                 .mean = measurement_complex_mul(source, t)},
        .open_y_1x = {.count = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED, .mean = open_y},
        .open_y_hg = {.count = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED, .mean = open_y},
        .hg_observed_transfer = {.count = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED,
                                 .mean = h_hg},
    };
    evidence.ret_1x_path.stable = true;
    evidence.ret_hg_path.stable = true;
    evidence.ret_1x_path.evidence_valid = true;
    evidence.ret_hg_path.evidence_valid = true;
    evidence.ret_1x_path.usable_count = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED;
    evidence.ret_hg_path.usable_count = APP_CAL_WORKFLOW_REQUIRED_ACCEPTED;
    return evidence;
}

static int test_campaign_solves_condition_and_inserts_candidate_record(void)
{
    int failures = 0;
    const measurement_cal_key_t key =
        key_for(HW_RANGE_ID_1K, HW_EXCITATION_FREQ_1KHZ, HW_EXCITATION_AMP_100MVRMS);
    app_calibration_campaign_t campaign;
    app_calibration_campaign_init(&campaign);
    failures += expect_true(app_calibration_campaign_begin_condition(&campaign, &key) == BSP_STATUS_OK,
                            "campaign begins condition");
    failures += expect_u32(app_calibration_campaign_missing_mask(&campaign),
                           0x07u,
                           "all standards initially missing");

    const measurement_complex_t source = measurement_complex(0.1f, 0.0f);
    const measurement_complex_t open_t = measurement_complex(0.98f, 0.01f);
    const measurement_complex_t short_t = measurement_complex(0.02f, 0.0f);
    const measurement_complex_t load_t = measurement_complex(0.50f, 0.02f);
    const measurement_complex_t load_z = measurement_complex(1000.0f, 0.0f);
    app_cal_evidence_t open = campaign_evidence(APP_CAL_STANDARD_OPEN, key, open_t, source, load_z);
    app_cal_evidence_t shorted = campaign_evidence(APP_CAL_STANDARD_SHORT, key, short_t, source, load_z);
    app_cal_evidence_t load = campaign_evidence(APP_CAL_STANDARD_LOAD, key, load_t, source, load_z);
    failures += expect_true(app_calibration_campaign_submit_evidence(&campaign, &open) == BSP_STATUS_OK,
                            "open evidence accepted");
    failures += expect_true(app_calibration_campaign_submit_evidence(&campaign, &shorted) == BSP_STATUS_OK,
                            "short evidence accepted");
    failures += expect_true(app_calibration_campaign_submit_evidence(&campaign, &load) == BSP_STATUS_OK,
                            "load evidence accepted");
    failures += expect_true(app_calibration_campaign_condition_ready(&campaign), "condition ready");

    measurement_cal_record_t record;
    failures += expect_true(app_calibration_campaign_solve_condition(&campaign, &record) ==
                                MEASUREMENT_CAL_SOLVER_OK,
                            "campaign solves condition");
    failures += expect_true((record.correction.flags & MEASUREMENT_CAL_FLAG_OSL_MODEL) != 0u,
                            "campaign record is OSL");
    failures += expect_true((record.correction.flags & MEASUREMENT_CAL_FLAG_TEMPERATURE_VALID) != 0u,
                            "campaign preserves temperature validity");
    measurement_cal_osl_coefficients_t coefficients;
    failures += expect_true(measurement_cal_get_osl_coefficients(&record.correction, &coefficients),
                            "campaign OSL coefficients decode");
    failures += expect_complex_near(coefficients.t_open, open_t, 0.0001f, "campaign converts open_y to t_open");

    measurement_cal_set_t candidate;
    measurement_cal_set_init(&candidate,
                             MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                             0u);
    failures += expect_true(app_calibration_campaign_insert_record(&record, &candidate) == BSP_STATUS_OK,
                            "candidate insert");
    failures += expect_u32(candidate.record_count, 1u, "candidate count");
    record.correction.load_z_ohms = measurement_complex(999.0f, 1.0f);
    failures += expect_true(app_calibration_campaign_insert_record(&record, &candidate) == BSP_STATUS_OK,
                            "candidate replace");
    failures += expect_u32(candidate.record_count, 1u, "candidate still one record");
    return failures;
}

int main(int argc, char **argv)
{
    if ((argc > 1) && (strcmp(argv[1], "--print-sizes") == 0))
    {
        (void)printf("app_calibration_service_t=%lu\n",
                     (unsigned long)app_calibration_service_context_size_bytes());
        (void)printf("app_calibration_workflow_t=%lu\n",
                     (unsigned long)app_calibration_workflow_context_size_bytes());
        (void)printf("app_cal_evidence_t=%lu\n",
                     (unsigned long)app_cal_evidence_size_bytes());
        (void)printf("app_cal_capture_sample_t=%lu\n",
                     (unsigned long)sizeof(app_cal_capture_sample_t));
        (void)printf("app_cal_complex_stats_t=%lu\n",
                     (unsigned long)sizeof(app_cal_complex_stats_t));
        (void)printf("app_cal_path_evidence_t=%lu\n",
                     (unsigned long)sizeof(app_cal_path_evidence_t));
        (void)printf("app_calibration_session_t=%lu\n",
                     (unsigned long)app_calibration_session_context_size_bytes());
        (void)printf("app_calibration_campaign_t=%lu\n",
                     (unsigned long)app_calibration_campaign_context_size_bytes());
        return 0;
    }

    int failures = 0;
    failures += test_clean_load_completes();
    failures += test_open_accepts_phasor_evidence_without_forced_z();
    failures += test_raw_open_block_accepts_denominator_singularity();
    failures += test_raw_load_block_extracts_independent_hg_source_path();
    failures += test_raw_hg_observed_transfer_preserves_nonideal_gain();
    failures += test_clipped_hg_does_not_reject_good_1x_path();
    failures += test_clean_short_completes();
    failures += test_unsupported_condition_rejected();
    failures += test_clipping_rejected_until_bounded_failure();
    failures += test_unstable_evidence_fails_after_max_attempts();
    failures += test_1x_weak_hg_good_completes_hg_path_only();
    failures += test_alternating_paths_do_not_fake_stability();
    failures += test_unstable_open_observable_fails();
    failures += test_safety_abort_fails_immediately();
    failures += test_cancel_during_capture_discards_evidence();
    failures += test_product_service_owns_store_and_blocks_rescan_while_busy();
    failures += test_service_commit_activates_only_after_verified_store_done();
    failures += test_service_commit_failure_preserves_old_active();
    failures += test_dirty_candidate_blocks_rescan_until_discard();
    failures += test_second_recalibration_alternates_slots_without_aliasing();
    failures += test_campaign_solves_condition_and_inserts_candidate_record();
    failures += test_session_runs_open_capture_end_to_end();
    failures += test_session_phase05_safety_abort_fails_safely();
    failures += test_session_cancel_waits_for_phase05_abort();
    return failures == 0 ? 0 : 1;
}
