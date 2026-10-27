#ifndef WTK_MEASUREMENT_TRIMMED_H
#define WTK_MEASUREMENT_TRIMMED_H

#include <stdbool.h>
#include <stdint.h>
#include "measurement/measurement_dsp.h"

enum
{
    MEASUREMENT_TRIMMED_SAMPLES = 7u,
    MEASUREMENT_TRIMMED_DISCARD = 2u,
    MEASUREMENT_TRIMMED_RETAINED = 3u,
    MEASUREMENT_TRIMMED_MAX_EXTRA_CAPTURES = 8u,
};

typedef struct
{
    uint8_t samples;
    uint8_t extra_captures;
    uint8_t r_retained_mask;
    uint8_t x_retained_mask;
    bool requested;
    bool applied;
} measurement_trimmed_summary_t;

/* Coordinate-wise trimming: R and X can retain different original points.
 * Preserve all points and both masks; this is not a mean of three whole Zs. */
typedef struct
{
    measurement_complex_t points[MEASUREMENT_TRIMMED_SAMPLES];
    measurement_complex_t estimate;
    measurement_trimmed_summary_t summary;
} measurement_trimmed_t;

void measurement_trimmed_init(measurement_trimmed_t *filter);
bool measurement_trimmed_add(measurement_trimmed_t *filter, measurement_complex_t point);
bool measurement_trimmed_finish(measurement_trimmed_t *filter);

#endif
