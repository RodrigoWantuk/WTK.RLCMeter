#include "measurement/measurement_dsp.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

enum
{
    ADC_CAL_STREAM_COUNT = 6u,
};

#define MEAS_PI_F (3.14159265358979323846f)
#define MEAS_TWO_PI_F (6.28318530717958647692f)
#define MEAS_IDEAL_ADC_SCALE_V (3.3f / 4095.0f)
#define MEAS_HG_NOMINAL_GAIN (15.4680851064f)
#define MEAS_DEFAULT_NEAR_ZERO (1.0e-6f)

typedef struct
{
    float cos_step;
    float sin_step;
    uint16_t samples_per_cycle;
} reference_profile_t;

typedef struct
{
    float re_sum;
    float im_sum;
} phasor_accumulator_t;

static bool finite_float(float value)
{
    return isfinite(value) != 0;
}

static float abs_float(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float max_float(float a, float b)
{
    return (a > b) ? a : b;
}

static float sqrt_approx(float value)
{
    if (!(value > 0.0f))
    {
        return 0.0f;
    }

    float estimate = (value >= 1.0f) ? value : 1.0f;
    for (uint32_t i = 0u; i < 10u; i++)
    {
        estimate = 0.5f * (estimate + (value / estimate));
    }
    return estimate;
}

static float atan_approx(float z)
{
    const float az = abs_float(z);
    const float c1 = 0.2447f;
    const float c2 = 0.0663f;
    float angle = (MEAS_PI_F / 4.0f) * z - z * (az - 1.0f) * (c1 + (c2 * az));
    return angle;
}

static float atan2_approx(float y, float x)
{
    if (x > 0.0f)
    {
        return atan_approx(y / x);
    }
    if (x < 0.0f)
    {
        return (y >= 0.0f) ? (atan_approx(y / x) + MEAS_PI_F) :
                             (atan_approx(y / x) - MEAS_PI_F);
    }
    if (y > 0.0f)
    {
        return MEAS_PI_F / 2.0f;
    }
    if (y < 0.0f)
    {
        return -MEAS_PI_F / 2.0f;
    }
    return 0.0f;
}

measurement_complex_t measurement_complex(float re, float im)
{
    measurement_complex_t value = {.re = re, .im = im};
    return value;
}

measurement_complex_t measurement_complex_add(measurement_complex_t a, measurement_complex_t b)
{
    return measurement_complex(a.re + b.re, a.im + b.im);
}

measurement_complex_t measurement_complex_sub(measurement_complex_t a, measurement_complex_t b)
{
    return measurement_complex(a.re - b.re, a.im - b.im);
}

measurement_complex_t measurement_complex_mul(measurement_complex_t a, measurement_complex_t b)
{
    return measurement_complex((a.re * b.re) - (a.im * b.im),
                               (a.re * b.im) + (a.im * b.re));
}

measurement_status_t measurement_complex_div(measurement_complex_t a,
                                             measurement_complex_t b,
                                             measurement_complex_t *out)
{
    if (out == NULL)
    {
        return MEASUREMENT_STATUS_INVALID_ARG;
    }
    if (!measurement_complex_is_finite(a) || !measurement_complex_is_finite(b))
    {
        *out = measurement_complex(0.0f, 0.0f);
        return MEASUREMENT_STATUS_NONFINITE;
    }

    const float denom = (b.re * b.re) + (b.im * b.im);
    if (!(denom > (MEAS_DEFAULT_NEAR_ZERO * MEAS_DEFAULT_NEAR_ZERO)))
    {
        *out = measurement_complex(0.0f, 0.0f);
        return MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL;
    }

    *out = measurement_complex(((a.re * b.re) + (a.im * b.im)) / denom,
                               ((a.im * b.re) - (a.re * b.im)) / denom);
    return MEASUREMENT_STATUS_OK;
}

float measurement_complex_mag(measurement_complex_t value)
{
    return sqrt_approx((value.re * value.re) + (value.im * value.im));
}

float measurement_complex_phase_rad(measurement_complex_t value)
{
    return atan2_approx(value.im, value.re);
}

bool measurement_complex_is_finite(measurement_complex_t value)
{
    return finite_float(value.re) && finite_float(value.im);
}

bool measurement_complex_near_zero(measurement_complex_t value, float threshold)
{
    const float limit = (threshold > 0.0f) ? threshold : MEAS_DEFAULT_NEAR_ZERO;
    return ((value.re * value.re) + (value.im * value.im)) <= (limit * limit);
}

measurement_adc_calibration_t measurement_adc_calibration_ideal(void)
{
    const measurement_adc_scale_t scale = {
        .code_to_volts = MEAS_IDEAL_ADC_SCALE_V,
        .offset_volts = 0.0f,
    };
    measurement_adc_calibration_t cal = {
        .vexc_1 = scale,
        .ret_1x = scale,
        .vexc_2 = scale,
        .ret_hg = scale,
        .vmid_adc1 = scale,
        .vmid_adc2 = scale,
    };
    return cal;
}

static measurement_complex_t ideal_zref(hw_range_id_t range_id)
{
    switch (range_id)
    {
    case HW_RANGE_ID_10R:
        return measurement_complex(10.0f, 0.0f);
    case HW_RANGE_ID_100R:
        return measurement_complex(100.0f, 0.0f);
    case HW_RANGE_ID_1K:
        return measurement_complex(1000.0f, 0.0f);
    case HW_RANGE_ID_10K:
        return measurement_complex(10000.0f, 0.0f);
    case HW_RANGE_ID_100K:
        return measurement_complex(100000.0f, 0.0f);
    case HW_RANGE_ID_1M:
        return measurement_complex(1000000.0f, 0.0f);
    case HW_RANGE_ID_INVALID:
    default:
        return measurement_complex(0.0f, 0.0f);
    }
}

measurement_dsp_config_t measurement_dsp_config_ideal(hw_range_id_t range_id)
{
    measurement_dsp_config_t config = {
        .ret_hg_transfer = {.re = MEAS_HG_NOMINAL_GAIN, .im = 0.0f},
        .zref_ohms = ideal_zref(range_id),
        .source_min_v_peak = 0.001f,
        .return_min_v_peak = 0.00005f,
        .denominator_min_v_peak = 0.00005f,
        .reactance_min_ohms = 0.001f,
        .interpretation_ratio_resistive_max = 0.10f,
        .interpretation_ratio_reactive_min = 0.25f,
    };
    return config;
}

static bool reference_profile(const hw_metrology_block_t *block, reference_profile_t *profile)
{
    if ((block == NULL) || (profile == NULL))
    {
        return false;
    }
    if ((block->sample_count != HW_METROLOGY_SAMPLES_PER_BLOCK) ||
        (block->cycles_per_block == 0u) ||
        (block->samples_per_cycle == 0u))
    {
        return false;
    }

    switch (block->samples_per_cycle)
    {
    case MEASUREMENT_PHASE_REF_POINTS_64:
        profile->cos_step = 0.9951847267f;
        profile->sin_step = 0.0980171403f;
        profile->samples_per_cycle = MEASUREMENT_PHASE_REF_POINTS_64;
        return true;
    case MEASUREMENT_PHASE_REF_POINTS_16:
        profile->cos_step = 0.9238795325f;
        profile->sin_step = 0.3826834324f;
        profile->samples_per_cycle = MEASUREMENT_PHASE_REF_POINTS_16;
        return true;
    default:
        return false;
    }
}

static float scale_raw(uint16_t raw, measurement_adc_scale_t scale)
{
    return ((float)raw * scale.code_to_volts) + scale.offset_volts;
}

static void accumulate(phasor_accumulator_t *acc, float sample_v, float cos_ref, float sin_ref)
{
    acc->re_sum += sample_v * cos_ref;
    acc->im_sum -= sample_v * sin_ref;
}

static measurement_complex_t finalize_accumulator(phasor_accumulator_t acc, uint16_t sample_count)
{
    const float norm = 2.0f / (float)sample_count;
    return measurement_complex(acc.re_sum * norm, acc.im_sum * norm);
}

bsp_status_t measurement_extract_phasors(const hw_metrology_block_t *block,
                                         const measurement_adc_calibration_t *adc_cal,
                                         const measurement_dsp_config_t *config,
                                         measurement_phasor_set_t *phasors)
{
    if ((block == NULL) || (adc_cal == NULL) || (config == NULL) || (phasors == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if ((block->raw_words == NULL) || !block->valid)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    reference_profile_t ref;
    if (!reference_profile(block, &ref))
    {
        return BSP_STATUS_NOT_SUPPORTED;
    }

    phasor_accumulator_t vexc_1 = {0.0f, 0.0f};
    phasor_accumulator_t ret_1x = {0.0f, 0.0f};
    phasor_accumulator_t vexc_2 = {0.0f, 0.0f};
    phasor_accumulator_t ret_hg = {0.0f, 0.0f};
    phasor_accumulator_t vmid_1 = {0.0f, 0.0f};
    phasor_accumulator_t vmid_2 = {0.0f, 0.0f};
    float cos_ref = 1.0f;
    float sin_ref = 0.0f;

    for (uint32_t n = 0u; n < (uint32_t)block->sample_count; n++)
    {
        hw_metrology_sample_t sample;
        if (hw_metrology_unpack_sample(block->raw_words, n, &sample) != BSP_STATUS_OK)
        {
            return BSP_STATUS_ERROR;
        }
        accumulate(&vexc_1, scale_raw(sample.vexc_1, adc_cal->vexc_1), cos_ref, sin_ref);
        accumulate(&ret_1x, scale_raw(sample.ret_1x, adc_cal->ret_1x), cos_ref, sin_ref);
        accumulate(&vexc_2, scale_raw(sample.vexc_2, adc_cal->vexc_2), cos_ref, sin_ref);
        accumulate(&ret_hg, scale_raw(sample.ret_hg, adc_cal->ret_hg), cos_ref, sin_ref);
        accumulate(&vmid_1, scale_raw(sample.vmid_adc1, adc_cal->vmid_adc1), cos_ref, sin_ref);
        accumulate(&vmid_2, scale_raw(sample.vmid_adc2, adc_cal->vmid_adc2), cos_ref, sin_ref);

        const float next_cos = (cos_ref * ref.cos_step) - (sin_ref * ref.sin_step);
        const float next_sin = (sin_ref * ref.cos_step) + (cos_ref * ref.sin_step);
        cos_ref = next_cos;
        sin_ref = next_sin;
    }

    phasors->vexc_1 = finalize_accumulator(vexc_1, block->sample_count);
    phasors->ret_1x = finalize_accumulator(ret_1x, block->sample_count);
    phasors->vexc_2 = finalize_accumulator(vexc_2, block->sample_count);
    phasors->ret_hg = finalize_accumulator(ret_hg, block->sample_count);
    phasors->vmid_adc1 = finalize_accumulator(vmid_1, block->sample_count);
    phasors->vmid_adc2 = finalize_accumulator(vmid_2, block->sample_count);
    phasors->vmid = measurement_complex_mul(measurement_complex_add(phasors->vmid_adc1, phasors->vmid_adc2),
                                            measurement_complex(0.5f, 0.0f));

    measurement_complex_t hg_delta = measurement_complex_sub(phasors->ret_hg, phasors->vmid);
    measurement_complex_t hg_scaled = {0.0f, 0.0f};
    if (measurement_complex_div(hg_delta, config->ret_hg_transfer, &hg_scaled) != MEASUREMENT_STATUS_OK)
    {
        phasors->valid = false;
        return BSP_STATUS_ERROR;
    }
    phasors->ret_hg_reconstructed = measurement_complex_add(phasors->vmid, hg_scaled);
    phasors->vexc_1_peak_v = measurement_complex_mag(measurement_complex_sub(phasors->vexc_1, phasors->vmid));
    phasors->ret_1x_peak_v = measurement_complex_mag(measurement_complex_sub(phasors->ret_1x, phasors->vmid));
    phasors->vexc_2_peak_v = measurement_complex_mag(measurement_complex_sub(phasors->vexc_2, phasors->vmid));
    phasors->ret_hg_peak_v =
        measurement_complex_mag(measurement_complex_sub(phasors->ret_hg_reconstructed, phasors->vmid));
    phasors->clipped = block->clipped;
    phasors->valid = !block->dma_error && !block->timeout;
    return phasors->valid ? BSP_STATUS_OK : BSP_STATUS_ERROR;
}

measurement_channel_quality_t measurement_channel_quality(const measurement_complex_t *signal,
                                                          bool clipped,
                                                          bool calibration_valid,
                                                          float min_signal_v_peak)
{
    measurement_channel_quality_t quality = {
        .usable = false,
        .clipped = clipped,
        .calibration_valid = calibration_valid,
        .signal_too_small = true,
        .signal_peak_v = 0.0f,
    };
    if (signal == NULL)
    {
        return quality;
    }
    quality.signal_peak_v = measurement_complex_mag(*signal);
    quality.signal_too_small = quality.signal_peak_v < min_signal_v_peak;
    quality.usable = !clipped && calibration_valid && !quality.signal_too_small &&
                     measurement_complex_is_finite(*signal);
    return quality;
}

measurement_return_channel_t measurement_select_return_channel(
    const measurement_channel_quality_t *ret_1x,
    const measurement_channel_quality_t *ret_hg)
{
    if ((ret_hg != NULL) && ret_hg->usable)
    {
        return MEASUREMENT_RETURN_HG;
    }
    (void)ret_1x;
    return MEASUREMENT_RETURN_1X;
}

measurement_impedance_result_t measurement_compute_impedance(measurement_complex_t vs_v,
                                                             measurement_complex_t vx_v,
                                                             measurement_complex_t zref_ohms,
                                                             const measurement_dsp_config_t *config,
                                                             measurement_return_channel_t channel)
{
    measurement_impedance_result_t result = {
        .status = MEASUREMENT_STATUS_OK,
        .channel = channel,
        .vs_v = vs_v,
        .vx_v = vx_v,
        .z_ohms = {0.0f, 0.0f},
        .open_like = false,
        .short_like = false,
    };
    if (config == NULL)
    {
        result.status = MEASUREMENT_STATUS_INVALID_ARG;
        return result;
    }
    if (!measurement_complex_is_finite(vs_v) ||
        !measurement_complex_is_finite(vx_v) ||
        !measurement_complex_is_finite(zref_ohms))
    {
        result.status = MEASUREMENT_STATUS_NONFINITE;
        return result;
    }
    if (measurement_complex_near_zero(zref_ohms, MEAS_DEFAULT_NEAR_ZERO))
    {
        result.status = MEASUREMENT_STATUS_INVALID_ZREF;
        return result;
    }
    if (measurement_complex_near_zero(vs_v, config->source_min_v_peak))
    {
        result.status = MEASUREMENT_STATUS_SOURCE_TOO_SMALL;
        return result;
    }

    const measurement_complex_t denominator = measurement_complex_sub(vs_v, vx_v);
    if (measurement_complex_near_zero(denominator, config->denominator_min_v_peak))
    {
        result.status = MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL;
        result.open_like = true;
        return result;
    }

    if (measurement_complex_near_zero(vx_v, config->return_min_v_peak))
    {
        result.short_like = true;
    }

    measurement_complex_t ratio = {0.0f, 0.0f};
    const measurement_status_t div_status = measurement_complex_div(vx_v, denominator, &ratio);
    if (div_status != MEASUREMENT_STATUS_OK)
    {
        result.status = div_status;
        return result;
    }

    result.z_ohms = measurement_complex_mul(zref_ohms, ratio);
    if (!measurement_complex_is_finite(result.z_ohms))
    {
        result.status = MEASUREMENT_STATUS_NONFINITE;
    }
    return result;
}

measurement_derived_result_t measurement_derive_quantities(measurement_complex_t z_ohms,
                                                           uint32_t frequency_hz,
                                                           const measurement_dsp_config_t *config,
                                                           measurement_status_t impedance_status)
{
    measurement_derived_result_t derived = {
        .valid = false,
        .resistance_ohms = z_ohms.re,
        .reactance_ohms = z_ohms.im,
        .magnitude_ohms = 0.0f,
        .phase_rad = 0.0f,
        .capacitance_valid = false,
        .capacitance_f = 0.0f,
        .inductance_valid = false,
        .inductance_h = 0.0f,
        .q_valid = false,
        .q = 0.0f,
        .d_valid = false,
        .d = 0.0f,
        .interpretation = MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN,
    };
    if ((config == NULL) || (impedance_status != MEASUREMENT_STATUS_OK) ||
        !measurement_complex_is_finite(z_ohms) || (frequency_hz == 0u))
    {
        return derived;
    }

    const float omega = MEAS_TWO_PI_F * (float)frequency_hz;
    const float x_abs = abs_float(z_ohms.im);
    const float r_abs = abs_float(z_ohms.re);
    derived.valid = true;
    derived.magnitude_ohms = measurement_complex_mag(z_ohms);
    derived.phase_rad = measurement_complex_phase_rad(z_ohms);

    if (z_ohms.im > config->reactance_min_ohms)
    {
        derived.inductance_valid = true;
        derived.inductance_h = z_ohms.im / omega;
    }
    else if (z_ohms.im < -config->reactance_min_ohms)
    {
        derived.capacitance_valid = true;
        derived.capacitance_f = -1.0f / (omega * z_ohms.im);
    }

    if ((r_abs > MEAS_DEFAULT_NEAR_ZERO) && (x_abs > config->reactance_min_ohms))
    {
        derived.q_valid = true;
        derived.q = x_abs / r_abs;
        derived.d_valid = true;
        derived.d = r_abs / x_abs;
    }

    derived.interpretation = measurement_interpret_single(&derived, config);
    return derived;
}

measurement_interpretation_t measurement_interpret_single(const measurement_derived_result_t *derived,
                                                          const measurement_dsp_config_t *config)
{
    if ((derived == NULL) || (config == NULL) || !derived->valid)
    {
        return MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN;
    }
    const float x_abs = abs_float(derived->reactance_ohms);
    const float r_abs = abs_float(derived->resistance_ohms);
    const float ratio = x_abs / max_float(r_abs, 1.0e-6f);
    if (ratio <= config->interpretation_ratio_resistive_max)
    {
        return MEASUREMENT_INTERPRET_RESISTIVE;
    }
    if (ratio >= config->interpretation_ratio_reactive_min)
    {
        return (derived->reactance_ohms < 0.0f) ? MEASUREMENT_INTERPRET_CAPACITIVE :
                                                  MEASUREMENT_INTERPRET_INDUCTIVE;
    }
    return MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN;
}

bsp_status_t measurement_process_block(const hw_metrology_block_t *block,
                                       const measurement_adc_calibration_t *adc_cal,
                                       const measurement_dsp_config_t *config,
                                       measurement_result_t *result)
{
    if ((block == NULL) || (adc_cal == NULL) || (config == NULL) || (result == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    *result = (measurement_result_t){0};
    result->status = MEASUREMENT_STATUS_INVALID_ARG;

    if (measurement_extract_phasors(block, adc_cal, config, &result->phasors) != BSP_STATUS_OK)
    {
        result->status = block->clipped ? MEASUREMENT_STATUS_CLIPPED : MEASUREMENT_STATUS_INVALID_ARG;
        return BSP_STATUS_ERROR;
    }

    const measurement_complex_t ret_1x_signal =
        measurement_complex_sub(result->phasors.ret_1x, result->phasors.vmid);
    const measurement_complex_t ret_hg_signal =
        measurement_complex_sub(result->phasors.ret_hg_reconstructed, result->phasors.vmid);
    const bool ret_1x_clipped = block->streams[HW_METROLOGY_STREAM_RET_1X].hard_clipped ||
                                block->streams[HW_METROLOGY_STREAM_VEXC_1].hard_clipped;
    const bool ret_hg_clipped = block->streams[HW_METROLOGY_STREAM_RET_HG].hard_clipped ||
                                block->streams[HW_METROLOGY_STREAM_VEXC_2].hard_clipped;
    result->ret_1x_quality = measurement_channel_quality(&ret_1x_signal,
                                                         ret_1x_clipped,
                                                         true,
                                                         config->return_min_v_peak);
    result->ret_hg_quality =
        measurement_channel_quality(&ret_hg_signal,
                                    ret_hg_clipped,
                                    !measurement_complex_near_zero(config->ret_hg_transfer,
                                                                   MEAS_DEFAULT_NEAR_ZERO),
                                    config->return_min_v_peak);

    result->selected_channel = measurement_select_return_channel(&result->ret_1x_quality,
                                                                 &result->ret_hg_quality);
    if ((result->selected_channel == MEASUREMENT_RETURN_HG) && result->ret_hg_quality.usable)
    {
        result->impedance = measurement_compute_impedance(
            measurement_complex_sub(result->phasors.vexc_2, result->phasors.vmid),
            ret_hg_signal,
            config->zref_ohms,
            config,
            MEASUREMENT_RETURN_HG);
    }
    else if (result->ret_1x_quality.usable)
    {
        result->selected_channel = MEASUREMENT_RETURN_1X;
        result->impedance = measurement_compute_impedance(
            measurement_complex_sub(result->phasors.vexc_1, result->phasors.vmid),
            ret_1x_signal,
            config->zref_ohms,
            config,
            MEASUREMENT_RETURN_1X);
    }
    else
    {
        result->status = block->clipped ? MEASUREMENT_STATUS_CLIPPED : MEASUREMENT_STATUS_CHANNEL_UNUSABLE;
        return BSP_STATUS_ERROR;
    }

    result->status = result->impedance.status;
    result->derived = measurement_derive_quantities(result->impedance.z_ohms,
                                                    block->excitation_frequency_hz,
                                                    config,
                                                    result->impedance.status);
    return (result->status == MEASUREMENT_STATUS_OK) ? BSP_STATUS_OK : BSP_STATUS_ERROR;
}

const char *measurement_status_string(measurement_status_t status)
{
    switch (status)
    {
    case MEASUREMENT_STATUS_OK:
        return "OK";
    case MEASUREMENT_STATUS_INVALID_ARG:
        return "INVALID_ARG";
    case MEASUREMENT_STATUS_UNSUPPORTED_PROFILE:
        return "UNSUPPORTED_PROFILE";
    case MEASUREMENT_STATUS_NONFINITE:
        return "NONFINITE";
    case MEASUREMENT_STATUS_CLIPPED:
        return "CLIPPED";
    case MEASUREMENT_STATUS_CHANNEL_UNUSABLE:
        return "CHANNEL_UNUSABLE";
    case MEASUREMENT_STATUS_SOURCE_TOO_SMALL:
        return "SOURCE_TOO_SMALL";
    case MEASUREMENT_STATUS_RETURN_TOO_SMALL:
        return "RETURN_TOO_SMALL";
    case MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL:
        return "DENOMINATOR_TOO_SMALL";
    case MEASUREMENT_STATUS_INVALID_ZREF:
        return "INVALID_ZREF";
    default:
        return "UNKNOWN";
    }
}

const char *measurement_interpretation_string(measurement_interpretation_t interpretation)
{
    switch (interpretation)
    {
    case MEASUREMENT_INTERPRET_RESISTIVE:
        return "RESISTIVE";
    case MEASUREMENT_INTERPRET_CAPACITIVE:
        return "CAPACITIVE";
    case MEASUREMENT_INTERPRET_INDUCTIVE:
        return "INDUCTIVE";
    case MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN:
    default:
        return "MIXED_OR_UNKNOWN";
    }
}
