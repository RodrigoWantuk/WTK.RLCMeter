#include "measurement/measurement_calibration_solver.h"

#include <math.h>
#include <stddef.h>

enum
{
    OSL_STANDARD_COUNT = 3u,
};

#define OSL_MIN_SEPARATION (1.0e-5f)

static bool finite_complex(measurement_complex_t value)
{
    return measurement_complex_is_finite(value);
}

static bool standard_type_matches(const measurement_cal_solver_standard_t *standard,
                                  measurement_cal_standard_type_t expected)
{
    return (standard != NULL) && (standard->standard == expected);
}

static bool keys_match(const measurement_cal_key_t *a, const measurement_cal_key_t *b)
{
    return measurement_cal_key_equal(a, b);
}

static bool finite_standard(const measurement_cal_solver_standard_t *standard)
{
    return (standard != NULL) &&
           finite_complex(standard->t_1x) &&
           finite_complex(standard->t_hg) &&
           finite_complex(standard->hg_observed_transfer) &&
           finite_complex(standard->standard_z_ohms);
}

static bool load_valid(const measurement_cal_solver_standard_t *load)
{
    return (load != NULL) &&
           load->standard_z_valid &&
           finite_complex(load->standard_z_ohms) &&
           !measurement_complex_near_zero(load->standard_z_ohms, OSL_MIN_SEPARATION);
}

static bool input_shape_valid(const measurement_cal_solver_input_t *input)
{
    return (input != NULL) &&
           standard_type_matches(&input->open, MEASUREMENT_CAL_STANDARD_OPEN) &&
           standard_type_matches(&input->shorted, MEASUREMENT_CAL_STANDARD_SHORT) &&
           standard_type_matches(&input->load, MEASUREMENT_CAL_STANDARD_LOAD) &&
           keys_match(&input->open.key, &input->shorted.key) &&
           keys_match(&input->open.key, &input->load.key) &&
           (input->open.key.model_version == MEASUREMENT_CAL_MODEL_VERSION_OSL_MOBIUS_V1) &&
           load_valid(&input->load);
}

static bool stable_triplet(const measurement_cal_solver_input_t *input)
{
    return input->open.stable && input->shorted.stable && input->load.stable;
}

static uint8_t temperature_count(const measurement_cal_solver_input_t *input)
{
    uint8_t count = 0u;
    count += input->open.temperature_valid ? 1u : 0u;
    count += input->shorted.temperature_valid ? 1u : 0u;
    count += input->load.temperature_valid ? 1u : 0u;
    return count;
}

static int32_t mean_temperature_mC(const measurement_cal_solver_input_t *input)
{
    int32_t sum = 0;
    if (input->open.temperature_valid)
    {
        sum += input->open.temperature_mC;
    }
    if (input->shorted.temperature_valid)
    {
        sum += input->shorted.temperature_mC;
    }
    if (input->load.temperature_valid)
    {
        sum += input->load.temperature_mC;
    }
    const uint8_t count = temperature_count(input);
    return (count == 0u) ? 0 : (sum / (int32_t)count);
}

static bool mean_hg_transfer(const measurement_cal_solver_input_t *input,
                             measurement_complex_t *transfer)
{
    if (transfer == NULL)
    {
        return false;
    }
    measurement_complex_t sum = measurement_complex(0.0f, 0.0f);
    uint8_t count = 0u;
    const measurement_cal_solver_standard_t *standards[OSL_STANDARD_COUNT] = {
        &input->open,
        &input->shorted,
        &input->load,
    };
    for (uint8_t i = 0u; i < OSL_STANDARD_COUNT; i++)
    {
        if (standards[i]->hg_observed_valid &&
            finite_complex(standards[i]->hg_observed_transfer))
        {
            sum = measurement_complex_add(sum, standards[i]->hg_observed_transfer);
            count++;
        }
    }
    if (count == 0u)
    {
        return false;
    }
    *transfer = measurement_complex(sum.re / (float)count, sum.im / (float)count);
    return finite_complex(*transfer) && !measurement_complex_near_zero(*transfer, OSL_MIN_SEPARATION);
}

