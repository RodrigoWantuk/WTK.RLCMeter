#include "app/app_calibration_service.h"

#include <stdio.h>
#include <string.h>

#define TEST_CAPACITY_BYTES (2u * 1024u * 1024u)

typedef struct
{
    uint8_t read_count;
} fake_store_t;

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
        .source_v = measurement_complex(0.100f, 0.0f),
        .ret_1x_v = measurement_complex(0.050f, 0.0f),
        .ret_hg_v = measurement_complex(0.050f, 0.0f),
        .z_1x_ohms = z,
        .z_hg_ohms = z,
        .source_peak_v = 0.100f,
        .ret_1x_peak_v = 0.050f,
        .ret_hg_peak_v = 0.050f,
        .denominator_1x_peak_v = 0.050f,
        .denominator_hg_peak_v = 0.050f,
        .ret_1x_usable = true,
        .ret_hg_usable = true,
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
        return 0;
    }

    int failures = 0;
    failures += test_clean_load_completes();
    failures += test_open_accepts_phasor_evidence_without_forced_z();
    failures += test_clean_short_completes();
    failures += test_unsupported_condition_rejected();
    failures += test_clipping_rejected_until_bounded_failure();
    failures += test_unstable_evidence_fails_after_max_attempts();
    failures += test_safety_abort_fails_immediately();
    failures += test_cancel_during_capture_discards_evidence();
    failures += test_product_service_owns_store_and_blocks_rescan_while_busy();
    return failures == 0 ? 0 : 1;
}
