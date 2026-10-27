#ifndef WTK_APP_CURVE_CACHE_H
#define WTK_APP_CURVE_CACHE_H

#include "measurement/measurement_cal_curve_store.h"

/* Optional WCRV only: sparse 36 x 48-byte curves + presence/binding metadata.
 * Immutable after boot; no external Flash transactions during a quiet window. */
typedef struct
{
    measurement_cal_curve_t curves[MEASUREMENT_CONDITION_REV1_INDEX_COUNT];
    uint8_t present[MEASUREMENT_CONDITION_REV1_INDEX_COUNT];
    uint32_t osl_sequence;
    uint32_t osl_crc32;
    uint32_t curve_sequence;
    bool ready;
} app_curve_cache_t;
bsp_status_t app_curve_cache_load(app_curve_cache_t *cache,
                                  const measurement_cal_curve_store_t *store,
                                  uint8_t *scratch, size_t scratch_size);
bsp_status_t app_curve_cache_get(const app_curve_cache_t *cache,
                                 const measurement_cal_key_t *key,
                                 uint32_t osl_sequence, uint32_t osl_crc32,
                                 measurement_cal_curve_t *curve);

#endif
