#ifndef WTK_MEASUREMENT_DSP_H
#define WTK_MEASUREMENT_DSP_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "hardware/hw_excitation.h"
#include "hardware/hw_metrology_raw.h"
#include "hardware/hw_range.h"

enum
{
    MEASUREMENT_ADC_RAW_MAX = 4095u,
    MEASUREMENT_PHASE_REF_POINTS_64 = 64u,
    MEASUREMENT_PHASE_REF_POINTS_16 = 16u,
};

typedef struct
{
    float re;
    float im;
} measurement_complex_t;

typedef enum
{
    MEASUREMENT_STATUS_OK = 0,
    MEASUREMENT_STATUS_INVALID_ARG,
    MEASUREMENT_STATUS_UNSUPPORTED_PROFILE,
    MEASUREMENT_STATUS_NONFINITE,
    MEASUREMENT_STATUS_CLIPPED,
    MEASUREMENT_STATUS_CHANNEL_UNUSABLE,
    MEASUREMENT_STATUS_SOURCE_TOO_SMALL,
    MEASUREMENT_STATUS_RETURN_TOO_SMALL,
    MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL,
    MEASUREMENT_STATUS_INVALID_ZREF,
} measurement_status_t;

typedef enum
{
    MEASUREMENT_RETURN_1X = 0,
    MEASUREMENT_RETURN_HG,
} measurement_return_channel_t;

typedef enum
{
    MEASUREMENT_INTERPRET_RESISTIVE = 0,
    MEASUREMENT_INTERPRET_CAPACITIVE,
    MEASUREMENT_INTERPRET_INDUCTIVE,
    MEASUREMENT_INTERPRET_MIXED_OR_UNKNOWN,
} measurement_interpretation_t;

typedef struct
{
    float code_to_volts;
    float offset_volts;
} measurement_adc_scale_t;

typedef struct
{
    measurement_adc_scale_t vexc_1;
    measurement_adc_scale_t ret_1x;
    measurement_adc_scale_t vexc_2;
    measurement_adc_scale_t ret_hg;
    measurement_adc_scale_t vmid_adc1;
    measurement_adc_scale_t vmid_adc2;
} measurement_adc_calibration_t;

typedef struct
{
    measurement_complex_t ret_hg_transfer;
    measurement_complex_t zref_ohms;
    float source_min_v_peak;
    float return_min_v_peak;
    float denominator_min_v_peak;
    float reactance_min_ohms;
    float interpretation_ratio_resistive_max;
    float interpretation_ratio_reactive_min;
} measurement_dsp_config_t;

typedef struct
{
    measurement_complex_t vexc_1;
    measurement_complex_t ret_1x;
    measurement_complex_t vexc_2;
    measurement_complex_t ret_hg;
    measurement_complex_t vmid_adc1;
    measurement_complex_t vmid_adc2;
    measurement_complex_t vmid;
    measurement_complex_t ret_hg_reconstructed;
    float vexc_1_peak_v;
    float ret_1x_peak_v;
    float vexc_2_peak_v;
    float ret_hg_peak_v;
    bool valid;
    bool clipped;
} measurement_phasor_set_t;

typedef struct
{
    bool usable;
    bool clipped;
    bool calibration_valid;
    bool signal_too_small;
    float signal_peak_v;
} measurement_channel_quality_t;

typedef struct
{
    measurement_status_t status;
    measurement_return_channel_t channel;
    measurement_complex_t vs_v;
    measurement_complex_t vx_v;
    measurement_complex_t z_ohms;
    bool open_like;
    bool short_like;
} measurement_impedance_result_t;

typedef struct
{
    bool valid;
    float resistance_ohms;
    float reactance_ohms;
    float magnitude_ohms;
    float phase_rad;
    bool capacitance_valid;
    float capacitance_f;
    bool inductance_valid;
    float inductance_h;
    bool q_valid;
    float q;
    bool d_valid;
    float d;
    measurement_interpretation_t interpretation;
} measurement_derived_result_t;

typedef struct
{
    measurement_status_t status;
    measurement_phasor_set_t phasors;
    measurement_channel_quality_t ret_1x_quality;
    measurement_channel_quality_t ret_hg_quality;
    measurement_return_channel_t selected_channel;
    measurement_impedance_result_t impedance;
    measurement_derived_result_t derived;
} measurement_result_t;

measurement_complex_t measurement_complex(float re, float im);
measurement_complex_t measurement_complex_add(measurement_complex_t a, measurement_complex_t b);
measurement_complex_t measurement_complex_sub(measurement_complex_t a, measurement_complex_t b);
measurement_complex_t measurement_complex_mul(measurement_complex_t a, measurement_complex_t b);
measurement_status_t measurement_complex_div(measurement_complex_t a,
                                             measurement_complex_t b,
                                             measurement_complex_t *out);
float measurement_complex_mag(measurement_complex_t value);
float measurement_complex_phase_rad(measurement_complex_t value);
bool measurement_complex_is_finite(measurement_complex_t value);
bool measurement_complex_near_zero(measurement_complex_t value, float threshold);

measurement_adc_calibration_t measurement_adc_calibration_ideal(void);
measurement_dsp_config_t measurement_dsp_config_ideal(hw_range_id_t range_id);

bsp_status_t measurement_extract_phasors(const hw_metrology_block_t *block,
                                         const measurement_adc_calibration_t *adc_cal,
                                         const measurement_dsp_config_t *config,
                                         measurement_phasor_set_t *phasors);
measurement_channel_quality_t measurement_channel_quality(const measurement_complex_t *signal,
                                                          bool clipped,
                                                          bool calibration_valid,
                                                          float min_signal_v_peak);
measurement_return_channel_t measurement_select_return_channel(
    const measurement_channel_quality_t *ret_1x,
    const measurement_channel_quality_t *ret_hg);
measurement_impedance_result_t measurement_compute_impedance(measurement_complex_t vs_v,
                                                             measurement_complex_t vx_v,
                                                             measurement_complex_t zref_ohms,
                                                             const measurement_dsp_config_t *config,
                                                             measurement_return_channel_t channel);
measurement_derived_result_t measurement_derive_quantities(measurement_complex_t z_ohms,
                                                           uint32_t frequency_hz,
                                                           const measurement_dsp_config_t *config,
                                                           measurement_status_t impedance_status);
measurement_interpretation_t measurement_interpret_single(const measurement_derived_result_t *derived,
                                                          const measurement_dsp_config_t *config);
bsp_status_t measurement_process_block(const hw_metrology_block_t *block,
                                       const measurement_adc_calibration_t *adc_cal,
                                       const measurement_dsp_config_t *config,
                                       measurement_result_t *result);
const char *measurement_status_string(measurement_status_t status);
const char *measurement_interpretation_string(measurement_interpretation_t interpretation);

#endif
