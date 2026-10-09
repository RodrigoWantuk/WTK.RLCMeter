#include "measurement/measurement_cal_curve.h"

#include <math.h>
#include <stdio.h>

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int expect_near(float actual, float expected, float tolerance, const char *message)
{
    if (fabsf(actual - expected) > tolerance)
    {
        (void)fprintf(stderr, "FAIL: %s (%.7g versus %.7g)\n", message,
                      (double)actual, (double)expected);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;
    measurement_cal_curve_t curve;
    measurement_cal_curve_identity(&curve);
    measurement_complex_t out = {0.0f, 0.0f};
    const float ratios[] = {0.1f, 0.2f, 0.31622776f, 1.0f, 3.1622777f, 10.0f};
    for (size_t i = 0u; i < sizeof(ratios) / sizeof(ratios[0]); i++)
    {
        const measurement_complex_t raw = {ratios[i] * 1000.0f, 0.0f};
        failures += expect_true(measurement_cal_curve_apply(&curve, raw, 1000.0f, &out) ==
                                MEASUREMENT_CAL_CURVE_OK, "identity applies throughout domain");
        failures += expect_near(out.re, raw.re, 0.002f, "identity preserves real impedance");
        failures += expect_near(out.im, 0.0f, 0.002f, "identity preserves imaginary impedance");
    }

    curve.knot[1] = (measurement_cal_curve_matrix_t){2.0f, 0.0f, 0.0f, 2.0f};
    curve.knot[2] = (measurement_cal_curve_matrix_t){3.0f, 0.0f, 0.0f, 3.0f};
    failures += expect_true(measurement_cal_curve_apply(&curve, (measurement_complex_t){316.22776f, 0.0f},
                                                         1000.0f, &out) == MEASUREMENT_CAL_CURVE_OK,
                            "lower log midpoint applies");
    failures += expect_near(out.re, 474.34164f, 0.005f, "lower log midpoint interpolates");
    failures += expect_true(measurement_cal_curve_apply(&curve, (measurement_complex_t){3162.2777f, 0.0f},
                                                         1000.0f, &out) == MEASUREMENT_CAL_CURVE_OK,
                            "upper log midpoint applies");
    failures += expect_near(out.re, 7905.6943f, 0.04f, "upper log midpoint interpolates");

    curve.knot[1] = (measurement_cal_curve_matrix_t){0.0f, -1.0f, 1.0f, 0.0f};
    failures += expect_true(measurement_cal_curve_apply(&curve, (measurement_complex_t){600.0f, 800.0f},
                                                         1000.0f, &out) == MEASUREMENT_CAL_CURVE_OK,
                            "complex cross-coupling applies");
    failures += expect_near(out.re, -800.0f, 0.005f, "complex real cross-coupling");
    failures += expect_near(out.im, 600.0f, 0.005f, "complex imaginary cross-coupling");

    out = (measurement_complex_t){123.0f, 456.0f};
    failures += expect_true(measurement_cal_curve_apply(&curve, (measurement_complex_t){1.0f, 0.0f},
                                                         1000.0f, &out) ==
                                MEASUREMENT_CAL_CURVE_OUT_OF_DOMAIN, "outside domain rejected");
    failures += expect_near(out.re, 123.0f, 0.0f, "rejected curve leaves output untouched");
    failures += expect_true(measurement_cal_curve_apply(&curve, (measurement_complex_t){1000.0f, 0.0f},
                                                         0.0f, &out) ==
                                MEASUREMENT_CAL_CURVE_INVALID_ARG, "invalid RREF rejected");
    curve.knot[0].m00 = NAN;
    failures += expect_true(measurement_cal_curve_apply(&curve, (measurement_complex_t){1000.0f, 0.0f},
                                                         1000.0f, &out) ==
                                MEASUREMENT_CAL_CURVE_NONFINITE, "invalid coefficient rejected");
    return failures == 0 ? 0 : 1;
}
