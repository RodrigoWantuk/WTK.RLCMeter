#ifndef WTK_MEASUREMENT_CALIBRATION_SOLVER_H
#define WTK_MEASUREMENT_CALIBRATION_SOLVER_H

#include <stdbool.h>
#include <stdint.h>

#include "measurement/measurement_calibration.h"

typedef enum
{
    MEASUREMENT_CAL_SOLVER_OK = 0,
    MEASUREMENT_CAL_SOLVER_INVALID_ARG,
    MEASUREMENT_CAL_SOLVER_KEY_MISMATCH,
    MEASUREMENT_CAL_SOLVER_MISSING_STANDARDS,
    MEASUREMENT_CAL_SOLVER_MISSING_OPEN,
    MEASUREMENT_CAL_SOLVER_MISSING_SHORT,
    MEASUREMENT_CAL_SOLVER_MISSING_LOAD,
    MEASUREMENT_CAL_SOLVER_UNSTABLE,
    MEASUREMENT_CAL_SOLVER_UNSTABLE_OPEN,
    MEASUREMENT_CAL_SOLVER_UNSTABLE_SHORT,
    MEASUREMENT_CAL_SOLVER_UNSTABLE_LOAD,
    MEASUREMENT_CAL_SOLVER_NO_VALID_PATH,
    MEASUREMENT_CAL_SOLVER_HG_MISSING,
    MEASUREMENT_CAL_SOLVER_ILL_CONDITIONED,
    MEASUREMENT_CAL_SOLVER_NONFINITE,
    MEASUREMENT_CAL_SOLVER_UNSUPPORTED_MODEL,
} measurement_cal_solver_status_t;

typedef enum
{
    MEASUREMENT_CAL_STANDARD_OPEN = 0,
    MEASUREMENT_CAL_STANDARD_SHORT,
    MEASUREMENT_CAL_STANDARD_LOAD,
} measurement_cal_standard_type_t;

typedef struct
{
    measurement_cal_key_t key;
    measurement_cal_standard_type_t standard;
    measurement_complex_t standard_z_ohms;
    bool standard_z_valid;
    measurement_complex_t t_1x;
    measurement_complex_t t_hg;
    measurement_complex_t hg_observed_transfer;
    int32_t temperature_mC;
    bool ret_1x_valid;
    bool ret_hg_valid;
    bool hg_observed_valid;
    bool stable;
    bool temperature_valid;
    bool present;
} measurement_cal_solver_standard_t;

typedef struct
{
    measurement_cal_solver_standard_t open;
    measurement_cal_solver_standard_t shorted;
    measurement_cal_solver_standard_t load;
} measurement_cal_solver_input_t;

typedef struct
{
    measurement_cal_key_t key;
    measurement_cal_osl_coefficients_t coefficients;
    measurement_return_channel_t fit_channel;
    int32_t temperature_mC;
    bool temperature_valid;
    bool hg_observed;
} measurement_cal_solver_solution_t;

measurement_cal_solver_status_t measurement_cal_solver_solve(
    const measurement_cal_solver_input_t *input,
    measurement_cal_solver_solution_t *solution);
measurement_cal_record_t measurement_cal_solver_make_record(
    const measurement_cal_solver_solution_t *solution);
measurement_cal_solver_status_t measurement_cal_solver_apply_osl(
    const measurement_cal_osl_coefficients_t *coefficients,
    measurement_complex_t t,
    measurement_complex_t *z_ohms);
const char *measurement_cal_solver_status_string(measurement_cal_solver_status_t status);
uint32_t measurement_cal_solver_solution_size_bytes(void);

#endif
