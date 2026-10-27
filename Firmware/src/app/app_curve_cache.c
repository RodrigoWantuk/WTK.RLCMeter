#include "app/app_curve_cache.h"

bsp_status_t app_curve_cache_load(app_curve_cache_t *cache,
                                  const measurement_cal_curve_store_t *store,
                                  uint8_t *scratch, size_t scratch_size)
{
    if (cache == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *cache = (app_curve_cache_t){0};
    if ((store == NULL) || (scratch == NULL) || (store->read == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!store->active)
    {
        return BSP_STATUS_NOT_SUPPORTED;
    }
    uint8_t count = 0u;
    for (unsigned bit = 0u; bit < MEASUREMENT_CONDITION_REV1_INDEX_COUNT; ++bit)
    {
        const uint8_t index = store->record_index[bit];
        if (index != UINT8_MAX)
        {
            if (index >= MEASUREMENT_CONDITION_REV1_MAX_SUPPORTED)
            {
                return BSP_STATUS_ERROR;
            }
            if (index >= count)
            {
                count = (uint8_t)(index + 1u);
            }
        }
    }
    const size_t size = MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES +
        (size_t)count * MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES +
        MEASUREMENT_CAL_CURVE_FRAME_TRAILER_BYTES;
    if ((count == 0u) || (size > scratch_size))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    const bsp_status_t read = store->read(store->slot_start, scratch, size, store->user);
    if (read != BSP_STATUS_OK)
    {
        return read;
    }
    measurement_cal_curve_frame_info_t info;
    if (!measurement_cal_curve_frame_validate(scratch, size, store->osl_sequence,
            store->osl_frame_crc32, true, &info) ||
        (info.curve_sequence != store->curve_sequence) || (info.record_count != count))
    {
        return BSP_STATUS_ERROR;
    }
    for (unsigned bit = 0u; bit < MEASUREMENT_CONDITION_REV1_INDEX_COUNT; ++bit)
    {
        const uint8_t index = store->record_index[bit];
        if (index == UINT8_MAX)
        {
            continue;
        }
        const uint8_t *record = scratch + MEASUREMENT_CAL_CURVE_FRAME_HEADER_BYTES +
            (size_t)index * MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES;
        const measurement_cal_key_t key = measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
            MEASUREMENT_CAL_MODEL_VERSION_CURRENT, (hw_range_id_t)record[0],
            (hw_excitation_freq_t)record[1], (hw_excitation_amp_t)record[2]);
        uint8_t actual_bit;
        if (!measurement_cal_rev1_condition_bit(&key, &actual_bit) || (actual_bit != bit) ||
            !measurement_cal_curve_record_decode(record, MEASUREMENT_CAL_CURVE_FRAME_RECORD_BYTES,
                                                  &key, &cache->curves[bit]))
        {
            return BSP_STATUS_ERROR;
        }
        cache->present[bit] = 1u;
    }
    cache->osl_sequence = store->osl_sequence;
    cache->osl_crc32 = store->osl_frame_crc32;
    cache->curve_sequence = store->curve_sequence;
    cache->ready = true;
    return BSP_STATUS_OK;
}

bsp_status_t app_curve_cache_get(const app_curve_cache_t *cache,
                                 const measurement_cal_key_t *key,
                                 uint32_t osl_sequence, uint32_t osl_crc32,
                                 measurement_cal_curve_t *curve)
{
    if ((cache == NULL) || (key == NULL) || (curve == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!cache->ready)
    {
        return BSP_STATUS_ERROR;
    }
    uint8_t bit;
    if ((cache->osl_sequence != osl_sequence) || (cache->osl_crc32 != osl_crc32) ||
        !measurement_cal_rev1_condition_bit(key, &bit) ||
        (bit >= MEASUREMENT_CONDITION_REV1_INDEX_COUNT) || (cache->present[bit] == 0u))
    {
        return BSP_STATUS_NOT_SUPPORTED;
    }
    *curve = cache->curves[bit];
    return BSP_STATUS_OK;
}
