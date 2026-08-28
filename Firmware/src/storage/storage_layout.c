#include "storage/storage_layout.h"

#include <stddef.h>

static bool capacity_supported(uint32_t capacity_bytes)
{
    return (capacity_bytes >= STORAGE_LAYOUT_MUTABLE_RESERVED_BYTES) &&
           ((capacity_bytes % STORAGE_LAYOUT_W25Q_SECTOR_SIZE) == 0u);
}

bool storage_layout_partition(uint32_t capacity_bytes,
                              storage_partition_id_t id,
                              storage_partition_t *partition)
{
    if ((partition == NULL) || !capacity_supported(capacity_bytes))
    {
        return false;
    }

    const uint32_t mutable_start = capacity_bytes - STORAGE_LAYOUT_MUTABLE_RESERVED_BYTES;
    const uint32_t cal_a_start = mutable_start;
    const uint32_t cal_b_start = cal_a_start + STORAGE_LAYOUT_CAL_SLOT_BYTES;
    const uint32_t settings_a_start = cal_b_start + STORAGE_LAYOUT_CAL_SLOT_BYTES;
    const uint32_t settings_b_start = settings_a_start + STORAGE_LAYOUT_SETTINGS_SLOT_BYTES;
    const uint32_t diagnostics_start = settings_b_start + STORAGE_LAYOUT_SETTINGS_SLOT_BYTES;
    const uint32_t test_start = capacity_bytes - STORAGE_LAYOUT_W25Q_SECTOR_SIZE;

    switch (id)
    {
    case STORAGE_PARTITION_RESOURCE_PACK:
        *partition = (storage_partition_t){
            .id = id,
            .start = 0u,
            .size = mutable_start,
        };
        return true;
    case STORAGE_PARTITION_CALIBRATION_A:
        *partition = (storage_partition_t){
            .id = id,
            .start = cal_a_start,
            .size = STORAGE_LAYOUT_CAL_SLOT_BYTES,
        };
        return true;
    case STORAGE_PARTITION_CALIBRATION_B:
        *partition = (storage_partition_t){
            .id = id,
            .start = cal_b_start,
            .size = STORAGE_LAYOUT_CAL_SLOT_BYTES,
        };
        return true;
    case STORAGE_PARTITION_SETTINGS_A:
        *partition = (storage_partition_t){
            .id = id,
            .start = settings_a_start,
            .size = STORAGE_LAYOUT_SETTINGS_SLOT_BYTES,
        };
        return true;
    case STORAGE_PARTITION_SETTINGS_B:
        *partition = (storage_partition_t){
            .id = id,
            .start = settings_b_start,
            .size = STORAGE_LAYOUT_SETTINGS_SLOT_BYTES,
        };
        return true;
    case STORAGE_PARTITION_DIAGNOSTICS:
        *partition = (storage_partition_t){
            .id = id,
            .start = diagnostics_start,
            .size = STORAGE_LAYOUT_DIAGNOSTICS_BYTES,
        };
        return true;
    case STORAGE_PARTITION_BRINGUP_TEST:
        *partition = (storage_partition_t){
            .id = id,
            .start = test_start,
            .size = STORAGE_LAYOUT_W25Q_SECTOR_SIZE,
        };
        return true;
    default:
        return false;
    }
}

bool storage_layout_contains(const storage_partition_t *partition,
                             uint32_t address,
                             uint32_t size)
{
    if ((partition == NULL) || (size == 0u) || (address < partition->start))
    {
        return false;
    }
    const uint32_t offset = address - partition->start;
    return (offset < partition->size) && (size <= (partition->size - offset));
}
