#include "measurement/measurement_cal_curve_store.h"

#include <stdio.h>
#include <string.h>

#include "storage/storage_crc32.h"
#include "storage/storage_layout.h"

enum { FLASH_BYTES = 65536u, FRAME_BYTES = MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES +
    MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES + MEASUREMENT_CAL_CURVE_FRAME_TRAILER_BYTES };

static uint8_t flash[FLASH_BYTES];

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

static void make_frame(uint8_t dst[FRAME_BYTES], uint32_t curve_sequence, bool qualified)
{
    (void)memset(dst, 0, FRAME_BYTES);
    write_u32(&dst[0], MEASUREMENT_CAL_CURVE_FRAME_MAGIC);
    write_u16(&dst[4], MEASUREMENT_CAL_CURVE_FRAME_VERSION);
    write_u16(&dst[6], 1u);
    write_u32(&dst[8], MEASUREMENT_CAL_HARDWARE_REV1);
    write_u16(&dst[12], MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
    write_u16(&dst[14], qualified ? MEASUREMENT_CAL_CURVE_FRAME_FLAG_QUALIFIED : 0u);
    write_u32(&dst[16], 42u);
    write_u32(&dst[20], curve_sequence);
    write_u32(&dst[24], 0x12345678u);
    uint8_t *record = &dst[MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES];
    record[0] = (uint8_t)HW_RANGE_ID_1K;
    record[1] = (uint8_t)HW_EXCITATION_FREQ_1KHZ;
    record[2] = (uint8_t)HW_EXCITATION_AMP_100MVRMS;
    for (uint8_t i = 0u; i < MEASUREMENT_CAL_CURVE_KNOT_COUNT; i++)
    {
        write_f32(&record[4u + (size_t)i * 16u], 1.0f);
        write_f32(&record[16u + (size_t)i * 16u], 1.0f);
    }
    write_u32(&record[52], storage_crc32(record, 52u));
    write_u32(&dst[FRAME_BYTES - 8u], storage_crc32(dst, FRAME_BYTES - 8u));
    write_u32(&dst[FRAME_BYTES - 4u], MEASUREMENT_CAL_COMMIT_MARKER);
}

static bsp_status_t fake_read(uint32_t address, void *dst, size_t size, void *user)
{
    (void)user;
    if ((address > FLASH_BYTES) || (size > FLASH_BYTES - address))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    (void)memcpy(dst, &flash[address], size);
    return BSP_STATUS_OK;
}

int main(void)
{
    int failures = 0;
    uint8_t scratch[MEASUREMENT_CAL_CURVE_FRAME_MAX_BYTES];
    storage_partition_t slot_a;
    storage_partition_t slot_b;
    failures += expect_true(storage_layout_partition(FLASH_BYTES, STORAGE_PARTITION_CAL_CURVE_A,
                                                     &slot_a), "slot A exists");
    failures += expect_true(storage_layout_partition(FLASH_BYTES, STORAGE_PARTITION_CAL_CURVE_B,
                                                     &slot_b), "slot B exists");
    (void)memset(flash, 0xFF, sizeof(flash));
    measurement_cal_curve_store_t store;
    failures += expect_true(measurement_cal_curve_store_load(&store, fake_read, NULL,
        FLASH_BYTES, 42u, 0x12345678u, scratch, sizeof(scratch)) == BSP_STATUS_NOT_SUPPORTED,
        "blank Flash leaves overlay inactive");

    make_frame(&flash[slot_a.start], 7u, true);
    make_frame(&flash[slot_b.start], 8u, false);
    failures += expect_true(measurement_cal_curve_store_load(&store, fake_read, NULL,
        FLASH_BYTES, 42u, 0x12345678u, scratch, sizeof(scratch)) == BSP_STATUS_OK,
        "qualified A survives newer unqualified B");
    failures += expect_true(store.slot_start == slot_a.start && store.curve_sequence == 7u,
                            "active slot is older qualified A");
    const measurement_cal_key_t key = measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
        MEASUREMENT_CAL_MODEL_VERSION_CURRENT, HW_RANGE_ID_1K,
        HW_EXCITATION_FREQ_1KHZ, HW_EXCITATION_AMP_100MVRMS);
    measurement_cal_curve_t curve;
    failures += expect_true(measurement_cal_curve_store_read(&store, &key, 42u, 0x12345678u,
                                                             &curve) == BSP_STATUS_OK,
                            "active record readable");
    failures += expect_true(measurement_cal_curve_store_read(&store, &key, 43u, 0x12345678u,
                                                             &curve) == BSP_STATUS_NOT_SUPPORTED,
                            "OSL change invalidates active overlay");
    flash[slot_a.start + MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES + 4u] ^= 1u;
    failures += expect_true(measurement_cal_curve_store_read(&store, &key, 42u, 0x12345678u,
                                                             &curve) == BSP_STATUS_ERROR,
                            "record corruption detected after boot");
    make_frame(&flash[slot_a.start], 7u, true);
    make_frame(&flash[slot_b.start], 8u, true);
    failures += expect_true(measurement_cal_curve_store_load(&store, fake_read, NULL,
        FLASH_BYTES, 42u, 0x12345678u, scratch, sizeof(scratch)) == BSP_STATUS_OK,
        "newer qualified B selected");
    failures += expect_true(store.slot_start == slot_b.start && store.curve_sequence == 8u,
                            "active slot is newer B");
    flash[slot_b.start + FRAME_BYTES - 4u] ^= 1u;
    failures += expect_true(measurement_cal_curve_store_load(&store, fake_read, NULL,
        FLASH_BYTES, 42u, 0x12345678u, scratch, sizeof(scratch)) == BSP_STATUS_OK,
        "interrupted B commit rolls back to A");
    failures += expect_true(store.slot_start == slot_a.start, "rollback keeps A");
    make_frame(&flash[slot_a.start], UINT32_MAX, true);
    make_frame(&flash[slot_b.start], 1u, true);
    failures += expect_true(measurement_cal_curve_store_load(&store, fake_read, NULL,
        FLASH_BYTES, 42u, 0x12345678u, scratch, sizeof(scratch)) == BSP_STATUS_OK,
        "qualified sequence wrap remains readable");
    failures += expect_true(store.slot_start == slot_b.start && store.curve_sequence == 1u,
                            "wrapped sequence selects newer B");
    make_frame(&flash[slot_a.start], 1u, true);
    make_frame(&flash[slot_b.start], UINT32_MAX, true);
    failures += expect_true(measurement_cal_curve_store_load(&store, fake_read, NULL,
        FLASH_BYTES, 42u, 0x12345678u, scratch, sizeof(scratch)) == BSP_STATUS_OK,
        "reverse wrap remains readable");
    failures += expect_true(store.slot_start == slot_a.start && store.curve_sequence == 1u,
                            "reverse wrap keeps newer A");
    return failures == 0 ? 0 : 1;
}