static measurement_cal_solver_status_t solve_path(measurement_complex_t t_open,
                                                  measurement_complex_t t_short,
                                                  measurement_complex_t t_load,
                                                  measurement_complex_t z_load,
                                                  measurement_cal_osl_coefficients_t *coefficients)
{
    if (coefficients == NULL)
    {
        return MEASUREMENT_CAL_SOLVER_INVALID_ARG;
    }
    if (!finite_complex(t_open) || !finite_complex(t_short) ||
        !finite_complex(t_load) || !finite_complex(z_load))
    {
        return MEASUREMENT_CAL_SOLVER_NONFINITE;
    }
    if (measurement_complex_near_zero(measurement_complex_sub(t_load, t_short), OSL_MIN_SEPARATION) ||
        measurement_complex_near_zero(measurement_complex_sub(t_open, t_short), OSL_MIN_SEPARATION))
    {
        return MEASUREMENT_CAL_SOLVER_ILL_CONDITIONED;
    }

    measurement_complex_t load_minus_open = measurement_complex_sub(t_load, t_open);
    measurement_complex_t load_minus_short = measurement_complex_sub(t_load, t_short);
    measurement_complex_t ratio = measurement_complex(0.0f, 0.0f);
    if (measurement_complex_div(load_minus_open, load_minus_short, &ratio) != MEASUREMENT_STATUS_OK)
    {
        return MEASUREMENT_CAL_SOLVER_ILL_CONDITIONED;
    }
    coefficients->load_z_ohms = z_load;
    coefficients->t_short = t_short;
    coefficients->t_open = t_open;
    coefficients->k = measurement_complex_mul(z_load, ratio);
    if (!finite_complex(coefficients->k))
    {
        return MEASUREMENT_CAL_SOLVER_NONFINITE;
    }
    return MEASUREMENT_CAL_SOLVER_OK;
}

measurement_cal_solver_status_t measurement_cal_solver_solve(
    const measurement_cal_solver_input_t *input,
    measurement_cal_solver_solution_t *solution)
{
    if ((input == NULL) || (solution == NULL))
    {
        return MEASUREMENT_CAL_SOLVER_INVALID_ARG;
    }
    *solution = (measurement_cal_solver_solution_t){0};
    if (!input_shape_valid(input))
    {
        return MEASUREMENT_CAL_SOLVER_KEY_MISMATCH;
    }
    if (!finite_standard(&input->open) ||
        !finite_standard(&input->shorted) ||
        !finite_standard(&input->load))
    {
        return MEASUREMENT_CAL_SOLVER_NONFINITE;
    }
    if (!stable_triplet(input))
    {
        return MEASUREMENT_CAL_SOLVER_UNSTABLE;
    }

    measurement_complex_t h_hg = measurement_complex(0.0f, 0.0f);
    const bool hg_observed = mean_hg_transfer(input, &h_hg);
    measurement_cal_osl_coefficients_t coefficients = {0};
    measurement_cal_solver_status_t status = MEASUREMENT_CAL_SOLVER_NO_VALID_PATH;
    measurement_return_channel_t channel = MEASUREMENT_RETURN_1X;

    if (input->open.ret_1x_valid && input->shorted.ret_1x_valid && input->load.ret_1x_valid)
    {
        status = solve_path(input->open.t_1x,
                            input->shorted.t_1x,
                            input->load.t_1x,
                            input->load.standard_z_ohms,
                            &coefficients);
        channel = MEASUREMENT_RETURN_1X;
    }
    if ((status != MEASUREMENT_CAL_SOLVER_OK) &&
        input->open.ret_hg_valid && input->shorted.ret_hg_valid && input->load.ret_hg_valid)
    {
        if (!hg_observed)
        {
            return MEASUREMENT_CAL_SOLVER_HG_MISSING;
        }
        status = solve_path(input->open.t_hg,
                            input->shorted.t_hg,
                            input->load.t_hg,
                            input->load.standard_z_ohms,
                            &coefficients);
        channel = MEASUREMENT_RETURN_HG;
    }
    if (status != MEASUREMENT_CAL_SOLVER_OK)
    {
        return status;
    }

    solution->key = input->open.key;
    solution->coefficients = coefficients;
    solution->coefficients.ret_hg_transfer = hg_observed ?
                                                 h_hg :
                                                 measurement_dsp_config_ideal(input->open.key.range_id).ret_hg_transfer;
    solution->fit_channel = channel;
    solution->temperature_valid = temperature_count(input) != 0u;
    solution->temperature_mC = mean_temperature_mC(input);
    solution->hg_observed = hg_observed;
    return MEASUREMENT_CAL_SOLVER_OK;
}

