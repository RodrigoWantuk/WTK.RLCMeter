#include "measurement/measurement_cal_curve.h"

#include <math.h>
#include <stddef.h>

static float log10_1_to_10(float value)
{
    uint8_t binary_exponent = 0u;
    while (value >= 2.0f)
    {
        value *= 0.5f;
        binary_exponent++;
    }
    const float z = (value - 1.0f) / (value + 1.0f);
    const float z2 = z * z;
    const float ln_m = 2.0f * z *
                       (1.0f + z2 * (1.0f / 3.0f + z2 * (1.0f / 5.0f +
                       z2 * (1.0f / 7.0f + z2 / 9.0f))));
    return ((float)binary_exponent * 0.30102999566f) +
           (ln_m * 0.43429448190f);
}

void measurement_cal_curve_identity(measurement_cal_curve_t *curve)
{
    if (curve == NULL)
    {
        return;
    }
    for (uint8_t i = 0u; i < MEASUREMENT_CAL_CURVE_KNOT_COUNT; i++)
    {
        curve->knot[i] = (measurement_cal_curve_matrix_t){1.0f, 0.0f, 0.0f, 1.0f};
    }
}

measurement_cal_curve_status_t measurement_cal_curve_apply(
    const measurement_cal_curve_t *curve,
    measurement_complex_t osl_z_ohms,
    float rref_ohms,
    measurement_complex_t *corrected_z_ohms)
{
    if ((curve == NULL) || (corrected_z_ohms == NULL) ||
        !measurement_complex_is_finite(osl_z_ohms) ||
        !isfinite(rref_ohms) || (rref_ohms <= 0.0f))
    {
        return MEASUREMENT_CAL_CURVE_INVALID_ARG;
    }
    for (uint8_t i = 0u; i < MEASUREMENT_CAL_CURVE_KNOT_COUNT; i++)
    {
        const measurement_cal_curve_matrix_t *matrix = &curve->knot[i];
        if (!isfinite(matrix->m00) || !isfinite(matrix->m01) ||
            !isfinite(matrix->m10) || !isfinite(matrix->m11))
        {
            return MEASUREMENT_CAL_CURVE_NONFINITE;
        }
    }
    const float ratio = measurement_complex_mag(osl_z_ohms) / rref_ohms;
    if (!isfinite(ratio))
    {
        return MEASUREMENT_CAL_CURVE_NONFINITE;
    }
    if ((ratio < 0.1f) || (ratio > 10.0f))
    {
        return MEASUREMENT_CAL_CURVE_OUT_OF_DOMAIN;
    }
    const uint8_t lower = (ratio < 1.0f) ? (uint8_t)0u : (uint8_t)1u;
    const float decade_ratio = (lower == 0u) ? ratio * 10.0f : ratio;
    float fraction = log10_1_to_10(decade_ratio);
    if (fraction < 0.0f)
    {
        fraction = 0.0f;
    }
    else if (fraction > 1.0f)
    {
        fraction = 1.0f;
    }
    const measurement_cal_curve_matrix_t *a = &curve->knot[lower];
    const measurement_cal_curve_matrix_t *b = &curve->knot[lower + 1u];
    const float m00 = a->m00 + fraction * (b->m00 - a->m00);
    const float m01 = a->m01 + fraction * (b->m01 - a->m01);
    const float m10 = a->m10 + fraction * (b->m10 - a->m10);
    const float m11 = a->m11 + fraction * (b->m11 - a->m11);
    const measurement_complex_t corrected = {
        .re = m00 * osl_z_ohms.re + m01 * osl_z_ohms.im,
        .im = m10 * osl_z_ohms.re + m11 * osl_z_ohms.im,
    };
    if (!measurement_complex_is_finite(corrected))
    {
        return MEASUREMENT_CAL_CURVE_NONFINITE;
    }
    *corrected_z_ohms = corrected;
    return MEASUREMENT_CAL_CURVE_OK;
}
