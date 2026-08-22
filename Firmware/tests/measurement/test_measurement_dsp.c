#include "measurement/measurement_dsp.h"

#include <stdio.h>
#include <string.h>

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
    block->excitation_frequency_hz = (frequency == HW_EXCITATION_FREQ_100HZ) ? 100u :
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
    failures += test_impedance_vector(HW_EXCITATION_FREQ_100HZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(0.0f, -1000.0f),
                                      "C 100Hz",
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
    failures += test_impedance_vector(HW_EXCITATION_FREQ_100HZ,
                                      HW_RANGE_ID_1K,
                                      measurement_complex(0.0f, 1000.0f),
                                      "L 100Hz",
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

int main(void)
{
    int failures = 0;
    failures += test_complex_helpers();
    failures += test_phasor_convention();
    failures += test_impedance_vectors();
    failures += test_quality_and_errors();
    failures += test_derived_values();
    return (failures == 0) ? 0 : 1;
}
