#include "measurement/measurement_dsp.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "wtk_build_config.h"

#define TEST_PI_F (3.14159265358979323846f)
#define TEST_TWO_PI_F (6.28318530717958647692f)
#define TEST_ADC_SCALE (3.3f / 4095.0f)

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
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

static int test_complex_magnitude_scales(void)
{
    int failures = 0;
    const struct
    {
        measurement_complex_t value;
        float expected;
        float tolerance;
    } cases[] = {
        {{3.0f, 4.0f}, 5.0f, 0.00001f},
        {{3000.0f, 4000.0f}, 5000.0f, 0.001f},
        {{600000.0f, 800000.0f}, 1000000.0f, 0.1f},
        {{0.003f, 0.004f}, 0.005f, 0.00000001f},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        failures += expect_near(measurement_complex_mag(cases[i].value),
                                cases[i].expected, cases[i].tolerance,
                                "complex magnitude remains accurate across decades");
    }
    return failures;
}

static measurement_complex_t cadd(measurement_complex_t a, measurement_complex_t b)
{
    return measurement_complex(a.re + b.re, a.im + b.im);
}

static measurement_complex_t csub(measurement_complex_t a, measurement_complex_t b)
{
    return measurement_complex(a.re - b.re, a.im - b.im);
}

static measurement_complex_t cmul(measurement_complex_t a, measurement_complex_t b)
{
    return measurement_complex((a.re * b.re) - (a.im * b.im),
                               (a.re * b.im) + (a.im * b.re));
}

