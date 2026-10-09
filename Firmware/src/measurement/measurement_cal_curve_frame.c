#include "measurement/measurement_cal_curve_frame.h"

#include <float.h>
#include <math.h>
#include <string.h>

#include "storage/storage_crc32.h"

_Static_assert(sizeof(float) == 4u && FLT_RADIX == 2 && FLT_MANT_DIG == 24,
               "curve frame requires IEEE-754 binary32");


static uint16_t read_u16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8u));
}

static uint32_t read_u32(const uint8_t *src)
{
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8u) |
           ((uint32_t)src[2] << 16u) | ((uint32_t)src[3] << 24u);
}

static float read_f32(const uint8_t *src)
{
    const uint32_t bits = read_u32(src);
    float value;
    (void)memcpy(&value, &bits, sizeof(value));
    return value;
}

static measurement_cal_key_t record_key(const uint8_t *record)
{
    return measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                               MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                               (hw_range_id_t)record[0],
                               (hw_excitation_freq_t)record[1],
                               (hw_excitation_amp_t)record[2]);
}

bool measurement_cal_curve_frame_validate(const uint8_t *frame,
                                          size_t size,
                                          uint32_t expected_osl_sequence,
                                          uint32_t expected_osl_frame_crc32,
                                          bool require_qualified,
                                          measurement_cal_curve_frame_info_t *info)
{
    if ((frame == NULL) || (size < MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES +
                                 MEASUREMENT_CAL_CURVE_FRAME_TRAILER_BYTES) ||
        (size > MEASUREMENT_CAL_CURVE_FRAME_MAX_BYTES))
    {
        return false;
    }
    const uint16_t count = read_u16(&frame[6]);
    const uint16_t flags = read_u16(&frame[14]);
    if ((read_u32(frame) != MEASUREMENT_CAL_CURVE_FRAME_MAGIC) ||
        (read_u16(&frame[4]) != MEASUREMENT_CAL_CURVE_FRAME_VERSION) ||
        (count == 0u) || (count > MEASUREMENT_CONDITION_REV1_MAX_SUPPORTED) ||
        (size != MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES +
                 (size_t)count * MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES +
                 MEASUREMENT_CAL_CURVE_FRAME_TRAILER_BYTES) ||
        (read_u32(&frame[8]) != MEASUREMENT_CAL_HARDWARE_REV1) ||
        (read_u16(&frame[12]) != MEASUREMENT_CAL_MODEL_VERSION_CURRENT) ||
        ((flags & (uint16_t)~MEASUREMENT_CAL_CURVE_FRAME_FLAG_QUALIFIED) != 0u) ||
        (require_qualified &&
         ((flags & MEASUREMENT_CAL_CURVE_FRAME_FLAG_QUALIFIED) == 0u)) ||
        (expected_osl_sequence == 0u) ||
        (read_u32(&frame[16]) != expected_osl_sequence) ||
        (read_u32(&frame[20]) == 0u) ||
        (read_u32(&frame[24]) != expected_osl_frame_crc32) ||
        (read_u32(&frame[size - 4u]) != MEASUREMENT_CAL_COMMIT_MARKER) ||
        (read_u32(&frame[size - 8u]) != storage_crc32(frame, size - 8u)))
    {
        return false;
    }

    uint64_t seen = 0u;
    for (uint16_t i = 0u; i < count; i++)
    {
        const uint8_t *record = &frame[MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES +
                                       (size_t)i * MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES];
        const measurement_cal_key_t key = record_key(record);
        uint8_t bit;
        if ((record[3] != 0u) ||
            (read_u32(&record[52]) != storage_crc32(record, 52u)) ||
            !measurement_cal_rev1_condition_bit(&key, &bit) ||
            ((seen & (UINT64_C(1) << bit)) != 0u))
        {
            return false;
        }
        seen |= UINT64_C(1) << bit;
        for (uint8_t j = 0u; j < MEASUREMENT_CAL_CURVE_COEFFICIENT_COUNT; j++)
        {
            if (!isfinite(read_f32(&record[4u + (size_t)j * 4u])))
            {
                return false;
            }
        }
    }
    if (info != NULL)
    {
        *info = (measurement_cal_curve_frame_info_t){
            .osl_sequence = read_u32(&frame[16]),
            .osl_frame_crc32 = read_u32(&frame[24]),
            .curve_sequence = read_u32(&frame[20]),
            .record_count = count,
            .qualified = (flags & MEASUREMENT_CAL_CURVE_FRAME_FLAG_QUALIFIED) != 0u,
        };
    }
    return true;
}

bool measurement_cal_curve_frame_find(const uint8_t *frame,
                                      size_t size,
                                      const measurement_cal_key_t *key,
                                      uint32_t expected_osl_sequence,
                                      uint32_t expected_osl_frame_crc32,
                                      bool require_qualified,
                                      measurement_cal_curve_t *curve)
{
    if ((frame == NULL) || (size < MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES) ||
        (key == NULL) || (curve == NULL) ||
        !measurement_cal_curve_frame_validate(frame, size, expected_osl_sequence,
                                              expected_osl_frame_crc32,
                                              require_qualified, NULL))
    {
        return false;
    }
    const uint16_t count = read_u16(&frame[6]);
    for (uint16_t i = 0u; i < count; i++)
    {
        const uint8_t *record = &frame[MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES +
                                       (size_t)i * MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES];
        const measurement_cal_key_t entry_key = record_key(record);
        if (measurement_cal_key_equal(&entry_key, key))
        {
            return measurement_cal_curve_record_decode(record,
                MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES, key, curve);
        }
    }
    return false;
}

bool measurement_cal_curve_record_decode(const uint8_t *record,
                                         size_t size,
                                         const measurement_cal_key_t *key,
                                         measurement_cal_curve_t *curve)
{
    if ((record == NULL) || (size != MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES) ||
        (key == NULL) || (curve == NULL) || (record[3] != 0u) ||
        (read_u32(&record[52]) != storage_crc32(record, 52u)))
    {
        return false;
    }
    const measurement_cal_key_t entry_key = record_key(record);
    if (!measurement_cal_key_equal(&entry_key, key) ||
        !measurement_cal_condition_allowed(key->range_id, key->frequency, key->amplitude))
    {
        return false;
    }
    measurement_cal_curve_t decoded = {0};
    for (uint8_t j = 0u; j < MEASUREMENT_CAL_CURVE_KNOT_COUNT; j++)
    {
        const uint8_t *matrix = &record[4u + (size_t)j * 16u];
        decoded.knot[j] = (measurement_cal_curve_matrix_t){
            .m00 = read_f32(&matrix[0]),
            .m01 = read_f32(&matrix[4]),
            .m10 = read_f32(&matrix[8]),
            .m11 = read_f32(&matrix[12]),
        };
        if (!isfinite(decoded.knot[j].m00) || !isfinite(decoded.knot[j].m01) ||
            !isfinite(decoded.knot[j].m10) || !isfinite(decoded.knot[j].m11))
        {
            return false;
        }
    }
    *curve = decoded;
    return true;
}
