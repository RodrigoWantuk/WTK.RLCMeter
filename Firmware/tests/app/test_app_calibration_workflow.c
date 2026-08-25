#include "app/app_calibration_service.h"
#include "app/app_calibration_session.h"

#include <stdio.h>
#include <string.h>

#define TEST_CAPACITY_BYTES (2u * 1024u * 1024u)
#define TEST_MID_V (1.65f)
#define TEST_ADC_SCALE (3.3f / 4095.0f)
#define TEST_HG_NOMINAL (15.4680851064f)

typedef struct
{
    uint8_t read_count;
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
    const measurement_complex_t ret_1x = measurement_complex(0.006f, 0.002f);
    const measurement_complex_t h_hg = measurement_complex(12.0f, 1.5f);
    make_block(&block,
               raw,
               &request,
               measurement_complex(0.100f, 0.0f),
               ret_1x,
               measurement_complex(0.100f, 0.0f),
               measurement_complex_mul(ret_1x, h_hg));

    app_cal_capture_sample_t sample;
    failures += expect_true(app_calibration_workflow_sample_from_block(&block, &request, &sample) == BSP_STATUS_OK,
                            "nonideal HG block should produce evidence");
    failures += expect_true(sample.hg_overlap_valid, "HG overlap should be valid");
    failures += expect_complex_near(sample.hg_observed_transfer, h_hg, 0.75f, "observed raw HG transfer");
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
    (void)address;
    fake_store_t *fake = (fake_store_t *)user;
    if ((fake == NULL) || (dst == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    fake->read_count++;
    (void)memset(dst, 0xFF, size);
    return BSP_STATUS_OK;
}

static bsp_status_t fake_start(uint32_t address, uint32_t now_ms, void *user)
{
    (void)address;
    (void)now_ms;
    (void)user;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_program(uint32_t address,
                                 const void *src,
                                 size_t size,
                                 uint32_t now_ms,
                                 void *user)
{
    (void)address;
    (void)src;
    (void)size;
    (void)now_ms;
    (void)user;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_poll(uint32_t now_ms, void *user)
{
    (void)now_ms;
    (void)user;
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
    failures += test_session_runs_open_capture_end_to_end();
    failures += test_session_phase05_safety_abort_fails_safely();
    failures += test_session_cancel_waits_for_phase05_abort();
    return failures == 0 ? 0 : 1;
}