static measurement_complex_t cdiv(measurement_complex_t a, measurement_complex_t b)
{
    measurement_complex_t out = {0.0f, 0.0f};
    (void)measurement_complex_div(a, b, &out);
    return out;
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

static void make_empty_block(hw_metrology_block_t *block,
                             uint32_t *raw,
                             hw_excitation_freq_t frequency,
                             hw_range_id_t range_id)
{
    memset(block, 0, sizeof(*block));
    memset(raw, 0, sizeof(uint32_t) * HW_METROLOGY_RAW_WORD_COUNT);
    hw_metrology_adc_profile_t profile;
    (void)hw_metrology_adc_profile(frequency, &profile);
    block->valid = true;
    block->mode = HW_METROLOGY_MODE_DUT_MEASURE;
    block->dut_measure = true;
    block->excitation_frequency_hz = (frequency == HW_EXCITATION_FREQ_10HZ) ? 10u :
                                     (frequency == HW_EXCITATION_FREQ_1KHZ)  ? 1000u :
                                                                                10000u;
    block->requested_amplitude_mvrms = 100u;
    block->range_id = range_id;
    block->adc_clock_hz = 12000000u;
    block->sample_time_cycles_x2 = HW_METROLOGY_ADC_SAMPLE_TIME_CYCLES_X2;
    block->sample_rate_hz = profile.sample_rate_hz;
    block->samples_per_cycle = profile.samples_per_cycle;
    block->cycles_per_block = profile.cycles_per_block;
    block->sample_count = HW_METROLOGY_SAMPLES_PER_BLOCK;
    block->words_per_sample = HW_METROLOGY_WORDS_PER_SAMPLE;
    block->raw_words = raw;
    block->dma_complete = true;
}

static void fill_impedance_block(hw_metrology_block_t *block,
                                 uint32_t *raw,
                                 measurement_complex_t dut_z,
                                 measurement_complex_t source_vs,
                                 measurement_complex_t hg_transfer)
{
    const measurement_dsp_config_t config = measurement_dsp_config_ideal(block->range_id);
    const measurement_complex_t vx = cmul(dut_z, cdiv(source_vs, cadd(config.zref_ohms, dut_z)));
    const measurement_complex_t vexc = source_vs;
    const measurement_complex_t ret_1x = vx;
    const measurement_complex_t ret_hg = cmul(vx, hg_transfer);
    float cos_ref = 1.0f;
    float sin_ref = 0.0f;

    for (uint32_t n = 0u; n < HW_METROLOGY_SAMPLES_PER_BLOCK; n++)
    {
        const float vexc_v = waveform_value(1.65f, vexc, cos_ref, sin_ref);
        const float ret_1x_v = waveform_value(1.65f, ret_1x, cos_ref, sin_ref);
        const float ret_hg_v = waveform_value(1.65f, ret_hg, cos_ref, sin_ref);
        const float vmid_v = 1.65f;
        raw[(3u * n) + 0u] = hw_metrology_pack_word(volts_to_raw(vexc_v), volts_to_raw(ret_1x_v));
        raw[(3u * n) + 1u] = hw_metrology_pack_word(volts_to_raw(vexc_v), volts_to_raw(ret_hg_v));
        raw[(3u * n) + 2u] = hw_metrology_pack_word(volts_to_raw(vmid_v), volts_to_raw(vmid_v));
        step_reference(block->samples_per_cycle, &cos_ref, &sin_ref);
    }
    hw_metrology_analyze_block(raw, HW_METROLOGY_SAMPLES_PER_BLOCK, block);
}

static int test_complex_helpers(void)
{
    int failures = 0;
    measurement_complex_t div = {0.0f, 0.0f};
    failures += expect_true(measurement_complex_div(measurement_complex(3.0f, 2.0f),
                                                    measurement_complex(1.0f, -1.0f),
                                                    &div) == MEASUREMENT_STATUS_OK,
                            "complex div ok");
    failures += expect_complex_near(div, measurement_complex(0.5f, 2.5f), 0.0001f, "complex div value");
    failures += expect_near(measurement_complex_mag(measurement_complex(3.0f, 4.0f)), 5.0f, 0.0001f, "mag");
    failures += expect_near(measurement_complex_phase_rad(measurement_complex(0.0f, 1.0f)),
                            TEST_PI_F / 2.0f,
                            0.004f,
                            "phase");
    failures += expect_true(measurement_complex_div(measurement_complex(1.0f, 0.0f),
                                                    measurement_complex(0.0f, 0.0f),
                                                    &div) == MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL,
                            "zero divide guarded");
    return failures;
}

static int test_phasor_convention(void)
{
    int failures = 0;
    hw_metrology_block_t block;
    uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    make_empty_block(&block, raw, HW_EXCITATION_FREQ_1KHZ, HW_RANGE_ID_1K);
    const measurement_complex_t source = measurement_complex(0.08660254f, 0.05000000f);
    fill_impedance_block(&block,
                         raw,
                         measurement_complex(1000.0f, 0.0f),
                         source,
                         measurement_complex(15.468085f, 0.0f));

    measurement_phasor_set_t phasors = {0};
    measurement_adc_calibration_t adc = measurement_adc_calibration_ideal();
    measurement_dsp_config_t config = measurement_dsp_config_ideal(HW_RANGE_ID_1K);
    failures += expect_true(measurement_extract_phasors(&block, &adc, &config, &phasors) == BSP_STATUS_OK,
                            "phasor extraction");
    failures += expect_complex_near(csub(phasors.vexc_1, phasors.vmid), source, 0.0015f, "positive phase");
    return failures;
}

static int test_impedance_vector(hw_excitation_freq_t frequency,
                                 hw_range_id_t range,
                                 measurement_complex_t dut_z,
                                 const char *message,
                                 measurement_interpretation_t expected)
{
    int failures = 0;
    hw_metrology_block_t block;
    uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    make_empty_block(&block, raw, frequency, range);
    measurement_dsp_config_t config = measurement_dsp_config_ideal(range);
    fill_impedance_block(&block,
                         raw,
                         dut_z,
                         measurement_complex(0.05f, 0.0f),
                         config.ret_hg_transfer);

    measurement_result_t result;
    measurement_adc_calibration_t adc = measurement_adc_calibration_ideal();
    failures += expect_true(measurement_process_block(&block, &adc, &config, &result) == BSP_STATUS_OK, message);
    const float z_tolerance = (measurement_complex_mag(dut_z) > 5000.0f) ? 150.0f : 3.5f;
    failures += expect_complex_near(result.impedance.z_ohms, dut_z, z_tolerance, message);
    failures += expect_true(result.derived.interpretation == expected, message);
    return failures;
}

static int test_impedance_vectors(void)
{
    int failures = 0;
    failures += test_impedance_vector(HW_EXCITATION_FREQ_1KHZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(100.0f, 0.0f),
                                      "R 0.1x",
                                      MEASUREMENT_INTERPRET_RESISTIVE);
    failures += test_impedance_vector(HW_EXCITATION_FREQ_1KHZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(1000.0f, 0.0f),
                                      "R 1x",
                                      MEASUREMENT_INTERPRET_RESISTIVE);
    failures += test_impedance_vector(HW_EXCITATION_FREQ_1KHZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(10000.0f, 0.0f),
                                      "R 10x",
                                      MEASUREMENT_INTERPRET_RESISTIVE);

    const float c_1k = 1.0f / (TEST_TWO_PI_F * 1000.0f * 1000.0f);
    (void)c_1k;
    failures += test_impedance_vector(HW_EXCITATION_FREQ_10HZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(0.0f, -1000.0f),
                                      "C 10Hz",
                                      MEASUREMENT_INTERPRET_CAPACITIVE);
    failures += test_impedance_vector(HW_EXCITATION_FREQ_1KHZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(0.0f, -1000.0f),
                                      "C 1k",
                                      MEASUREMENT_INTERPRET_CAPACITIVE);
    failures += test_impedance_vector(HW_EXCITATION_FREQ_10KHZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(0.0f, -1000.0f),
                                      "C 10k",
                                      MEASUREMENT_INTERPRET_CAPACITIVE);
    failures += test_impedance_vector(HW_EXCITATION_FREQ_10HZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(0.0f, 1000.0f),
                                      "L 10Hz",
                                      MEASUREMENT_INTERPRET_INDUCTIVE);
    failures += test_impedance_vector(HW_EXCITATION_FREQ_1KHZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(0.0f, 1000.0f),
                                      "L 1k",
                                      MEASUREMENT_INTERPRET_INDUCTIVE);
    failures += test_impedance_vector(HW_EXCITATION_FREQ_10KHZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(0.0f, 1000.0f),
                                      "L 10k",
                                      MEASUREMENT_INTERPRET_INDUCTIVE);
    failures += test_impedance_vector(HW_EXCITATION_FREQ_1KHZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(1000.0f, -1000.0f),
                                      "series RC",
                                      MEASUREMENT_INTERPRET_CAPACITIVE);
    failures += test_impedance_vector(HW_EXCITATION_FREQ_1KHZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(1000.0f, 1000.0f),
                                      "series RL",
                                      MEASUREMENT_INTERPRET_INDUCTIVE);
    return failures;
}

static int test_quality_and_errors(void)
{
    int failures = 0;
    measurement_dsp_config_t config = measurement_dsp_config_ideal(HW_RANGE_ID_1K);
    measurement_impedance_result_t result =
        measurement_compute_impedance(measurement_complex(0.0f, 0.0f),
                                      measurement_complex(0.01f, 0.0f),
                                      config.zref_ohms,
                                      &config,
                                      MEASUREMENT_RETURN_1X);
    failures += expect_true(result.status == MEASUREMENT_STATUS_SOURCE_TOO_SMALL, "source too small");

    result = measurement_compute_impedance(measurement_complex(0.05f, 0.0f),
                                           measurement_complex(0.04999f, 0.0f),
                                           config.zref_ohms,
                                           &config,
                                           MEASUREMENT_RETURN_1X);
    failures += expect_true(result.status == MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL, "open-like");
    failures += expect_true(result.open_like, "open flag");

    result = measurement_compute_impedance(measurement_complex(0.05f, 0.0f),
                                           measurement_complex(0.0f, 0.0f),
                                           config.zref_ohms,
                                           &config,
                                           MEASUREMENT_RETURN_1X);
    failures += expect_true(result.status == MEASUREMENT_STATUS_OK, "short computes");
    failures += expect_true(result.short_like, "short flag");

    hw_metrology_block_t block;
    uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    make_empty_block(&block, raw, HW_EXCITATION_FREQ_1KHZ, HW_RANGE_ID_1K);
    fill_impedance_block(&block,
                         raw,
                         measurement_complex(1000.0f, 0.0f),
                         measurement_complex(0.05f, 0.0f),
                         config.ret_hg_transfer);
    raw[0] = hw_metrology_pack_word(4095u, 2048u);
    raw[1] = hw_metrology_pack_word(4095u, 4095u);
    hw_metrology_analyze_block(raw, HW_METROLOGY_SAMPLES_PER_BLOCK, &block);
    measurement_result_t processed;
    measurement_adc_calibration_t adc = measurement_adc_calibration_ideal();
    failures += expect_true(measurement_process_block(&block, &adc, &config, &processed) == BSP_STATUS_ERROR,
                            "clipping rejected");
    failures += expect_true(processed.status == MEASUREMENT_STATUS_CLIPPED, "clip status");
#if WTK_PRODUCT_FACTORY_PROVISIONED
    make_empty_block(&block, raw, HW_EXCITATION_FREQ_1KHZ, HW_RANGE_ID_1K);
    fill_impedance_block(&block, raw, measurement_complex(1000.0f,0.0f),
        measurement_complex(0.05f,0.0f),config.ret_hg_transfer);
    block.clipped = true;
    block.streams[HW_METROLOGY_STREAM_RET_HG].hard_clipped = true;
    failures += expect_true(measurement_process_block(&block,&adc,&config,&processed)==BSP_STATUS_OK &&
        processed.selected_channel==MEASUREMENT_RETURN_1X && !processed.ret_1x_quality.clipped &&
        processed.ret_hg_quality.clipped,"actual DSP retains valid 1X when HG clips");
    failures += expect_near(processed.impedance.z_ohms.re,1000.0f,25.0f,"ADC/DSP 1X resistor fixture");
    block.streams[HW_METROLOGY_STREAM_RET_1X].hard_clipped = true;
    failures += expect_true(measurement_process_block(&block,&adc,&config,&processed)==BSP_STATUS_ERROR,
        "actual DSP rejects both clipped return paths");
#endif
    return failures;
}

static int test_derived_values(void)
{
    int failures = 0;
    measurement_dsp_config_t config = measurement_dsp_config_ideal(HW_RANGE_ID_1K);
    measurement_derived_result_t cap = measurement_derive_quantities(measurement_complex(10.0f, -1000.0f),
                                                                     1000u,
                                                                     &config,
                                                                     MEASUREMENT_STATUS_OK);
    failures += expect_true(cap.valid && cap.capacitance_valid, "cap valid");
    failures += expect_near(cap.capacitance_f, 1.0f / (TEST_TWO_PI_F * 1000.0f * 1000.0f), 2.0e-9f, "cap value");
    failures += expect_true(cap.q_valid && cap.d_valid, "cap q d");

    measurement_derived_result_t ind = measurement_derive_quantities(measurement_complex(10.0f, 1000.0f),
                                                                     1000u,
                                                                     &config,
                                                                     MEASUREMENT_STATUS_OK);
    failures += expect_true(ind.valid && ind.inductance_valid, "ind valid");
    failures += expect_near(ind.inductance_h, 1000.0f / (TEST_TWO_PI_F * 1000.0f), 0.0002f, "ind value");

    measurement_derived_result_t res = measurement_derive_quantities(measurement_complex(1000.0f, 0.0001f),
                                                                     1000u,
                                                                     &config,
                                                                     MEASUREMENT_STATUS_OK);
    failures += expect_true(res.valid && !res.capacitance_valid && !res.inductance_valid, "near-zero X invalid LC");
    failures += expect_true(res.interpretation == MEASUREMENT_INTERPRET_RESISTIVE, "res interpretation");
    return failures;
}


/* Host-only feasibility, not measured accuracy. CSV rows use an independent
 * double-precision divider/loaded-ladder oracle, then exercise production guards. */
static int test_required_rlc_domain(void)
{
    int failures = 0;
    double max_z_error = 0.0, max_lc_error = 0.0;
    const double inductors[] = {500e-9, 1e-6, 10e-6, 100e-6, 1e-3, 10e-3, 100e-3, 1.0, 2.0};
    const double capacitors[] = {1e-12, 2.2e-12, 10e-12, 100e-12, 1e-9, 10e-9, 100e-9,
                                 1e-6, 10e-6, 100e-6, 1000e-6, 10e-3};
    const double frequencies[] = {10.0, 1000.0, 10000.0};
    const double ranges[] = {10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0};
    const double pi = 3.14159265358979323846;
    (void)puts("FEASIBILITY,evidence,type,value_si,frequency_hz,reactance_ohm,rref_ohm,amplitude_class_mvrms,source_peak_v,return_peak_v,hg_peak_v,current_peak_a,z_over_rref,ret_1x_peak_codes,ret_hg_peak_codes,math_observable,hg_headroom,classification");
    for (unsigned type = 0u; type < 2u; type++)
    {
        const double *values = type == 0u ? inductors : capacitors;
        const unsigned count = type == 0u ? 9u : 12u;
        for (unsigned n = 0u; n < count; n++)
        {
            for (unsigned f = 0u; f < 3u; f++)
            {
                const double omega = 2.0 * pi * frequencies[f];
                const double x = type == 0u ? omega * values[n] : -1.0 / (omega * values[n]);
                unsigned r = 0u;
                double best = fabs(log(fabs(x) / ranges[0]));
                for (unsigned candidate = 1u; candidate < 6u; candidate++)
                {
                    const double distance = fabs(log(fabs(x) / ranges[candidate]));
                    if (distance < best) { r = candidate; best = distance; }
                }
                const double rc = omega * 5100.0 * 1e-9;
                const double ladder = 1.0 / hypot(1.0 - 5.0 * rc * rc, 6.0 * rc - rc * rc * rc);
                const unsigned amplitude = r == 0u ? 100u : 500u;
                const double source = sqrt(2.0) * (double)amplitude / 1000.0 * ladder;
                const double denominator = ranges[r] * ranges[r] + x * x;
                const double return_re = source * x * x / denominator;
                const double return_im = source * ranges[r] * x / denominator;
                const double return_peak = hypot(return_re, return_im);
                const double hg = return_peak * (1.0 + 68000.0 / 4700.0);
                const measurement_dsp_config_t config = measurement_dsp_config_ideal((hw_range_id_t)r);
                const measurement_impedance_result_t actual = measurement_compute_impedance(
                    measurement_complex((float)source, 0.0f),
                    measurement_complex((float)return_re, (float)return_im),
                    config.zref_ohms, &config, MEASUREMENT_RETURN_1X);
                const bool observable = actual.status == MEASUREMENT_STATUS_OK &&
                                        return_peak >= config.return_min_v_peak;
                const double q = fabs(x) / ranges[r];
                const char *classification = "PLAUSIBLE_FOR_BENCH_TEST";
                if (!observable || (type == 0u && values[n] <= 1e-6) ||
                    (type == 1u && values[n] <= 10e-12))
                    classification = "OUTSIDE_DEMONSTRATED_CAPABILITY";
                else if (q < 0.1 || q > 10.0 || (type == 1u && values[n] <= 100e-12) ||
                         (r >= 4u && f == 2u))
                    classification = "LOW_CONFIDENCE";
                if (type == 1u && values[n] >= 100e-6)
                    classification = "ELECTRICAL_SAFETY_HOLD"; /* Unapproved large/polarized parts. */
                if (observable)
                {
                    const double z_error = fabs((double)actual.z_ohms.im / x - 1.0);
                    if (z_error > max_z_error) { max_z_error = z_error; }
                    failures += expect_true(isfinite(actual.z_ohms.im) &&
                                            fabs((double)actual.z_ohms.im / x - 1.0) < 0.002,
                                            "independent ideal divider agrees with production complex equation");
                    const measurement_derived_result_t derived = measurement_derive_quantities(
                        measurement_complex(1.0f, (float)x), (uint32_t)frequencies[f],
                        &config, MEASUREMENT_STATUS_OK);
                    if (fabs(x) > config.reactance_min_ohms)
                    {
                        const float recovered = type == 0u ? derived.inductance_h : derived.capacitance_f;
                        const double lc_error = fabs((double)recovered / values[n] - 1.0);
                        if (lc_error > max_lc_error) { max_lc_error = lc_error; }
                        failures += expect_true(fabs((double)recovered / values[n] - 1.0) < 1e-5,
                                                "all derived L/C use actual 10/1k/10k frequency");
                    }
                }
                (void)printf("FEASIBILITY,SYNTHETIC,%s,%.9g,%.0f,%.9g,%.0f,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%s,%s,%s\n",
                             type == 0u ? "L" : "C", values[n], frequencies[f], x, ranges[r], amplitude,
                             source, return_peak, hg, source / sqrt(denominator), q,
                             return_peak * 4095.0 / 3.3, hg * 4095.0 / 3.3,
                             observable ? "YES" : "NO",
                             hg < 1.65 - 3.3 * HW_METROLOGY_CLIP_LOW_MAX / 4095.0 ? "YES" : "NO", classification);
            }
        }
    }
    (void)printf("FEASIBILITY_ERROR max_relative_z=%.9g max_relative_lc=%.9g SYNTHETIC_ONLY\n", max_z_error, max_lc_error);
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_complex_magnitude_scales();
    failures += test_complex_helpers();
    failures += test_phasor_convention();
    failures += test_impedance_vectors();
    failures += test_quality_and_errors();
    failures += test_derived_values();
    failures += test_required_rlc_domain();
#if WTK_PRODUCT_FACTORY_PROVISIONED
    const float components[] = {-1000.0f,-10.0f,-1.0f,0.0f,1.0f,10.0f,1000.0f};
    for(size_t i=0u;i<sizeof(components)/sizeof(components[0]);i++)
        for(size_t j=0u;j<sizeof(components)/sizeof(components[0]);j++)
            failures += expect_near(measurement_complex_phase_rad(measurement_complex(components[i],components[j])),
                atan2f(components[j],components[i]),0.002f,"phase range reduction vs host atan2");
#endif
    return (failures == 0) ? 0 : 1;
}
