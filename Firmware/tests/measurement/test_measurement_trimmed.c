#include "measurement/measurement_trimmed.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

static int check(bool condition, const char *label)
{
    if (!condition) (void)fprintf(stderr, "FAIL: %s\n", label);
    return condition ? 0 : 1;
}

int main(void)
{
    int failures = 0;
    measurement_trimmed_t filter;
    measurement_trimmed_init(&filter);
    failures += check(!measurement_trimmed_finish(&filter), "no empty estimate");
    failures += check(!measurement_trimmed_add(&filter, measurement_complex(NAN, 0.0f)) &&
                      !measurement_trimmed_add(&filter, measurement_complex(0.0f, INFINITY)) &&
                      filter.summary.samples == 0u, "nonfinite points do not consume samples");
    const float r[] = {1000.0f, 10.0f, -1000.0f, 12.0f, 11.0f, 1001.0f, -1001.0f};
    const float x[] = {-12.0f, -1000.0f, 1000.0f, -10.0f, 1001.0f, -11.0f, -1001.0f};
    for (uint8_t i = 0u; i < MEASUREMENT_TRIMMED_SAMPLES; i++)
        failures += check(measurement_trimmed_add(&filter, measurement_complex(r[i], x[i])), "finite point admitted");
    failures += check(!measurement_trimmed_add(&filter, measurement_complex(0.0f, 0.0f)), "sample set is bounded");
    failures += check(measurement_trimmed_finish(&filter) && fabsf(filter.estimate.re - 11.0f) < 0.00001f &&
                      fabsf(filter.estimate.im + 11.0f) < 0.00001f, "both tails trimmed on signed coordinates");
    failures += check(filter.summary.r_retained_mask == 26u && filter.summary.x_retained_mask == 41u,
                      "distinct masks retain original identity");
    const measurement_dsp_config_t config = measurement_dsp_config_ideal(HW_RANGE_ID_1K);
    const measurement_derived_result_t d = measurement_derive_quantities(filter.estimate, 1000u, &config,
                                                                        MEASUREMENT_STATUS_OK);
    failures += check(d.capacitance_valid && !d.inductance_valid && d.phase_rad < 0.0f,
                      "derived quantities share the filtered impedance");
    measurement_trimmed_init(&filter);
    for (uint8_t i = 0u; i < MEASUREMENT_TRIMMED_SAMPLES; i++)
        (void)measurement_trimmed_add(&filter, measurement_complex(-100.0f, (float)i - 3.0f));
    failures += check(measurement_trimmed_finish(&filter) && fabsf(filter.estimate.im) < 0.00001f &&
                      fabsf(measurement_complex_phase_rad(filter.estimate)) > 3.0f,
                      "complex estimate avoids averaging wrapped plus/minus pi angles");
    failures += check(filter.summary.r_retained_mask == 28u, "equal coordinates have stable deterministic ties");
    measurement_trimmed_init(&filter);
    for (uint8_t i = 0u; i < MEASUREMENT_TRIMMED_SAMPLES; i++)
        (void)measurement_trimmed_add(&filter, measurement_complex(FLT_MAX / 2.0f, -FLT_MAX / 2.0f));
    failures += check(measurement_trimmed_finish(&filter), "scaled mean avoids finite-input sum overflow");
    measurement_trimmed_init(&filter);
    failures += check(filter.summary.samples == 0u && !filter.summary.applied && filter.points[0].re == 0.0f,
                      "reset cannot reuse preceding session points");
    return failures == 0 ? 0 : 1;
}
