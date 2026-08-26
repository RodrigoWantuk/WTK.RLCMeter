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

static void temperature_push(app_cal_temperature_evidence_t *stats, int32_t temperature_mC)
{
    if (stats == NULL)
    {
        return;
    }
    if (stats->count == 0u)
    {
        stats->mean_mC = temperature_mC;
        stats->min_mC = temperature_mC;
        stats->max_mC = temperature_mC;
        stats->valid = true;
    }
    else
    {
        const int32_t delta = temperature_mC - stats->mean_mC;
        stats->mean_mC += delta / (int32_t)((uint32_t)stats->count + 1u);
        if (temperature_mC < stats->min_mC)
        {
            stats->min_mC = temperature_mC;
        }
        if (temperature_mC > stats->max_mC)
        {
            stats->max_mC = temperature_mC;
        }
    }
    stats->count++;
}

static bool path_has_valid_observable(const app_cal_capture_sample_t *sample,
                                      app_cal_standard_type_t standard,
                                      measurement_return_channel_t channel)
{
    if (sample == NULL)
    {
        return false;
    }
    const bool hg = channel == MEASUREMENT_RETURN_HG;
    const bool usable = hg ? sample->ret_hg_usable : sample->ret_1x_usable;
    if (!usable)
    {
        return false;
    }
    if (standard == APP_CAL_STANDARD_OPEN)
    {
        return hg ? (sample->open_y_hg_valid && finite_complex(sample->open_y_hg)) :
                    (sample->open_y_1x_valid && finite_complex(sample->open_y_1x));
    }
    const float threshold = (float)APP_CAL_WORKFLOW_DENOMINATOR_MIN_UV_PEAK / 1000000.0f;
    const float denominator = hg ? sample->denominator_hg_peak_v : sample->denominator_1x_peak_v;
    if (denominator < threshold)
    {
        return false;
    }
    return hg ? (sample->z_hg_valid && finite_complex(sample->z_hg_ohms)) :
                (sample->z_1x_valid && finite_complex(sample->z_1x_ohms));
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
        !finite_complex(sample->vexc_1_v) ||
        !finite_complex(sample->vexc_2_v) ||
        !finite_complex(sample->ret_1x_v) ||
        !finite_complex(sample->ret_hg_raw_v) ||
        !finite_complex(sample->ret_hg_reconstructed_v) ||
        !finite_complex(sample->ret_hg_v) ||
        !finite_float(sample->source_peak_v) ||
        !finite_float(sample->vexc_1_peak_v) ||
        !finite_float(sample->vexc_2_peak_v) ||
        !finite_float(sample->ret_1x_peak_v) ||
        !finite_float(sample->ret_hg_raw_peak_v) ||
        !finite_float(sample->ret_hg_reconstructed_peak_v) ||
        !finite_float(sample->ret_hg_peak_v))
    {
        flags |= APP_CAL_REJECT_NONFINITE;
    }
    const float source_threshold = (float)APP_CAL_WORKFLOW_SOURCE_MIN_UV_PEAK / 1000000.0f;
    if ((sample->vexc_1_peak_v < source_threshold) &&
        (sample->vexc_2_peak_v < source_threshold))
    {
        flags |= APP_CAL_REJECT_SOURCE_TOO_SMALL;
    }
    const bool usable_1x = path_has_valid_observable(sample, standard, MEASUREMENT_RETURN_1X);
    const bool usable_hg = path_has_valid_observable(sample, standard, MEASUREMENT_RETURN_HG);
    if (!usable_1x && !usable_hg)
    {
        flags |= APP_CAL_REJECT_NO_USABLE_CHANNEL;
        if (sample->ret_1x_clipped || sample->ret_hg_clipped || sample->clipped)
        {
            flags |= APP_CAL_REJECT_CLIPPED;
        }
    }
    if (standard != APP_CAL_STANDARD_OPEN)
    {
        if (!usable_1x && !usable_hg)
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

    const bool open = workflow->request.standard.type == APP_CAL_STANDARD_OPEN;
    const bool ret_1x_stable = open ? stats_stable(&workflow->evidence.open_y_1x) :
                                      stats_stable(&workflow->evidence.z_1x);
    const bool ret_hg_stable = open ? stats_stable(&workflow->evidence.open_y_hg) :
                                     stats_stable(&workflow->evidence.z_hg);
    const bool source_1_stable = stats_stable(&workflow->evidence.source_1);
    const bool source_2_stable = stats_stable(&workflow->evidence.source_2);

    workflow->evidence.ret_1x_path.stable = ret_1x_stable && source_1_stable;
    workflow->evidence.ret_hg_path.stable = ret_hg_stable && source_2_stable;
    workflow->evidence.ret_1x_path.insufficient =
        workflow->evidence.ret_1x_path.usable_count < APP_CAL_WORKFLOW_REQUIRED_ACCEPTED;
    workflow->evidence.ret_hg_path.insufficient =
        workflow->evidence.ret_hg_path.usable_count < APP_CAL_WORKFLOW_REQUIRED_ACCEPTED;
    workflow->evidence.ret_1x_path.evidence_valid =
        !workflow->evidence.ret_1x_path.insufficient && workflow->evidence.ret_1x_path.stable;
    workflow->evidence.ret_hg_path.evidence_valid =
        !workflow->evidence.ret_hg_path.insufficient && workflow->evidence.ret_hg_path.stable;
    workflow->evidence.ret_1x_evidence_valid = workflow->evidence.ret_1x_path.evidence_valid;
    workflow->evidence.ret_hg_evidence_valid = workflow->evidence.ret_hg_path.evidence_valid;
    workflow->evidence.hg_overlap_valid =
        (workflow->evidence.hg_observed_transfer.count >= APP_CAL_WORKFLOW_REQUIRED_ACCEPTED) &&
        stats_stable(&workflow->evidence.hg_observed_transfer);
    workflow->evidence.stable = workflow->evidence.ret_1x_evidence_valid ||
                                workflow->evidence.ret_hg_evidence_valid;
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
    workflow->evidence.last_temperature_valid = sample->temperature_valid;
    const bool open = workflow->request.standard.type == APP_CAL_STANDARD_OPEN;
    if (sample->temperature_valid)
    {
        temperature_push(&workflow->evidence.temperature, sample->temperature_mC);
    }
    stats_push(&workflow->evidence.source, sample->source_v);
    stats_push(&workflow->evidence.source_1, sample->vexc_1_v);
    stats_push(&workflow->evidence.source_2, sample->vexc_2_v);
    if (sample->ret_1x_usable)
    {
        workflow->evidence.ret_1x_path.sample_count++;
        stats_push(&workflow->evidence.ret_1x, sample->ret_1x_v);
        if (open && sample->open_y_1x_valid)
        {
            stats_push(&workflow->evidence.open_y_1x, sample->open_y_1x);
            workflow->evidence.ret_1x_path.usable_count++;
        }
        else if (sample->z_1x_valid)
        {
            stats_push(&workflow->evidence.z_1x, sample->z_1x_ohms);
            workflow->evidence.ret_1x_path.usable_count++;
        }
    }
    else
    {
        workflow->evidence.ret_1x_consistent = false;
        workflow->evidence.ret_1x_path.rejected_count++;
    }
    if (sample->ret_hg_usable)
    {
        workflow->evidence.ret_hg_path.sample_count++;
        stats_push(&workflow->evidence.ret_hg_raw, sample->ret_hg_raw_v);
        stats_push(&workflow->evidence.ret_hg_reconstructed, sample->ret_hg_reconstructed_v);
        stats_push(&workflow->evidence.ret_hg, sample->ret_hg_v);
        if (open && sample->open_y_hg_valid)
        {
            stats_push(&workflow->evidence.open_y_hg, sample->open_y_hg);
            workflow->evidence.ret_hg_path.usable_count++;
        }
        else if (sample->z_hg_valid)
        {
            stats_push(&workflow->evidence.z_hg, sample->z_hg_ohms);
            workflow->evidence.ret_hg_path.usable_count++;
        }
    }
    else
    {
        workflow->evidence.ret_hg_consistent = false;
        workflow->evidence.ret_hg_path.rejected_count++;
    }
    if (sample->hg_overlap_valid)
    {
        stats_push(&workflow->evidence.hg_observed_transfer, sample->hg_observed_transfer);
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

static bool stream_clipped(const hw_metrology_block_t *block, hw_metrology_stream_t stream)
{
    return (block != NULL) && (stream < HW_METROLOGY_STREAM_COUNT) &&
           block->streams[stream].hard_clipped;
}

static bool compute_ratio(measurement_complex_t numerator,
                          measurement_complex_t denominator,
                          measurement_complex_t *out)
{
    return measurement_complex_div(numerator, denominator, out) == MEASUREMENT_STATUS_OK;
}

static bool compute_provisional_z(measurement_complex_t vx,
                                  measurement_complex_t denominator,
                                  measurement_complex_t zref,
                                  float denominator_min,
                                  measurement_complex_t *out)
{
    if ((out == NULL) || measurement_complex_near_zero(denominator, denominator_min))
    {
        return false;
    }
    measurement_complex_t ratio = {0.0f, 0.0f};
    if (!compute_ratio(vx, denominator, &ratio))
    {
        return false;
    }
    *out = measurement_complex_mul(zref, ratio);
    return measurement_complex_is_finite(*out);
}

static app_cal_capture_sample_t make_sample_from_phasors(const hw_metrology_block_t *block,
                                                         const app_cal_workflow_request_t *request,
                                                         const measurement_phasor_set_t *phasors,
                                                         const measurement_dsp_config_t *config)
{
    const measurement_complex_t vmid = phasors->vmid;
    const measurement_complex_t vexc_1 = measurement_complex_sub(phasors->vexc_1, vmid);
    const measurement_complex_t vexc_2 = measurement_complex_sub(phasors->vexc_2, vmid);
    const measurement_complex_t ret_1x = measurement_complex_sub(phasors->ret_1x, vmid);
    const measurement_complex_t ret_hg_raw = measurement_complex_sub(phasors->ret_hg, vmid);
    const measurement_complex_t ret_hg_reconstructed =
        measurement_complex_sub(phasors->ret_hg_reconstructed, vmid);
    const measurement_complex_t denominator_1x = measurement_complex_sub(vexc_1, ret_1x);
    const measurement_complex_t denominator_hg = measurement_complex_sub(vexc_2, ret_hg_reconstructed);
    measurement_complex_t open_y_1x = {0.0f, 0.0f};
    measurement_complex_t open_y_hg = {0.0f, 0.0f};
    measurement_complex_t h_hg = {0.0f, 0.0f};
    measurement_complex_t z_1x = {0.0f, 0.0f};
    measurement_complex_t z_hg = {0.0f, 0.0f};
    const bool open_y_1x_valid = compute_ratio(denominator_1x, ret_1x, &open_y_1x);
    const bool open_y_hg_valid = compute_ratio(denominator_hg, ret_hg_reconstructed, &open_y_hg);
    measurement_complex_t t_1x = {0.0f, 0.0f};
    measurement_complex_t t_hg_raw = {0.0f, 0.0f};
    const bool t_1x_valid = compute_ratio(ret_1x, vexc_1, &t_1x);
    const bool t_hg_raw_valid = compute_ratio(ret_hg_raw, vexc_2, &t_hg_raw);
    const bool raw_hg_ratio_valid = t_1x_valid &&
                                    t_hg_raw_valid &&
                                    compute_ratio(t_hg_raw, t_1x, &h_hg);
    const bool z_1x_valid = compute_provisional_z(ret_1x,
                                                  denominator_1x,
                                                  config->zref_ohms,
                                                  config->denominator_min_v_peak,
                                                  &z_1x);
    const bool z_hg_valid = compute_provisional_z(ret_hg_reconstructed,
                                                 denominator_hg,
                                                 config->zref_ohms,
                                                 config->denominator_min_v_peak,
                                                 &z_hg);
    const bool vmid_clipped = stream_clipped(block, HW_METROLOGY_STREAM_VMID_ADC1) ||
                              stream_clipped(block, HW_METROLOGY_STREAM_VMID_ADC2);
    const bool ret_1x_clipped = stream_clipped(block, HW_METROLOGY_STREAM_RET_1X) ||
                                stream_clipped(block, HW_METROLOGY_STREAM_VEXC_1) ||
                                vmid_clipped;
    const bool ret_hg_clipped = stream_clipped(block, HW_METROLOGY_STREAM_RET_HG) ||
                                stream_clipped(block, HW_METROLOGY_STREAM_VEXC_2) ||
                                vmid_clipped;
    const measurement_channel_quality_t ret_1x_quality =
        measurement_channel_quality(&ret_1x, ret_1x_clipped, true, config->return_min_v_peak);
    const measurement_channel_quality_t ret_hg_quality =
        measurement_channel_quality(&ret_hg_reconstructed,
                                    ret_hg_clipped,
                                    !measurement_complex_near_zero(config->ret_hg_transfer, 1.0e-6f),
                                    config->return_min_v_peak);
    const bool hg_overlap_valid = raw_hg_ratio_valid && ret_1x_quality.usable && ret_hg_quality.usable;

    app_cal_capture_sample_t sample = {
        .key = request->key,
        .standard_type = request->standard.type,
        .timestamp_ms = block->permit_validate_ms,
        .temperature_mC = request->temperature_mC,
        .temperature_valid = request->temperature_valid,
        .source_v = vexc_1,
        .vexc_1_v = vexc_1,
        .vexc_2_v = vexc_2,
        .ret_1x_v = ret_1x,
        .ret_hg_raw_v = ret_hg_raw,
        .ret_hg_reconstructed_v = ret_hg_reconstructed,
        .ret_hg_v = ret_hg_reconstructed,
        .vmid_adc1_v = phasors->vmid_adc1,
        .vmid_adc2_v = phasors->vmid_adc2,
        .open_y_1x = open_y_1x,
        .open_y_hg = open_y_hg,
        .hg_observed_transfer = h_hg,
        .z_1x_ohms = z_1x,
        .z_hg_ohms = z_hg,
        .source_peak_v = max_float(measurement_complex_mag(vexc_1), measurement_complex_mag(vexc_2)),
        .vexc_1_peak_v = measurement_complex_mag(vexc_1),
        .vexc_2_peak_v = measurement_complex_mag(vexc_2),
        .ret_1x_peak_v = measurement_complex_mag(ret_1x),
        .ret_hg_raw_peak_v = measurement_complex_mag(ret_hg_raw),
        .ret_hg_reconstructed_peak_v = measurement_complex_mag(ret_hg_reconstructed),
        .ret_hg_peak_v = measurement_complex_mag(ret_hg_reconstructed),
        .denominator_1x_peak_v = measurement_complex_mag(denominator_1x),
        .denominator_hg_peak_v = measurement_complex_mag(denominator_hg),
        .ret_1x_clipped = ret_1x_clipped,
        .ret_hg_clipped = ret_hg_clipped,
        .ret_1x_usable = ret_1x_quality.usable,
        .ret_hg_usable = ret_hg_quality.usable,
        .open_y_1x_valid = open_y_1x_valid,
        .open_y_hg_valid = open_y_hg_valid,
        .hg_overlap_valid = hg_overlap_valid,
        .z_1x_valid = z_1x_valid,
        .z_hg_valid = z_hg_valid,
        .clipped = vmid_clipped,
        .reject_flags = APP_CAL_REJECT_NONE,
    };
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
    measurement_phasor_set_t phasors = {0};
    const bsp_status_t status = measurement_extract_phasors(block, &adc, &config, &phasors);
    if (status != BSP_STATUS_OK)
    {
        *sample = (app_cal_capture_sample_t){
            .key = request->key,
            .standard_type = request->standard.type,
            .timestamp_ms = block->permit_validate_ms,
            .temperature_mC = request->temperature_mC,
            .temperature_valid = request->temperature_valid,
            .clipped = block->clipped,
            .reject_flags = APP_CAL_REJECT_DSP,
        };
        return status;
    }
    *sample = make_sample_from_phasors(block, request, &phasors, &config);
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
