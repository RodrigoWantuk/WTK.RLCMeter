#include "measurement/measurement_lead_compensation.h"
#include <math.h>
#include <string.h>

static bool factory_finite(const measurement_cal_osl_coefficients_t *factory)
{
    return factory != NULL && measurement_complex_is_finite(factory->k) &&
        measurement_complex_is_finite(factory->t_short) && measurement_complex_is_finite(factory->t_open);
}
static measurement_status_t divide_conditioned(measurement_complex_t numerator,
    measurement_complex_t denominator, float scale, measurement_complex_t *out)
{
    if (out == NULL) return MEASUREMENT_STATUS_INVALID_ARG;
    if (!measurement_complex_is_finite(numerator) || !measurement_complex_is_finite(denominator) ||
        !isfinite(scale)) return MEASUREMENT_STATUS_NONFINITE;
    if (measurement_complex_mag(denominator) <= 1.0e-5f * (scale > 1.0f ? scale : 1.0f))
        return MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL;
    return measurement_complex_div(numerator, denominator, out);
}
measurement_status_t measurement_lead_series(const measurement_cal_osl_coefficients_t *factory,
    measurement_complex_t ratio, measurement_complex_t *series)
{
    if (!factory_finite(factory) || !measurement_complex_is_finite(ratio)) return MEASUREMENT_STATUS_INVALID_ARG;
    return divide_conditioned(measurement_complex_mul(factory->k, measurement_complex_sub(ratio, factory->t_short)),
        measurement_complex_sub(ratio, factory->t_open),
        measurement_complex_mag(ratio) + measurement_complex_mag(factory->t_open), series);
}
measurement_status_t measurement_lead_shunt(const measurement_cal_osl_coefficients_t *factory,
    measurement_complex_t ratio, measurement_complex_t series, measurement_complex_t *shunt)
{
    if (!factory_finite(factory) || !measurement_complex_is_finite(ratio) || !measurement_complex_is_finite(series))
        return MEASUREMENT_STATUS_INVALID_ARG;
    const measurement_complex_t a = measurement_complex_sub(factory->k, series);
    const measurement_complex_t b = measurement_complex_sub(measurement_complex_mul(series, factory->t_open),
                                                           measurement_complex_mul(factory->k, factory->t_short));
    const measurement_complex_t at = measurement_complex_mul(a, ratio);
    /* Admittance directly from transfer, including ratio == factory OPEN. */
    return divide_conditioned(measurement_complex_sub(ratio, factory->t_open), measurement_complex_add(at, b),
        measurement_complex_mag(at) + measurement_complex_mag(b), shunt);
}
measurement_status_t measurement_lead_apply_ratio(const measurement_cal_osl_coefficients_t *factory,
    const measurement_lead_frequency_t *lead, measurement_complex_t ratio, measurement_complex_t *z)
{
    if (!factory_finite(factory) || lead == NULL || !measurement_complex_is_finite(ratio) ||
        !measurement_complex_is_finite(lead->series_ohms) || !measurement_complex_is_finite(lead->shunt_siemens))
        return MEASUREMENT_STATUS_INVALID_ARG;
    const measurement_complex_t a = measurement_complex_sub(factory->k, lead->series_ohms);
    const measurement_complex_t b = measurement_complex_sub(measurement_complex_mul(lead->series_ohms, factory->t_open),
                                                           measurement_complex_mul(factory->k, factory->t_short));
    const measurement_complex_t c = measurement_complex_sub(measurement_complex(1.0f, 0.0f),
                                                           measurement_complex_mul(lead->shunt_siemens, a));
    const measurement_complex_t d = measurement_complex_sub(measurement_complex(-factory->t_open.re, -factory->t_open.im),
                                                           measurement_complex_mul(lead->shunt_siemens, b));
    const measurement_complex_t ct = measurement_complex_mul(c, ratio);
    return divide_conditioned(measurement_complex_add(measurement_complex_mul(a, ratio), b),
        measurement_complex_add(ct, d), measurement_complex_mag(ct) + measurement_complex_mag(d), z);
}
bool measurement_lead_frequency_valid(const measurement_lead_frequency_t *lead, uint32_t frequency_hz)
{
    if (lead == NULL || (frequency_hz != 10u && frequency_hz != 1000u && frequency_hz != 10000u) ||
        !measurement_complex_is_finite(lead->series_ohms) || !measurement_complex_is_finite(lead->shunt_siemens)) return false;
    /* Engineering endpoint plausibility limits, not a qualified accuracy range.
     * Up to 10 ohm series delta, 10 uS leakage delta and 10 nF shunt delta. */
    return measurement_complex_mag(lead->series_ohms) <= 10.0f &&
        fabsf(lead->shunt_siemens.re) <= 1.0e-5f &&
        fabsf(lead->shunt_siemens.im) <= 6.28318530718f * (float)frequency_hz * 1.0e-8f + 1.0e-8f;
}
bool measurement_lead_profile_valid(const measurement_lead_profile_t *profile)
{
    if (profile == NULL || profile->hardware_revision != MEASUREMENT_CAL_HARDWARE_REV1 ||
        profile->model_version != MEASUREMENT_CAL_MODEL_VERSION_CURRENT || profile->coverage != MEASUREMENT_LEAD_COVERAGE_ALL ||
        (profile->cleared && profile->enabled)) return false;
    bool all_zero = true, all_ff = true;
    for (unsigned i = 0u; i < 12u; ++i)
    {
        all_zero = all_zero && profile->uid[i] == 0u;
        all_ff = all_ff && profile->uid[i] == 255u;
    }
    if (all_zero || all_ff) return false;
    static const uint32_t hz[] = {10u, 1000u, 10000u};
    for (unsigned i = 0u; i < MEASUREMENT_LEAD_FREQUENCIES; ++i)
        if (!measurement_lead_frequency_valid(&profile->frequencies[i], hz[i])) return false;
    return true;
}
bool measurement_lead_profile_bound(const measurement_lead_profile_t *profile,
    const uint8_t uid[12], uint32_t factory_sequence, uint32_t factory_crc32)
{
    return uid != NULL && measurement_lead_profile_valid(profile) && memcmp(profile->uid, uid, 12u) == 0 &&
        profile->factory_sequence == factory_sequence && profile->factory_crc32 == factory_crc32;
}

