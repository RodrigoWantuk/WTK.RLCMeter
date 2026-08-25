#include "app/app_calibration_workflow.h"

#include <math.h>
#include <stddef.h>

#include "measurement/measurement_condition.h"

static bool finite_complex(measurement_complex_t value)
{
    return measurement_complex_is_finite(value);
}

static bool finite_float(float value)
{
    return isfinite(value) != 0;
}

static float max_float(float a, float b)
{
    return (a > b) ? a : b;
}

static bool state_terminal(app_cal_workflow_state_t state)
{
    return (state == APP_CAL_WORKFLOW_COMPLETE) ||
           (state == APP_CAL_WORKFLOW_FAILED) ||
           (state == APP_CAL_WORKFLOW_CANCELED);
}

static bool standard_valid(const app_cal_standard_t *standard)
{
    if (standard == NULL)
    {
        return false;
    }
    if ((standard->type != APP_CAL_STANDARD_OPEN) &&
        (standard->type != APP_CAL_STANDARD_SHORT) &&
        (standard->type != APP_CAL_STANDARD_LOAD))
    {
        return false;
    }
    if (standard->type == APP_CAL_STANDARD_LOAD)
    {
        return standard->z_valid && finite_complex(standard->z_ohms);
    }
    return true;
}

static void stats_push(app_cal_complex_stats_t *stats, measurement_complex_t value)
{
    if (stats == NULL)
    {
        return;
    }
    stats->count++;
    const float count_f = (float)stats->count;
    const float delta_re = value.re - stats->mean.re;
    const float delta_im = value.im - stats->mean.im;
    stats->mean.re += delta_re / count_f;
    stats->mean.im += delta_im / count_f;
    stats->m2_re += delta_re * (value.re - stats->mean.re);
    stats->m2_im += delta_im * (value.im - stats->mean.im);
}

static float stats_variance_mag2(const app_cal_complex_stats_t *stats)
{
    if ((stats == NULL) || (stats->count < 2u))
    {
        return 0.0f;
    }
    return (stats->m2_re + stats->m2_im) / (float)(stats->count - 1u);
}

static bool stats_stable(const app_cal_complex_stats_t *stats)
{
    if ((stats == NULL) || (stats->count < APP_CAL_WORKFLOW_REQUIRED_ACCEPTED))
    {
        return false;
    }
    const float mean_mag = measurement_complex_mag(stats->mean);
    const float limit = max_float(1.0e-9f,
                                  mean_mag * ((float)APP_CAL_WORKFLOW_STABILITY_LIMIT_PPM / 1000000.0f));
    return stats_variance_mag2(stats) <= (limit * limit);
}

static bool sample_has_valid_z(const app_cal_capture_sample_t *sample)
{
    return (sample != NULL) &&
           ((sample->ret_1x_usable && sample->z_1x_valid && finite_complex(sample->z_1x_ohms)) ||
            (sample->ret_hg_usable && sample->z_hg_valid && finite_complex(sample->z_hg_ohms)));
}

static uint32_t sample_reject_flags(const app_cal_capture_sample_t *sample,
                                    app_cal_standard_type_t standard)
{
    uint32_t flags = APP_CAL_REJECT_NONE;
    if (sample == NULL)
    {
        return APP_CAL_REJECT_DSP;
    }
    flags |= sample->reject_flags;
    if (sample->clipped)
    {
        flags |= APP_CAL_REJECT_CLIPPED;
    }
    if (!finite_complex(sample->source_v) ||
        !finite_complex(sample->ret_1x_v) ||
        !finite_complex(sample->ret_hg_v) ||
        !finite_float(sample->source_peak_v) ||
        !finite_float(sample->ret_1x_peak_v) ||
        !finite_float(sample->ret_hg_peak_v))
    {
        flags |= APP_CAL_REJECT_NONFINITE;
    }
    if (sample->source_peak_v < ((float)APP_CAL_WORKFLOW_SOURCE_MIN_UV_PEAK / 1000000.0f))
    {
        flags |= APP_CAL_REJECT_SOURCE_TOO_SMALL;
    }
    if (!sample->ret_1x_usable && !sample->ret_hg_usable)
    {
        flags |= APP_CAL_REJECT_NO_USABLE_CHANNEL;
    }
    if (standard != APP_CAL_STANDARD_OPEN)
    {
        const float threshold = (float)APP_CAL_WORKFLOW_DENOMINATOR_MIN_UV_PEAK / 1000000.0f;
        if (((sample->ret_1x_usable && (sample->denominator_1x_peak_v < threshold)) ||
             (sample->ret_hg_usable && (sample->denominator_hg_peak_v < threshold))) ||
            !sample_has_valid_z(sample))
        {
            flags |= APP_CAL_REJECT_DENOMINATOR_TOO_SMALL;
        }
    }
    return flags;
}