measurement_cal_record_t measurement_cal_solver_make_record(
    const measurement_cal_solver_solution_t *solution)
{
    measurement_cal_record_t record = {0};
    if (solution == NULL)
    {
        return record;
    }
    record.key = solution->key;
    record.record_type = MEASUREMENT_CAL_RECORD_CONDITION;
    record.temperature_mC = solution->temperature_valid ? solution->temperature_mC : 0;
    record.correction = measurement_cal_make_osl_correction(&solution->coefficients,
                                                            solution->temperature_valid,
                                                            solution->hg_observed);
    record.condition_id = measurement_cal_condition_id(&record.key);
    return record;
}

measurement_cal_solver_status_t measurement_cal_solver_apply_osl(
    const measurement_cal_osl_coefficients_t *coefficients,
    measurement_complex_t t,
    measurement_complex_t *z_ohms)
{
    if ((coefficients == NULL) || (z_ohms == NULL))
    {
        return MEASUREMENT_CAL_SOLVER_INVALID_ARG;
    }
    if (!finite_complex(t) || !finite_complex(coefficients->t_short) ||
        !finite_complex(coefficients->t_open) || !finite_complex(coefficients->k))
    {
        return MEASUREMENT_CAL_SOLVER_NONFINITE;
    }
    const measurement_complex_t denominator = measurement_complex_sub(t, coefficients->t_open);
    if (measurement_complex_near_zero(denominator, OSL_MIN_SEPARATION))
    {
        return MEASUREMENT_CAL_SOLVER_ILL_CONDITIONED;
    }
    measurement_complex_t ratio = measurement_complex(0.0f, 0.0f);
    if (measurement_complex_div(measurement_complex_sub(t, coefficients->t_short),
                                denominator,
                                &ratio) != MEASUREMENT_STATUS_OK)
    {
        return MEASUREMENT_CAL_SOLVER_ILL_CONDITIONED;
    }
    *z_ohms = measurement_complex_mul(coefficients->k, ratio);
    return finite_complex(*z_ohms) ? MEASUREMENT_CAL_SOLVER_OK : MEASUREMENT_CAL_SOLVER_NONFINITE;
}

const char *measurement_cal_solver_status_string(measurement_cal_solver_status_t status)
{
    switch (status)
    {
    case MEASUREMENT_CAL_SOLVER_OK:
        return "OK";
    case MEASUREMENT_CAL_SOLVER_INVALID_ARG:
        return "INVALID_ARG";
    case MEASUREMENT_CAL_SOLVER_KEY_MISMATCH:
        return "KEY_MISMATCH";
    case MEASUREMENT_CAL_SOLVER_MISSING_STANDARDS:
        return "MISSING_STANDARDS";
    case MEASUREMENT_CAL_SOLVER_UNSTABLE:
        return "UNSTABLE";
    case MEASUREMENT_CAL_SOLVER_NO_VALID_PATH:
        return "NO_VALID_PATH";
    case MEASUREMENT_CAL_SOLVER_HG_MISSING:
        return "HG_MISSING";
    case MEASUREMENT_CAL_SOLVER_ILL_CONDITIONED:
        return "ILL_CONDITIONED";
    case MEASUREMENT_CAL_SOLVER_NONFINITE:
        return "NONFINITE";
    case MEASUREMENT_CAL_SOLVER_UNSUPPORTED_MODEL:
    default:
        return "UNSUPPORTED_MODEL";
    }
}

uint32_t measurement_cal_solver_solution_size_bytes(void)
{
    return (uint32_t)sizeof(measurement_cal_solver_solution_t);
}
