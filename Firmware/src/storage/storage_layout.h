#ifndef WTK_STORAGE_LAYOUT_H
#define WTK_STORAGE_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

enum
{
    STORAGE_LAYOUT_W25Q_SECTOR_SIZE = 4096u,
    STORAGE_LAYOUT_CAL_SLOT_BYTES = STORAGE_LAYOUT_W25Q_SECTOR_SIZE,
    STORAGE_LAYOUT_CAL_SLOT_COUNT = 2u,
    STORAGE_LAYOUT_SETTINGS_SLOT_BYTES = STORAGE_LAYOUT_W25Q_SECTOR_SIZE,
    STORAGE_LAYOUT_SETTINGS_SLOT_COUNT = 2u,
    STORAGE_LAYOUT_SETTINGS_BYTES =
        STORAGE_LAYOUT_SETTINGS_SLOT_COUNT * STORAGE_LAYOUT_SETTINGS_SLOT_BYTES,
    STORAGE_LAYOUT_DIAGNOSTICS_BYTES = 3u * STORAGE_LAYOUT_W25Q_SECTOR_SIZE,
    STORAGE_LAYOUT_MUTABLE_RESERVED_BYTES =
        (STORAGE_LAYOUT_CAL_SLOT_COUNT * STORAGE_LAYOUT_CAL_SLOT_BYTES) +
        STORAGE_LAYOUT_SETTINGS_BYTES +
        STORAGE_LAYOUT_DIAGNOSTICS_BYTES +
        STORAGE_LAYOUT_W25Q_SECTOR_SIZE,
};

typedef enum
{
    STORAGE_PARTITION_RESOURCE_PACK = 0,
    STORAGE_PARTITION_CALIBRATION_A,
    STORAGE_PARTITION_CALIBRATION_B,
    STORAGE_PARTITION_SETTINGS_A,
    STORAGE_PARTITION_SETTINGS_B,
    STORAGE_PARTITION_DIAGNOSTICS,
    STORAGE_PARTITION_BRINGUP_TEST,
} storage_partition_id_t;

typedef struct
{
    storage_partition_id_t id;
    uint32_t start;
    uint32_t size;
} storage_partition_t;

bool storage_layout_partition(uint32_t capacity_bytes,
                              storage_partition_id_t id,
                              storage_partition_t *partition);
bool storage_layout_contains(const storage_partition_t *partition,
                             uint32_t address,
                             uint32_t size);

#endif