static uint32_t lead_frequency_hz(hw_excitation_freq_t frequency)
{
    switch(frequency) { case HW_EXCITATION_FREQ_10HZ:return 10u;
    case HW_EXCITATION_FREQ_1KHZ:return 1000u; case HW_EXCITATION_FREQ_10KHZ:return 10000u;
    default:return 0u; }
}
bsp_status_t measurement_lead_endpoint_ratio(const hw_metrology_block_t *block,
    const measurement_cal_set_t *factory, const measurement_cal_key_t *key,
    measurement_return_channel_t channel, measurement_complex_t *ratio)
{
    if(block==NULL || factory==NULL || key==NULL || ratio==NULL ||
       (channel!=MEASUREMENT_RETURN_1X && channel!=MEASUREMENT_RETURN_HG)) return BSP_STATUS_INVALID_ARG;
    measurement_cal_resolved_t r;
    const measurement_cal_resolve_status_t status=measurement_cal_resolve_condition(factory,key,false,&r);
    if(status!=MEASUREMENT_CAL_RESOLVE_FOUND && status!=MEASUREMENT_CAL_RESOLVE_UNQUALIFIED) return BSP_STATUS_ERROR;
    const bool hg=channel==MEASUREMENT_RETURN_HG;
    if(hg && (r.correction.flags&MEASUREMENT_CAL_FLAG_HG_OBSERVED)==0u) return BSP_STATUS_NOT_SUPPORTED;
    if(block->streams[HW_METROLOGY_STREAM_VMID_ADC1].hard_clipped ||
       block->streams[HW_METROLOGY_STREAM_VMID_ADC2].hard_clipped ||
       block->streams[hg?HW_METROLOGY_STREAM_VEXC_2:HW_METROLOGY_STREAM_VEXC_1].hard_clipped ||
       block->streams[hg?HW_METROLOGY_STREAM_RET_HG:HW_METROLOGY_STREAM_RET_1X].hard_clipped) return BSP_STATUS_ERROR;
    measurement_phasor_set_t p;
    if(measurement_extract_phasors(block,&r.adc,&r.config,&p)!=BSP_STATUS_OK || !p.valid) return BSP_STATUS_ERROR;
    const measurement_complex_t vs=measurement_complex_sub(hg?p.vexc_2:p.vexc_1,p.vmid);
    const measurement_complex_t vx=measurement_complex_sub(hg?p.ret_hg_reconstructed:p.ret_1x,p.vmid);
    if(!measurement_complex_is_finite(vs) || measurement_complex_near_zero(vs,r.config.source_min_v_peak)) return BSP_STATUS_ERROR;
    return measurement_complex_div(vx,vs,ratio)==MEASUREMENT_STATUS_OK && measurement_complex_is_finite(*ratio)?
        BSP_STATUS_OK:BSP_STATUS_ERROR;
}
bsp_status_t measurement_lead_process_block(const hw_metrology_block_t *block,
    const measurement_cal_set_t *factory, const measurement_cal_key_t *key,
    const measurement_lead_profile_t *lead, measurement_calibrated_result_t *result)
{
    const bsp_status_t original=measurement_cal_process_block(block,factory,key,false,result);
    if(lead==NULL || !lead->enabled || lead->cleared) return original;
    if(result==NULL || key==NULL || !measurement_lead_profile_valid(lead) || factory==NULL ||
       lead->factory_sequence!=factory->sequence || (unsigned)key->frequency>=MEASUREMENT_LEAD_FREQUENCIES)
        return BSP_STATUS_INVALID_ARG;
    /* A channel/source failure cannot be repaired by fixture compensation.
     * A factory OPEN pole can be: compose in ratio space before division. */
    if(result->result.status!=MEASUREMENT_STATUS_OK &&
       result->result.status!=MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL) return original;
    measurement_cal_resolved_t r;
    measurement_cal_osl_coefficients_t coefficients;
    const measurement_cal_resolve_status_t resolved=measurement_cal_resolve_condition(factory,key,false,&r);
    if((resolved!=MEASUREMENT_CAL_RESOLVE_FOUND && resolved!=MEASUREMENT_CAL_RESOLVE_UNQUALIFIED) ||
        !measurement_cal_get_osl_coefficients(&r.correction,&coefficients)) return BSP_STATUS_ERROR;
    measurement_complex_t ratio={0},z={0};
    measurement_status_t status=measurement_complex_div(result->result.impedance.vx_v,result->result.impedance.vs_v,&ratio);
    if(status==MEASUREMENT_STATUS_OK)
        status=measurement_lead_apply_ratio(&coefficients,&lead->frequencies[(unsigned)key->frequency],ratio,&z);
    result->provenance.lead_compensated=true; result->provenance.lead_sequence=lead->sequence;
    result->result.status=status; result->result.impedance.status=status;
    result->result.derived=(measurement_derived_result_t){0};
    if(status!=MEASUREMENT_STATUS_OK) return BSP_STATUS_ERROR;
    result->result.impedance.z_ohms=z;
    result->result.impedance.open_like=false; result->result.impedance.short_like=false;
    result->result.derived=measurement_derive_quantities(z,lead_frequency_hz(key->frequency),&r.config,status);
    return result->result.derived.valid?BSP_STATUS_OK:BSP_STATUS_ERROR;
}
