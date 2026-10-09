#ifndef WTK_MEASUREMENT_CAL_CURVE_FRAME_H
#define WTK_MEASUREMENT_CAL_CURVE_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "measurement/measurement_calibration.h"
#include "measurement/measurement_cal_curve.h"

enum
{
    MEASUREMENT_CAL_CURVE_FRAME_MAGIC = 0x56524357u, /* WCRV */
    MEASUREMENT_CAL_CURVE_FRAME_VERSION = 1u,
    MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES = 28u,
    MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES = 56u,
    MEASUREMENT_CAL_CURVE_FRAME_TRAILER_BYTES = 8u,
    MEASUREMENT_CAL_CURVE_FRAME_MAX_BYTES =
        MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES +
        MEASUREMENT_CONDITION_REV1_MAX_SUPPORTED * MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES +
        MEASUREMENT_CAL_CURVE_FRAME_TRAILER_BYTES,
    MEASUREMENT_CAL_CURVE_FRAME_FLAG_QUALIFIED = 1u,
};

typedef struct
{
    uint32_t osl_sequence;
    uint32_t osl_frame_crc32;
    uint32_t curve_sequence;
    uint16_t record_count;
    bool qualified;
} measurement_cal_curve_frame_info_t;

bool measurement_cal_curve_frame_validate(const uint8_t *frame,
                                          size_t size,
                                          uint32_t expected_osl_sequence,
                                          uint32_t expected_osl_frame_crc32,
                                          bool require_qualified,
                                          measurement_cal_curve_frame_info_t *info);
bool measurement_cal_curve_frame_find(const uint8_t *frame,
                                      size_t size,
                                      const measurement_cal_key_t *key,
                                      uint32_t expected_osl_sequence,
                                      uint32_t expected_osl_frame_crc32,
                                      bool require_qualified,
                                      measurement_cal_curve_t *curve);
bool measurement_cal_curve_record_decode(const uint8_t *record,
                                         size_t size,
                                         const measurement_cal_key_t *key,
                                         measurement_cal_curve_t *curve);

#endif