static void finish_failure(app_calibration_workflow_t *workflow,
                           app_cal_workflow_result_t result)
{
    workflow->state = APP_CAL_WORKFLOW_FAILED;
    workflow->result = result;
    workflow->waiting_capture = false;
    workflow->evidence.stable = false;
}

static void evaluate_completion(app_calibration_workflow_t *workflow)
{
    if (workflow == NULL)
    {
        return;
    }
    if (workflow->evidence.accepted < APP_CAL_WORKFLOW_REQUIRED_ACCEPTED)
    {
        if (workflow->evidence.attempts >= APP_CAL_WORKFLOW_MAX_ATTEMPTS)
        {
            finish_failure(workflow, APP_CAL_WORKFLOW_RESULT_TOO_MANY_REJECTS);
        }
        else
        {
            workflow->state = APP_CAL_WORKFLOW_CAPTURE_REQUESTED;
            workflow->waiting_capture = false;
        }
        return;
    }

    const bool ret_1x_stable = (workflow->evidence.z_1x.count >= APP_CAL_WORKFLOW_REQUIRED_ACCEPTED) ?
                                   stats_stable(&workflow->evidence.z_1x) :
                                   true;
    const bool ret_hg_stable = (workflow->evidence.z_hg.count >= APP_CAL_WORKFLOW_REQUIRED_ACCEPTED) ?
                                  stats_stable(&workflow->evidence.z_hg) :
                                  true;
    workflow->evidence.stable = ret_1x_stable && ret_hg_stable;
    if (workflow->evidence.stable)
    {
        workflow->state = APP_CAL_WORKFLOW_COMPLETE;
        workflow->result = APP_CAL_WORKFLOW_RESULT_OK;
        workflow->waiting_capture = false;
        return;
    }

    if (workflow->evidence.attempts >= APP_CAL_WORKFLOW_MAX_ATTEMPTS)
    {
        finish_failure(workflow, APP_CAL_WORKFLOW_RESULT_UNSTABLE);
        return;
    }
    workflow->state = APP_CAL_WORKFLOW_CAPTURE_REQUESTED;
    workflow->waiting_capture = false;
}

void app_calibration_workflow_init(app_calibration_workflow_t *workflow)
{
    if (workflow != NULL)
    {
        *workflow = (app_calibration_workflow_t){0};
        workflow->state = APP_CAL_WORKFLOW_IDLE;
    }
}

bsp_status_t app_calibration_workflow_start(app_calibration_workflow_t *workflow,
                                            const app_cal_workflow_request_t *request,
                                            uint32_t sequence)
{
    if ((workflow == NULL) || (request == NULL) || !standard_valid(&request->standard))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (app_calibration_workflow_active(workflow))
    {
        return BSP_STATUS_BUSY;
    }
    if (!measurement_condition_supported(request->key.range_id,
                                         request->key.frequency,
                                         request->key.amplitude) ||
        (request->key.hardware_revision != MEASUREMENT_CAL_HARDWARE_REV1) ||
        (request->key.model_version != MEASUREMENT_CAL_MODEL_VERSION_CURRENT))
    {
        app_calibration_workflow_init(workflow);
        workflow->request = *request;
        workflow->state = APP_CAL_WORKFLOW_FAILED;
        workflow->result = APP_CAL_WORKFLOW_RESULT_UNSUPPORTED_CONDITION;
        return BSP_STATUS_NOT_SUPPORTED;
    }

    app_calibration_workflow_init(workflow);
    workflow->request = *request;
    workflow->sequence = sequence;
    workflow->state = APP_CAL_WORKFLOW_CAPTURE_REQUESTED;
    workflow->result = APP_CAL_WORKFLOW_RESULT_NONE;
    workflow->evidence.key = request->key;
    workflow->evidence.standard = request->standard;
    workflow->evidence.sequence = sequence;
    workflow->evidence.ret_1x_consistent = true;
    workflow->evidence.ret_hg_consistent = true;
    return BSP_STATUS_BUSY;
}

bool app_calibration_workflow_active(const app_calibration_workflow_t *workflow)
{
    return (workflow != NULL) &&
           (workflow->state != APP_CAL_WORKFLOW_IDLE) &&
           !state_terminal(workflow->state);
}

bool app_calibration_workflow_capture_pending(const app_calibration_workflow_t *workflow)
{
    return (workflow != NULL) && (workflow->state == APP_CAL_WORKFLOW_CAPTURE_REQUESTED);
}

