#ifndef WTK_MEASUREMENT_CALIBRATION_STORE_H
#define WTK_MEASUREMENT_CALIBRATION_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "measurement/measurement_calibration.h"
#include "storage/storage_layout.h"

typedef enum
{
    MEASUREMENT_CAL_STORE_SLOT_A = 0,
    MEASUREMENT_CAL_STORE_SLOT_B = 1,
} measurement_cal_store_slot_t;

typedef enum
{
    MEASUREMENT_CAL_STORE_IDLE = 0,
    MEASUREMENT_CAL_STORE_ERASE_START,
    MEASUREMENT_CAL_STORE_ERASE_WAIT,
    MEASUREMENT_CAL_STORE_PROGRAM_HEADER_START,
    MEASUREMENT_CAL_STORE_PROGRAM_HEADER_WAIT,
    MEASUREMENT_CAL_STORE_PROGRAM_PAYLOAD_START,
    MEASUREMENT_CAL_STORE_PROGRAM_PAYLOAD_WAIT,
    MEASUREMENT_CAL_STORE_PROGRAM_COMMIT_START,
    MEASUREMENT_CAL_STORE_PROGRAM_COMMIT_WAIT,
    MEASUREMENT_CAL_STORE_VERIFY,
    MEASUREMENT_CAL_STORE_DONE,
    MEASUREMENT_CAL_STORE_ERROR,
} measurement_cal_store_state_t;

typedef struct
{
    bsp_status_t (*read)(uint32_t address, void *dst, size_t size, void *user);
    bsp_status_t (*erase_sector_start)(uint32_t address, uint32_t now_ms, void *user);
    bsp_status_t (*program_start)(uint32_t address, const void *src, size_t size, uint32_t now_ms, void *user);
    bsp_status_t (*poll)(uint32_t now_ms, void *user);
    void *user;
} measurement_cal_store_io_t;

typedef struct
{
    bool frame_valid;
    measurement_cal_store_slot_t slot;
    measurement_cal_frame_info_t frame;
    measurement_cal_validity_t validity;
    uint8_t record_count;
} measurement_cal_store_slot_info_t;

typedef struct
{
    measurement_cal_store_io_t io;
    storage_partition_t slots[2];
    uint8_t *image;
    size_t image_capacity;
    size_t image_size;
    size_t program_offset;
    size_t current_chunk;
    measurement_cal_set_t scan_set;
    measurement_cal_validity_t staged_validity;
    uint32_t expected_sequence;
    uint32_t expected_hardware_revision;
    uint16_t expected_model_version;
    uint8_t expected_record_count;
    uint64_t expected_key_mask;
    measurement_cal_store_slot_t target_slot;
    measurement_cal_store_state_t state;
    bsp_status_t last_status;
} measurement_cal_store_t;

bsp_status_t measurement_cal_store_init(measurement_cal_store_t *store,
                                        const measurement_cal_store_io_t *io,
                                        uint32_t capacity_bytes,
                                        uint8_t *image_scratch,
                                        size_t image_scratch_size);
bsp_status_t measurement_cal_store_load_newest(measurement_cal_store_t *store,
                                               measurement_cal_set_t *set,
                                               measurement_cal_store_slot_t *slot);
bsp_status_t measurement_cal_store_load_newest_usable(
    measurement_cal_store_t *store,
    const measurement_cal_requirements_t *requirements,
    uint32_t hardware_revision,
    uint16_t model_version,
    measurement_cal_set_t *set,
    measurement_cal_store_slot_t *slot,
    measurement_cal_store_slot_info_t diagnostics[2]);
bsp_status_t measurement_cal_store_write_start(measurement_cal_store_t *store,
                                               const measurement_cal_set_t *candidate,
                                               const measurement_cal_requirements_t *requirements);
bsp_status_t measurement_cal_store_step(measurement_cal_store_t *store, uint32_t now_ms);
bsp_status_t measurement_cal_store_acknowledge(measurement_cal_store_t *store);
bsp_status_t measurement_cal_store_refresh_diagnostics(
    measurement_cal_store_t *store,
    const measurement_cal_requirements_t *requirements,
    uint32_t hardware_revision,
    uint16_t model_version,
    measurement_cal_store_slot_info_t diagnostics[2]);
measurement_cal_store_slot_t measurement_cal_store_target_slot(const measurement_cal_store_t *store);
measurement_cal_store_state_t measurement_cal_store_state(const measurement_cal_store_t *store);
uint32_t measurement_cal_store_context_size_bytes(void);
bool measurement_cal_store_sequence_newer(uint32_t a, uint32_t b);

#endif
