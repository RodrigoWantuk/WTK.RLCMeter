#include "measurement/measurement_cal_curve_store.h"

#include <string.h>

#include "storage/storage_layout.h"


static uint16_t read_u16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8u));
}

static bool sequence_newer(uint32_t candidate, uint32_t active)
{
    return candidate != active && (uint32_t)(candidate - active) < UINT32_C(0x80000000);
}

bsp_status_t measurement_cal_curve_store_load(measurement_cal_curve_store_t *store,
                                              measurement_cal_curve_read_fn read,
                                              void *user,
                                              uint32_t capacity_bytes,
                                              uint32_t osl_sequence,
                                              uint32_t osl_frame_crc32,
                                              uint8_t *scratch,
                                              size_t scratch_size)
{
    if ((store == NULL) || (read == NULL) || (scratch == NULL) ||
        (scratch_size < MEASUREMENT_CAL_CURVE_FRAME_MAX_BYTES) || (osl_sequence == 0u))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *store = (measurement_cal_curve_store_t){.read = read, .user = user};
    (void)memset(store->record_index, UINT8_MAX, sizeof(store->record_index));
    const storage_partition_id_t ids[] = {
        STORAGE_PARTITION_CAL_CURVE_A, STORAGE_PARTITION_CAL_CURVE_B,
    };
    for (uint8_t slot = 0u; slot < 2u; slot++)
    {
        storage_partition_t partition;
        if (!storage_layout_partition(capacity_bytes, ids[slot], &partition))
        {
            return BSP_STATUS_INVALID_ARG;
        }
        if (read(partition.start, scratch,
                 MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES, user) != BSP_STATUS_OK)
        {
            continue;
        }
        const uint16_t count = read_u16(&scratch[6]);
        const size_t size = MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES +
                            (size_t)count * MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES +
                            MEASUREMENT_CAL_CURVE_FRAME_TRAILER_BYTES;
        if ((count == 0u) || (count > MEASUREMENT_CONDITION_REV1_MAX_SUPPORTED) ||
            (size > scratch_size) || (size > partition.size) ||
            (read(partition.start, scratch, size, user) != BSP_STATUS_OK))
        {
            continue;
        }
        measurement_cal_curve_frame_info_t info;
        if (!measurement_cal_curve_frame_validate(scratch, size, osl_sequence,
                                                   osl_frame_crc32, true, &info) ||
            (info.curve_sequence == 0u) ||
            (store->active && !sequence_newer(info.curve_sequence, store->curve_sequence)))
        {
            continue;
        }
        (void)memset(store->record_index, UINT8_MAX, sizeof(store->record_index));
        for (uint8_t i = 0u; i < info.record_count; i++)
        {
            const uint8_t *record = &scratch[MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES +
                                             (size_t)i * MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES];
            const measurement_cal_key_t key = measurement_cal_key(
                MEASUREMENT_CAL_HARDWARE_REV1, MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                (hw_range_id_t)record[0], (hw_excitation_freq_t)record[1],
                (hw_excitation_amp_t)record[2]);
            uint8_t bit;
            if (measurement_cal_rev1_condition_bit(&key, &bit))
            {
                store->record_index[bit] = i;
            }
        }
        store->slot_start = partition.start;
        store->osl_sequence = osl_sequence;
        store->osl_frame_crc32 = osl_frame_crc32;
        store->curve_sequence = info.curve_sequence;
        store->active = true;
    }
    return store->active ? BSP_STATUS_OK : BSP_STATUS_NOT_SUPPORTED;
}

bsp_status_t measurement_cal_curve_store_read(const measurement_cal_curve_store_t *store,
                                              const measurement_cal_key_t *key,
                                              uint32_t active_osl_sequence,
                                              uint32_t active_osl_frame_crc32,
                                              measurement_cal_curve_t *curve)
{
    if ((store == NULL) || (key == NULL) || (curve == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    uint8_t bit;
    if (!store->active || (active_osl_sequence != store->osl_sequence) ||
        (active_osl_frame_crc32 != store->osl_frame_crc32) ||
        !measurement_cal_rev1_condition_bit(key, &bit) ||
        (store->record_index[bit] == UINT8_MAX))
    {
        return BSP_STATUS_NOT_SUPPORTED;
    }
    uint8_t record[MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES];
    const uint32_t address = store->slot_start + MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES +
        (uint32_t)store->record_index[bit] * MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES;
    const bsp_status_t status = store->read(address, record, sizeof(record), store->user);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    return measurement_cal_curve_record_decode(record, sizeof(record), key, curve) ?
               BSP_STATUS_OK : BSP_STATUS_ERROR;
}
