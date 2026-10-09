#ifndef WTK_MEASUREMENT_CAL_CURVE_STORE_H
#define WTK_MEASUREMENT_CAL_CURVE_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "measurement/measurement_cal_curve_frame.h"

typedef bsp_status_t (*measurement_cal_curve_read_fn)(uint32_t address,
                                                       void *dst,
                                                       size_t size,
                                                       void *user);

typedef struct
{
    measurement_cal_curve_read_fn read;
    void *user;
    uint32_t slot_start;
    uint32_t osl_sequence;
    uint32_t osl_frame_crc32;
    uint32_t curve_sequence;
    uint8_t record_index[MEASUREMENT_CONDITION_REV1_MAX_SUPPORTED];
    bool active;
} measurement_cal_curve_store_t;

bsp_status_t measurement_cal_curve_store_load(measurement_cal_curve_store_t *store,
                                              measurement_cal_curve_read_fn read,
                                              void *user,
                                              uint32_t capacity_bytes,
                                              uint32_t osl_sequence,
                                              uint32_t osl_frame_crc32,
                                              uint8_t *scratch,
                                              size_t scratch_size);
bsp_status_t measurement_cal_curve_store_read(const measurement_cal_curve_store_t *store,
                                              const measurement_cal_key_t *key,
                                              uint32_t active_osl_sequence,
                                              uint32_t active_osl_frame_crc32,
                                              measurement_cal_curve_t *curve);

#endif
