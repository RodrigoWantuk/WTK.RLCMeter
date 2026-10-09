#include "measurement/measurement_cal_curve_frame.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "storage/storage_crc32.h"

static int expect_true(bool value, const char *message)
{
    if (!value)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static void write_u16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8u);
}

static void write_u32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8u);
    dst[2] = (uint8_t)(value >> 16u);
    dst[3] = (uint8_t)(value >> 24u);
}

static void write_f32(uint8_t *dst, float value)
{
    uint32_t bits;
    (void)memcpy(&bits, &value, sizeof(bits));
    write_u32(dst, bits);
}

static void seal(uint8_t *frame, size_t size)
{
    write_u32(&frame[size - 8u], storage_crc32(frame, size - 8u));
    write_u32(&frame[size - 4u], MEASUREMENT_CAL_COMMIT_MARKER);
}

int main(void)
{
    int failures = 0;
    enum { SIZE = MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES +
                  MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES +
                  MEASUREMENT_CAL_CURVE_FRAME_TRAILER_BYTES };
    uint8_t frame[SIZE] = {0};
    write_u32(&frame[0], MEASUREMENT_CAL_CURVE_FRAME_MAGIC);
    write_u16(&frame[4], MEASUREMENT_CAL_CURVE_FRAME_VERSION);
    write_u16(&frame[6], 1u);
    write_u32(&frame[8], MEASUREMENT_CAL_HARDWARE_REV1);
    write_u16(&frame[12], MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
    write_u32(&frame[16], 42u);
    write_u32(&frame[20], 7u);
    write_u32(&frame[24], 0x12345678u);
    uint8_t *record = &frame[MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES];
    record[0] = (uint8_t)HW_RANGE_ID_1K;
    record[1] = (uint8_t)HW_EXCITATION_FREQ_1KHZ;
    record[2] = (uint8_t)HW_EXCITATION_AMP_100MVRMS;
    for (uint8_t i = 0u; i < MEASUREMENT_CAL_CURVE_KNOT_COUNT; i++)
    {
        write_f32(&record[4u + (size_t)i * 16u], 1.0f);
        write_f32(&record[16u + (size_t)i * 16u], 1.0f);
    }
    write_u32(&record[52], storage_crc32(record, 52u));
    seal(frame, SIZE);
    measurement_cal_curve_frame_info_t info = {0};
    failures += expect_true(measurement_cal_curve_frame_validate(frame, SIZE, 42u, 0x12345678u,
                                                                false, &info),
                            "candidate frame validates structurally");
    failures += expect_true(info.record_count == 1u && info.curve_sequence == 7u &&
                            info.osl_frame_crc32 == 0x12345678u &&
                            !info.qualified, "candidate metadata decoded");
    failures += expect_true(!measurement_cal_curve_frame_validate(frame, SIZE, 42u, 0x12345678u,
                                                                 true, NULL),
                            "unqualified candidate cannot become active correction");
    failures += expect_true(!measurement_cal_curve_frame_validate(frame, SIZE, 43u, 0x12345678u,
                                                                 false, NULL),
                            "new OSL sequence invalidates old curve");
    failures += expect_true(!measurement_cal_curve_frame_validate(frame, SIZE, 42u, 0x12345679u,
                                                                 false, NULL),
                            "different OSL frame invalidates curve");
    write_u32(&frame[20], 0u);
    seal(frame, SIZE);
    failures += expect_true(!measurement_cal_curve_frame_validate(frame, SIZE, 42u, 0x12345678u,
                                                                 false, NULL),
                            "zero curve sequence rejected");
    write_u32(&frame[20], 7u);
    seal(frame, SIZE);
    const measurement_cal_key_t key = measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
        MEASUREMENT_CAL_MODEL_VERSION_CURRENT, HW_RANGE_ID_1K,
        HW_EXCITATION_FREQ_1KHZ, HW_EXCITATION_AMP_100MVRMS);
    measurement_cal_curve_t curve;
    failures += expect_true(measurement_cal_curve_frame_find(frame, SIZE, &key, 42u,
                                                             0x12345678u, false, &curve),
                            "exact condition can be found");
    failures += expect_true(curve.knot[1].m00 == 1.0f && curve.knot[1].m11 == 1.0f,
                            "matrix decoded in the specified order");
    frame[SIZE - 4u] ^= 1u;
    failures += expect_true(!measurement_cal_curve_frame_validate(frame, SIZE, 42u, 0x12345678u,
                                                                 false, NULL),
                            "missing commit rejected");
    seal(frame, SIZE);
    record[0] = (uint8_t)HW_RANGE_ID_INVALID;
    write_u32(&record[52], storage_crc32(record, 52u));
    seal(frame, SIZE);
    failures += expect_true(!measurement_cal_curve_frame_validate(frame, SIZE, 42u, 0x12345678u,
                                                                 false, NULL),
                            "invalid condition rejected");
    record[0] = (uint8_t)HW_RANGE_ID_1K;
    write_f32(&record[4], NAN);
    write_u32(&record[52], storage_crc32(record, 52u));
    seal(frame, SIZE);
    failures += expect_true(!measurement_cal_curve_frame_validate(frame, SIZE, 42u, 0x12345678u,
                                                                 false, NULL),
                            "nonfinite coefficient rejected");
    write_f32(&record[4], 1.0f);
    write_u32(&record[52], storage_crc32(record, 52u));
    seal(frame, SIZE);
    frame[28] ^= 1u;
    failures += expect_true(!measurement_cal_curve_frame_validate(frame, SIZE, 42u, 0x12345678u,
                                                                 false, NULL),
                            "payload corruption rejected by CRC");
    failures += expect_true(!measurement_cal_curve_frame_find(frame, 8u, &key, 42u,
                                                              0x12345678u, false, &curve),
                            "truncated find rejected safely");
    return failures == 0 ? 0 : 1;
}
