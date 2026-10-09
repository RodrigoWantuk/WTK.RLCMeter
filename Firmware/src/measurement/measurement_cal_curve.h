#ifndef WTK_MEASUREMENT_CAL_CURVE_H
#define WTK_MEASUREMENT_CAL_CURVE_H

#include "measurement/measurement_dsp.h"

enum
{
    MEASUREMENT_CAL_CURVE_KNOT_COUNT = 3u,
    MEASUREMENT_CAL_CURVE_COEFFICIENT_COUNT = 12u,
};

typedef struct
{
    float m00;
    float m01;
    float m10;
    float m11;
} measurement_cal_curve_matrix_t;

typedef struct
{
    measurement_cal_curve_matrix_t knot[MEASUREMENT_CAL_CURVE_KNOT_COUNT];
} measurement_cal_curve_t;

typedef enum
{
    MEASUREMENT_CAL_CURVE_OK = 0,
    MEASUREMENT_CAL_CURVE_INVALID_ARG,
    MEASUREMENT_CAL_CURVE_OUT_OF_DOMAIN,
    MEASUREMENT_CAL_CURVE_NONFINITE,
} measurement_cal_curve_status_t;

void measurement_cal_curve_identity(measurement_cal_curve_t *curve);
measurement_cal_curve_status_t measurement_cal_curve_apply(
    const measurement_cal_curve_t *curve,
    measurement_complex_t osl_z_ohms,
    float rref_ohms,
    measurement_complex_t *corrected_z_ohms);

#endif