bsp_status_t app_calibration_workflow_capture_request(const app_calibration_workflow_t *workflow,
                                                      measurement_cal_key_t *key)
{
    if ((workflow == NULL) || (key == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (workflow->state != APP_CAL_WORKFLOW_CAPTURE_REQUESTED)
    {
        return BSP_STATUS_BUSY;
    }
    *key = workflow->request.key;
    return BSP_STATUS_OK;
}

bsp_status_t app_calibration_workflow_mark_capture_started(app_calibration_workflow_t *workflow)
{
    if (workflow == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (workflow->state != APP_CAL_WORKFLOW_CAPTURE_REQUESTED)
    {
        return BSP_STATUS_BUSY;
    }
    workflow->state = APP_CAL_WORKFLOW_WAIT_CAPTURE;
    workflow->waiting_capture = true;
    return BSP_STATUS_OK;
}

bsp_status_t app_calibration_workflow_submit_sample(app_calibration_workflow_t *workflow,
                                                    const app_cal_capture_sample_t *sample)
{
    if ((workflow == NULL) || (sample == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (workflow->state != APP_CAL_WORKFLOW_WAIT_CAPTURE)
    {
        return BSP_STATUS_BUSY;
    }
    workflow->evidence.attempts++;
    workflow->waiting_capture = false;
    const uint32_t flags = sample_reject_flags(sample, workflow->request.standard.type);
    workflow->last_reject_flags = flags;
    if (flags != APP_CAL_REJECT_NONE)
    {
        workflow->evidence.rejected++;
        workflow->evidence.reject_flags |= flags;
        if (workflow->evidence.attempts >= APP_CAL_WORKFLOW_MAX_ATTEMPTS)
        {
            finish_failure(workflow, APP_CAL_WORKFLOW_RESULT_TOO_MANY_REJECTS);
        }
        else
        {
            workflow->state = APP_CAL_WORKFLOW_CAPTURE_REQUESTED;
        }
        return BSP_STATUS_ERROR;
    }

    workflow->evidence.accepted++;
    workflow->evidence.last_temperature_mC = sample->temperature_mC;
    stats_push(&workflow->evidence.source, sample->source_v);
    if (sample->ret_1x_usable)
    {
        stats_push(&workflow->evidence.ret_1x, sample->ret_1x_v);
        if (sample->z_1x_valid)
        {
            stats_push(&workflow->evidence.z_1x, sample->z_1x_ohms);
        }
    }
    else
    {
        workflow->evidence.ret_1x_consistent = false;
    }
    if (sample->ret_hg_usable)
    {
        stats_push(&workflow->evidence.ret_hg, sample->ret_hg_v);
        if (sample->z_hg_valid)
        {
            stats_push(&workflow->evidence.z_hg, sample->z_hg_ohms);
        }
    }
    else
    {
        workflow->evidence.ret_hg_consistent = false;
    }
    evaluate_completion(workflow);
    return BSP_STATUS_OK;
}

bsp_status_t app_calibration_workflow_submit_failure(app_calibration_workflow_t *workflow,
                                                     uint32_t reject_flags)
{
    if (workflow == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (workflow->state != APP_CAL_WORKFLOW_WAIT_CAPTURE)
    {
        return BSP_STATUS_BUSY;
    }
    workflow->evidence.attempts++;
    workflow->evidence.rejected++;
    workflow->evidence.reject_flags |= reject_flags;
    workflow->last_reject_flags = reject_flags;
    workflow->waiting_capture = false;
    if ((reject_flags & APP_CAL_REJECT_SAFETY_ABORT) != 0u)
    {
        finish_failure(workflow, APP_CAL_WORKFLOW_RESULT_SAFETY_ABORT);
        return BSP_STATUS_ERROR;
    }
    if ((reject_flags & APP_CAL_REJECT_PHASE05) != 0u)
    {
        finish_failure(workflow, APP_CAL_WORKFLOW_RESULT_PHASE05_ERROR);
        return BSP_STATUS_ERROR;
    }
    if (workflow->evidence.attempts >= APP_CAL_WORKFLOW_MAX_ATTEMPTS)
    {
        finish_failure(workflow, APP_CAL_WORKFLOW_RESULT_TOO_MANY_REJECTS);
        return BSP_STATUS_ERROR;
    }
    workflow->state = APP_CAL_WORKFLOW_CAPTURE_REQUESTED;
    return BSP_STATUS_BUSY;
}

bsp_status_t app_calibration_workflow_cancel(app_calibration_workflow_t *workflow)
{
    if (workflow == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!app_calibration_workflow_active(workflow))
    {
        workflow->state = APP_CAL_WORKFLOW_CANCELED;
        workflow->result = APP_CAL_WORKFLOW_RESULT_CANCELED;
        return BSP_STATUS_OK;
    }
    if (workflow->state == APP_CAL_WORKFLOW_WAIT_CAPTURE)
    {
        workflow->state = APP_CAL_WORKFLOW_CANCELING;
        return BSP_STATUS_BUSY;
    }
    app_calibration_workflow_cancel_complete(workflow);
    return BSP_STATUS_OK;
}

void app_calibration_workflow_cancel_complete(app_calibration_workflow_t *workflow)
{
    if (workflow != NULL)
    {
        workflow->state = APP_CAL_WORKFLOW_CANCELED;
        workflow->result = APP_CAL_WORKFLOW_RESULT_CANCELED;
        workflow->waiting_capture = false;
        workflow->last_reject_flags = APP_CAL_REJECT_NONE;
    }
}

static app_cal_capture_sample_t make_sample_from_result(const hw_metrology_block_t *block,
                                                        const app_cal_workflow_request_t *request,
                                                        const measurement_result_t *result,
                                                        const measurement_dsp_config_t *config)
{
    const measurement_complex_t vmid = result->phasors.vmid;
    const measurement_complex_t source = measurement_complex_sub(result->phasors.vexc_1, vmid);
    const measurement_complex_t ret_1x = measurement_complex_sub(result->phasors.ret_1x, vmid);
    const measurement_complex_t ret_hg = measurement_complex_sub(result->phasors.ret_hg_reconstructed, vmid);
    measurement_impedance_result_t z_1x =
        measurement_compute_impedance(source, ret_1x, config->zref_ohms, config, MEASUREMENT_RETURN_1X);
    measurement_impedance_result_t z_hg =
        measurement_compute_impedance(source, ret_hg, config->zref_ohms, config, MEASUREMENT_RETURN_HG);

    app_cal_capture_sample_t sample = {
        .key = request->key,
        .standard_type = request->standard.type,
        .timestamp_ms = block->permit_validate_ms,
        .temperature_mC = request->temperature_mC,
        .source_v = source,
        .ret_1x_v = ret_1x,
        .ret_hg_v = ret_hg,
        .z_1x_ohms = z_1x.z_ohms,
        .z_hg_ohms = z_hg.z_ohms,
        .source_peak_v = measurement_complex_mag(source),
        .ret_1x_peak_v = measurement_complex_mag(ret_1x),
        .ret_hg_peak_v = measurement_complex_mag(ret_hg),
        .denominator_1x_peak_v = measurement_complex_mag(measurement_complex_sub(source, ret_1x)),
        .denominator_hg_peak_v = measurement_complex_mag(measurement_complex_sub(source, ret_hg)),
        .ret_1x_usable = result->ret_1x_quality.usable && !result->ret_1x_quality.clipped,
        .ret_hg_usable = result->ret_hg_quality.usable && !result->ret_hg_quality.clipped,
        .z_1x_valid = z_1x.status == MEASUREMENT_STATUS_OK,
        .z_hg_valid = z_hg.status == MEASUREMENT_STATUS_OK,
        .clipped = block->clipped || result->phasors.clipped ||
                   result->ret_1x_quality.clipped ||
                   result->ret_hg_quality.clipped,
        .reject_flags = APP_CAL_REJECT_NONE,
    };
    if ((result->status != MEASUREMENT_STATUS_OK) && (result->status != MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL))
    {
        sample.reject_flags |= APP_CAL_REJECT_DSP;
    }
    if ((z_1x.status == MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL) ||
        (z_hg.status == MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL))
    {
        sample.reject_flags |= APP_CAL_REJECT_DENOMINATOR_TOO_SMALL;
    }
    return sample;
}

bsp_status_t app_calibration_workflow_sample_from_block(const hw_metrology_block_t *block,
                                                        const app_cal_workflow_request_t *request,
                                                        app_cal_capture_sample_t *sample)
{
    if ((block == NULL) || (request == NULL) || (sample == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *sample = (app_cal_capture_sample_t){0};
    if (!block->valid || (block->raw_words == NULL) || block->dma_error || block->timeout)
    {
        sample->key = request->key;
        sample->standard_type = request->standard.type;
        sample->reject_flags = APP_CAL_REJECT_PHASE05;
        return BSP_STATUS_ERROR;
    }

    const measurement_adc_calibration_t adc = measurement_adc_calibration_ideal();
    measurement_dsp_config_t config = measurement_dsp_config_ideal(request->key.range_id);
    measurement_result_t result;
    const bsp_status_t status = measurement_process_block(block, &adc, &config, &result);
    if (status != BSP_STATUS_OK)
    {
        *sample = (app_cal_capture_sample_t){
            .key = request->key,
            .standard_type = request->standard.type,
            .timestamp_ms = block->permit_validate_ms,
            .temperature_mC = request->temperature_mC,
            .source_peak_v = result.phasors.vexc_1_peak_v,
            .ret_1x_peak_v = result.ret_1x_quality.signal_peak_v,
            .ret_hg_peak_v = result.ret_hg_quality.signal_peak_v,
            .ret_1x_usable = result.ret_1x_quality.usable && !result.ret_1x_quality.clipped,
            .ret_hg_usable = result.ret_hg_quality.usable && !result.ret_hg_quality.clipped,
            .clipped = block->clipped || result.phasors.clipped,
            .reject_flags = (status == BSP_STATUS_OK) ? APP_CAL_REJECT_NONE : APP_CAL_REJECT_DSP,
        };
        return status;
    }
    *sample = make_sample_from_result(block, request, &result, &config);
    return BSP_STATUS_OK;
}

app_cal_workflow_state_t app_calibration_workflow_state(const app_calibration_workflow_t *workflow)
{
    return (workflow == NULL) ? APP_CAL_WORKFLOW_IDLE : workflow->state;
}

app_cal_workflow_result_t app_calibration_workflow_result(const app_calibration_workflow_t *workflow)
{
    return (workflow == NULL) ? APP_CAL_WORKFLOW_RESULT_INVALID_REQUEST : workflow->result;
}

const app_cal_evidence_t *app_calibration_workflow_evidence(const app_calibration_workflow_t *workflow)
{
    return (workflow == NULL) ? NULL : &workflow->evidence;
}

uint32_t app_calibration_workflow_last_reject_flags(const app_calibration_workflow_t *workflow)
{
    return (workflow == NULL) ? APP_CAL_REJECT_DSP : workflow->last_reject_flags;
}

const char *app_cal_standard_type_string(app_cal_standard_type_t type)
{
    switch (type)
    {
    case APP_CAL_STANDARD_OPEN:
        return "OPEN";
    case APP_CAL_STANDARD_SHORT:
        return "SHORT";
    case APP_CAL_STANDARD_LOAD:
        return "LOAD";
    default:
        return "UNKNOWN";
    }
}

const char *app_cal_workflow_state_string(app_cal_workflow_state_t state)
{
    switch (state)
    {
    case APP_CAL_WORKFLOW_IDLE:
        return "IDLE";
    case APP_CAL_WORKFLOW_CAPTURE_REQUESTED:
        return "CAPTURE_REQUESTED";
    case APP_CAL_WORKFLOW_WAIT_CAPTURE:
        return "WAIT_CAPTURE";
    case APP_CAL_WORKFLOW_COMPLETE:
        return "COMPLETE";
    case APP_CAL_WORKFLOW_FAILED:
        return "FAILED";
    case APP_CAL_WORKFLOW_CANCELING:
        return "CANCELING";
    case APP_CAL_WORKFLOW_CANCELED:
    default:
        return "CANCELED";
    }
}

const char *app_cal_workflow_result_string(app_cal_workflow_result_t result)
{
    switch (result)
    {
    case APP_CAL_WORKFLOW_RESULT_NONE:
        return "NONE";
    case APP_CAL_WORKFLOW_RESULT_OK:
        return "OK";
    case APP_CAL_WORKFLOW_RESULT_INVALID_REQUEST:
        return "INVALID_REQUEST";
    case APP_CAL_WORKFLOW_RESULT_UNSUPPORTED_CONDITION:
        return "UNSUPPORTED_CONDITION";
    case APP_CAL_WORKFLOW_RESULT_UNSTABLE:
        return "UNSTABLE";
    case APP_CAL_WORKFLOW_RESULT_TOO_MANY_REJECTS:
        return "TOO_MANY_REJECTS";
    case APP_CAL_WORKFLOW_RESULT_PHASE05_ERROR:
        return "PHASE05_ERROR";
    case APP_CAL_WORKFLOW_RESULT_SAFETY_ABORT:
        return "SAFETY_ABORT";
    case APP_CAL_WORKFLOW_RESULT_CANCELED:
    default:
        return "CANCELED";
    }
}

uint32_t app_calibration_workflow_context_size_bytes(void)
{
    return (uint32_t)sizeof(app_calibration_workflow_t);
}

uint32_t app_cal_evidence_size_bytes(void)
{
    return (uint32_t)sizeof(app_cal_evidence_t);
}
