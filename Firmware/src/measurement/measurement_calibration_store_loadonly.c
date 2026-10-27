/* Factory PRODUCT reads immutable OSL; no candidate copy or write machinery. */
#include "measurement/measurement_calibration_store.h"

bool measurement_cal_store_sequence_newer(uint32_t a, uint32_t b)
{
    return a != b && (uint32_t)(a - b) < 0x80000000u;
}

bsp_status_t measurement_cal_store_init(measurement_cal_store_t *store,
    const measurement_cal_store_io_t *io, uint32_t capacity, uint8_t *scratch, size_t bytes)
{
    if (store == NULL || io == NULL || io->read == NULL || scratch == NULL ||
        bytes < MEASUREMENT_CAL_MAX_FRAME_BYTES) return BSP_STATUS_INVALID_ARG;
    *store = (measurement_cal_store_t){.io = *io, .image = scratch, .image_capacity = bytes};
    if (!storage_layout_partition(capacity, STORAGE_PARTITION_CALIBRATION_A, &store->slots[0]) ||
        !storage_layout_partition(capacity, STORAGE_PARTITION_CALIBRATION_B, &store->slots[1]))
        return BSP_STATUS_NOT_SUPPORTED;
    return BSP_STATUS_OK;
}

static bool read_candidate(measurement_cal_store_t *store, unsigned slot,
    measurement_cal_set_t *set, measurement_cal_store_slot_info_t *info,
    const measurement_cal_requirements_t *requirements, uint32_t hardware,
    uint16_t model, bool full)
{
    *info = (measurement_cal_store_slot_info_t){.slot = (measurement_cal_store_slot_t)slot,
        .validity = {.status = MEASUREMENT_CAL_VALIDITY_MISSING, .flags = MEASUREMENT_CAL_VALID_FLAG_MISSING}};
    if (store->io.read(store->slots[slot].start, store->image,
                      MEASUREMENT_CAL_MAX_FRAME_BYTES, store->io.user) != BSP_STATUS_OK) return false;
    info->validity = measurement_cal_inspect_frame(store->image, MEASUREMENT_CAL_MAX_FRAME_BYTES,
                                                   hardware, model, &info->frame);
    info->frame_valid = info->validity.status == MEASUREMENT_CAL_VALIDITY_VALID;
    if (!info->frame_valid) return false;
    if (!measurement_cal_decode_set(store->image, MEASUREMENT_CAL_MAX_FRAME_BYTES, set, &info->frame))
    {
        info->frame_valid = false;
        info->validity = (measurement_cal_validity_t){.status = MEASUREMENT_CAL_VALIDITY_CORRUPT,
            .flags = MEASUREMENT_CAL_VALID_FLAG_CORRUPT};
        return false;
    }
    info->record_count = set->record_count;
    info->validity = full ? measurement_cal_validate_rev1_full_set(set) :
        measurement_cal_validate_set(set, requirements, hardware, model);
    return info->validity.status == MEASUREMENT_CAL_VALIDITY_VALID;
}

static bsp_status_t load(measurement_cal_store_t *store, measurement_cal_set_t *set,
    measurement_cal_store_slot_t *slot, measurement_cal_store_slot_info_t diagnostics[2],
    const measurement_cal_requirements_t *requirements, uint32_t hardware, uint16_t model, bool full)
{
    if (store == NULL || set == NULL || store->io.read == NULL) return BSP_STATUS_INVALID_ARG;
    bool found = false;
    unsigned best = 0u;
    uint32_t sequence = 0u;
    uint32_t crc = 0u;
    for (unsigned i = 0u; i < 2u; ++i)
    {
        measurement_cal_store_slot_info_t info;
        const bool valid = read_candidate(store, i, set, &info, requirements, hardware, model, full);
        if (diagnostics != NULL) diagnostics[i] = info;
        if (valid && (!found || measurement_cal_store_sequence_newer(set->sequence, sequence)))
        {
            found = true;
            best = i;
            sequence = set->sequence;
            crc = info.frame.crc32;
        }
    }
    if (!found) return BSP_STATUS_ERROR;
    /* One additional read replaces the otherwise permanent 2,576-B scan copy.
     * No active acquisition may use the output while boot/reload owns it. */
    measurement_cal_store_slot_info_t verified;
    if (!read_candidate(store, best, set, &verified, requirements, hardware, model, full) ||
        set->sequence != sequence || verified.frame.crc32 != crc) return BSP_STATUS_ERROR;
    if (slot != NULL) *slot = (measurement_cal_store_slot_t)best;
    return BSP_STATUS_OK;
}

bsp_status_t measurement_cal_store_load_full_usable(measurement_cal_store_t *store,
    measurement_cal_set_t *set, measurement_cal_store_slot_t *slot,
    measurement_cal_store_slot_info_t diagnostics[2])
{
    return load(store, set, slot, diagnostics, NULL, MEASUREMENT_CAL_HARDWARE_REV1,
                MEASUREMENT_CAL_MODEL_VERSION_CURRENT, true);
}
bsp_status_t measurement_cal_store_load_newest_usable(measurement_cal_store_t *store,
    const measurement_cal_requirements_t *requirements, uint32_t hardware, uint16_t model,
    measurement_cal_set_t *set, measurement_cal_store_slot_t *slot,
    measurement_cal_store_slot_info_t diagnostics[2])
{
    return load(store, set, slot, diagnostics, requirements, hardware, model, false);
}
bsp_status_t measurement_cal_store_load_newest(measurement_cal_store_t *store,
    measurement_cal_set_t *set, measurement_cal_store_slot_t *slot)
{
    return load(store, set, slot, NULL, NULL, MEASUREMENT_CAL_HARDWARE_REV1,
                MEASUREMENT_CAL_MODEL_VERSION_CURRENT, false);
}
measurement_cal_store_state_t measurement_cal_store_state(const measurement_cal_store_t *store)
{
    return store == NULL ? MEASUREMENT_CAL_STORE_ERROR : store->state;
}
uint32_t measurement_cal_store_context_size_bytes(void)
{
    return sizeof(measurement_cal_store_t);
}
