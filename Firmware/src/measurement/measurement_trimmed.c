#include "measurement/measurement_trimmed.h"

#include <stddef.h>

void measurement_trimmed_init(measurement_trimmed_t *filter)
{
    if (filter != NULL) *filter = (measurement_trimmed_t){0};
}

bool measurement_trimmed_add(measurement_trimmed_t *filter, measurement_complex_t point)
{
    if (filter == NULL || filter->summary.samples >= MEASUREMENT_TRIMMED_SAMPLES ||
        !measurement_complex_is_finite(point)) return false;
    filter->points[filter->summary.samples++] = point;
    return true;
}

static float trimmed_coordinate(const measurement_trimmed_t *filter, bool imaginary,
                                uint8_t *retained_mask)
{
    uint8_t indices[MEASUREMENT_TRIMMED_SAMPLES];
    float values[MEASUREMENT_TRIMMED_SAMPLES];
    for (uint8_t i = 0u; i < MEASUREMENT_TRIMMED_SAMPLES; i++)
    {
        const float value = imaginary ? filter->points[i].im : filter->points[i].re;
        uint8_t j = i;
        /* Stable ties retain deterministic original indices. */
        while (j > 0u && values[j - 1u] > value)
        {
            values[j] = values[j - 1u];
            indices[j] = indices[j - 1u];
            j--;
        }
        values[j] = value;
        indices[j] = i;
    }
    float mean = 0.0f;
    *retained_mask = 0u;
    for (uint8_t i = MEASUREMENT_TRIMMED_DISCARD;
         i < MEASUREMENT_TRIMMED_SAMPLES - MEASUREMENT_TRIMMED_DISCARD; i++)
    {
        /* Scale before adding so even finite large inputs cannot overflow a sum. */
        mean += values[i] / (float)MEASUREMENT_TRIMMED_RETAINED;
        *retained_mask |= (uint8_t)(1u << indices[i]);
    }
    return mean;
}

bool measurement_trimmed_finish(measurement_trimmed_t *filter)
{
    if (filter == NULL || filter->summary.samples != MEASUREMENT_TRIMMED_SAMPLES) return false;
    filter->estimate.re = trimmed_coordinate(filter, false, &filter->summary.r_retained_mask);
    filter->estimate.im = trimmed_coordinate(filter, true, &filter->summary.x_retained_mask);
    filter->summary.applied = measurement_complex_is_finite(filter->estimate);
    return filter->summary.applied;
}
