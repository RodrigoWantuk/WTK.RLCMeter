#include "measurement/measurement_calibration_store.h"

#include <string.h>

enum
{
    COMMIT_OFFSET = 60u,
    COMMIT_BYTES = 4u,
    PAGE_BYTES = 256u,
    KEY_BITS_PER_RANGE = 6u,
    KEY_BITS_PER_FREQ = 2u,
    MEASUREMENT_CAL_STORE_CONTEXT_BUDGET_BYTES = 6400u,
};

_Static_assert(sizeof(measurement_cal_set_t) <= MEASUREMENT_CAL_MAX_FRAME_BYTES,
               "decoded calibration set must fit one persisted frame");
_Static_assert(sizeof(measurement_cal_store_t) <= MEASUREMENT_CAL_STORE_CONTEXT_BUDGET_BYTES,
               "calibration store scratch exceeded SRAM budget");

static bool io_valid(const measurement_cal_store_io_t *io)
{
    return (io != NULL) &&
           (io->read != NULL) &&
           (io->erase_sector_start != NULL) &&
           (io->program_start != NULL) &&
           (io->poll != NULL);
}

static bool read_slot_info(measurement_cal_store_t *store,
                           measurement_cal_store_slot_t slot,
                           measurement_cal_set_t *set,
                           measurement_cal_store_slot_info_t *info,
                           const measurement_cal_requirements_t *requirements,
                           uint32_t hardware_revision,
                           uint16_t model_version);

bool measurement_cal_store_sequence_newer(uint32_t a, uint32_t b)
{
    return (a != b) && ((uint32_t)(a - b) < 0x80000000u);
}

static bool key_bit(const measurement_cal_key_t *key, uint8_t *bit)
{
    if ((key == NULL) || (bit == NULL) ||
        (key->hardware_revision != MEASUREMENT_CAL_HARDWARE_REV1) ||
        (key->model_version != MEASUREMENT_CAL_MODEL_VERSION_CURRENT) ||
        !measurement_cal_condition_allowed(key->range_id, key->frequency, key->amplitude))
    {
        return false;
    }
    if ((key->range_id > HW_RANGE_ID_1M) ||
        (key->frequency > HW_EXCITATION_FREQ_10KHZ) ||
        (key->amplitude > HW_EXCITATION_AMP_500MVRMS))
    {
        return false;
    }
    *bit = (uint8_t)(((uint8_t)key->range_id * KEY_BITS_PER_RANGE) +
                     ((uint8_t)key->frequency * KEY_BITS_PER_FREQ) +
                     (uint8_t)key->amplitude);
    return *bit < 64u;
}

static bool set_key_mask(const measurement_cal_set_t *set, uint64_t *mask)
{
    if ((set == NULL) || (mask == NULL) || (set->record_count > MEASUREMENT_CAL_MAX_RECORDS))
    {
        return false;
    }
    uint64_t built = 0u;
    for (uint8_t i = 0u; i < set->record_count; i++)
    {
        uint8_t bit = 0u;
        if (!key_bit(&set->records[i].key, &bit))
        {
            return false;
        }
        const uint64_t flag = (uint64_t)1u << bit;
        if ((built & flag) != 0u)
        {
            return false;
        }
        built |= flag;
    }
    *mask = built;
    return true;
}

static bool set_matches_expected_mask(const measurement_cal_set_t *set, uint64_t expected_mask)
{
    uint64_t actual_mask = 0u;
    return set_key_mask(set, &actual_mask) && (actual_mask == expected_mask);
}

static bool find_newest_decoded_slot(measurement_cal_store_t *store,
                                     const measurement_cal_requirements_t *requirements,
                                     measurement_cal_store_slot_t *slot,
                                     uint32_t *sequence)
{
    bool have_best = false;
    uint32_t best_sequence = 0u;
    measurement_cal_store_slot_t best_slot = MEASUREMENT_CAL_STORE_SLOT_A;
    measurement_cal_set_t *candidate = &store->scan_set;

    for (uint8_t i = 0u; i < 2u; i++)
    {
        const measurement_cal_store_slot_t current_slot = (measurement_cal_store_slot_t)i;
        const bool decoded = read_slot_info(store,
                                            current_slot,
                                            candidate,
                                            NULL,
                                            requirements,
                                            MEASUREMENT_CAL_HARDWARE_REV1,
                                            MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
        const measurement_cal_validity_t validity =
            measurement_cal_validate_set(decoded ? candidate : NULL,
                                         requirements,
                                         MEASUREMENT_CAL_HARDWARE_REV1,
                                         MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
        if (decoded && (validity.status == MEASUREMENT_CAL_VALIDITY_VALID) &&
            (!have_best || measurement_cal_store_sequence_newer(candidate->sequence, best_sequence)))
        {
            best_sequence = candidate->sequence;
            best_slot = current_slot;
            have_best = true;
        }
    }

    if (!have_best)
    {
        return false;
    }
    if (slot != NULL)
    {
        *slot = best_slot;
    }
    if (sequence != NULL)
    {
        *sequence = best_sequence;
    }
    return true;
}

bsp_status_t measurement_cal_store_init(measurement_cal_store_t *store,
                                        const measurement_cal_store_io_t *io,
                                        uint32_t capacity_bytes)
{
    if ((store == NULL) || !io_valid(io))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *store = (measurement_cal_store_t){0};
    store->io = *io;
    if (!storage_layout_partition(capacity_bytes, STORAGE_PARTITION_CALIBRATION_A, &store->slots[0]) ||
        !storage_layout_partition(capacity_bytes, STORAGE_PARTITION_CALIBRATION_B, &store->slots[1]))
    {
        store->state = MEASUREMENT_CAL_STORE_ERROR;
        store->last_status = BSP_STATUS_NOT_SUPPORTED;
        return BSP_STATUS_NOT_SUPPORTED;
    }
    store->state = MEASUREMENT_CAL_STORE_IDLE;
    store->last_status = BSP_STATUS_OK;
    return BSP_STATUS_OK;
}

static bool read_slot_info(measurement_cal_store_t *store,
                           measurement_cal_store_slot_t slot,
                           measurement_cal_set_t *set,
                           measurement_cal_store_slot_info_t *info,
                           const measurement_cal_requirements_t *requirements,
                           uint32_t hardware_revision,
                           uint16_t model_version)
{
    if ((store == NULL) || (set == NULL))
    {
        return false;
    }
    const storage_partition_t *partition = &store->slots[(uint8_t)slot];
    if (store->io.read(partition->start,
                       store->image,
                       MEASUREMENT_CAL_MAX_FRAME_BYTES,
                       store->io.user) != BSP_STATUS_OK)
    {
        if (info != NULL)
        {
            *info = (measurement_cal_store_slot_info_t){
                .frame_valid = false,
                .slot = slot,
                .record_count = 0u,
                .validity = {.status = MEASUREMENT_CAL_VALIDITY_MISSING,
                             .flags = MEASUREMENT_CAL_VALID_FLAG_MISSING},
            };
        }
        return false;
    }

    measurement_cal_frame_info_t frame = {0};
    measurement_cal_validity_t validity =
        measurement_cal_inspect_frame(store->image,
                                      MEASUREMENT_CAL_MAX_FRAME_BYTES,
                                      hardware_revision,
                                      model_version,
                                      &frame);
    const bool structurally_valid =
        (validity.status == MEASUREMENT_CAL_VALIDITY_VALID) ||
        (validity.status == MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_SCHEMA) ||
        (validity.status == MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_HARDWARE) ||
        (validity.status == MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_MODEL);
    bool decoded = false;
    if (validity.status == MEASUREMENT_CAL_VALIDITY_VALID)
    {
        decoded = measurement_cal_decode_set(store->image,
                                             MEASUREMENT_CAL_MAX_FRAME_BYTES,
                                             set,
                                             &frame);
        if (decoded)
        {
            validity = measurement_cal_validate_set(set, requirements, hardware_revision, model_version);
        }
        else
        {
            validity.status = MEASUREMENT_CAL_VALIDITY_CORRUPT;
            validity.flags = MEASUREMENT_CAL_VALID_FLAG_CORRUPT;
        }
    }
    if (info != NULL)
    {
        *info = (measurement_cal_store_slot_info_t){
            .frame_valid = structurally_valid && (validity.status != MEASUREMENT_CAL_VALIDITY_CORRUPT),
            .slot = slot,
            .frame = frame,
            .validity = validity,
            .record_count = decoded ? set->record_count : 0u,
        };
    }
    return decoded;
}

bsp_status_t measurement_cal_store_load_newest(measurement_cal_store_t *store,
                                               measurement_cal_set_t *set,
                                               measurement_cal_store_slot_t *slot)
{
    if ((store == NULL) || (set == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    measurement_cal_store_slot_info_t info_a;
    measurement_cal_store_slot_info_t info_b;
    const bool valid_a = read_slot_info(store,
                                        MEASUREMENT_CAL_STORE_SLOT_A,
                                        set,
                                        &info_a,
                                        NULL,
                                        MEASUREMENT_CAL_HARDWARE_REV1,
                                        MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
    measurement_cal_set_t *scratch = &store->scan_set;
    const bool valid_b = read_slot_info(store,
                                        MEASUREMENT_CAL_STORE_SLOT_B,
                                        scratch,
                                        &info_b,
                                        NULL,
                                        MEASUREMENT_CAL_HARDWARE_REV1,
                                        MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
    if (!valid_a && !valid_b)
    {
        return BSP_STATUS_ERROR;
    }
    if (valid_a && (!valid_b || measurement_cal_store_sequence_newer(set->sequence, scratch->sequence)))
    {
        if (slot != NULL)
        {
            *slot = MEASUREMENT_CAL_STORE_SLOT_A;
        }
        return BSP_STATUS_OK;
    }
    *set = *scratch;
    if (slot != NULL)
    {
        *slot = MEASUREMENT_CAL_STORE_SLOT_B;
    }
    return BSP_STATUS_OK;
}

bsp_status_t measurement_cal_store_load_newest_usable(
    measurement_cal_store_t *store,
    const measurement_cal_requirements_t *requirements,
    uint32_t hardware_revision,
    uint16_t model_version,
    measurement_cal_set_t *set,
    measurement_cal_store_slot_t *slot,
    measurement_cal_store_slot_info_t diagnostics[2])
{
    if ((store == NULL) || (set == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    measurement_cal_set_t *candidate = &store->scan_set;
    bool have_best = false;
    measurement_cal_store_slot_t best_slot = MEASUREMENT_CAL_STORE_SLOT_A;

    for (uint8_t i = 0u; i < 2u; i++)
    {
        const measurement_cal_store_slot_t current_slot = (measurement_cal_store_slot_t)i;
        const bool decoded = read_slot_info(store,
                                            current_slot,
                                            candidate,
                                            (diagnostics != NULL) ? &diagnostics[i] : NULL,
                                            requirements,
                                            hardware_revision,
                                            model_version);
        const measurement_cal_validity_t validity = (diagnostics != NULL) ?
            diagnostics[i].validity :
            measurement_cal_validate_set(decoded ? candidate : NULL,
                                         requirements,
                                         hardware_revision,
                                         model_version);
        if (decoded && (validity.status == MEASUREMENT_CAL_VALIDITY_VALID) &&
            (!have_best || measurement_cal_store_sequence_newer(candidate->sequence, set->sequence)))
        {
            *set = *candidate;
            best_slot = current_slot;
            have_best = true;
        }
    }

    if (!have_best)
    {
        return BSP_STATUS_ERROR;
    }
    if (slot != NULL)
    {
        *slot = best_slot;
    }
    return BSP_STATUS_OK;
}

bsp_status_t measurement_cal_store_write_start(measurement_cal_store_t *store,
                                               const measurement_cal_set_t *candidate,
                                               const measurement_cal_requirements_t *requirements)
{
    if ((store == NULL) || (candidate == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if ((store->state != MEASUREMENT_CAL_STORE_IDLE) &&
        (store->state != MEASUREMENT_CAL_STORE_DONE) &&
        (store->state != MEASUREMENT_CAL_STORE_ERROR))
    {
        return BSP_STATUS_BUSY;
    }

    measurement_cal_store_slot_t newest_slot = MEASUREMENT_CAL_STORE_SLOT_B;
    measurement_cal_store_slot_t target_slot = MEASUREMENT_CAL_STORE_SLOT_A;
    uint32_t next_sequence = candidate->sequence;
    uint32_t newest_sequence = 0u;
    if (find_newest_decoded_slot(store, NULL, NULL, &newest_sequence))
    {
        next_sequence = newest_sequence + 1u;
    }
    else if (next_sequence == 0u)
    {
        next_sequence = 1u;
    }
    if (find_newest_decoded_slot(store, requirements, &newest_slot, NULL))
    {
        target_slot = (newest_slot == MEASUREMENT_CAL_STORE_SLOT_A) ? MEASUREMENT_CAL_STORE_SLOT_B :
                                                                      MEASUREMENT_CAL_STORE_SLOT_A;
    }

    size_t written = 0u;
    if (!measurement_cal_serialize_set_with_header(candidate,
                                                   MEASUREMENT_CAL_SCHEMA_VERSION,
                                                   MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                                   next_sequence,
                                                   store->image,
                                                   sizeof(store->image),
                                                   &written))
    {
        store->state = MEASUREMENT_CAL_STORE_ERROR;
        store->last_status = BSP_STATUS_INVALID_ARG;
        return BSP_STATUS_INVALID_ARG;
    }

    uint64_t expected_key_mask = 0u;
    if (!measurement_cal_decode_set(store->image, written, &store->scan_set, NULL) ||
        !set_key_mask(&store->scan_set, &expected_key_mask) ||
        (store->scan_set.sequence != next_sequence) ||
        (store->scan_set.record_count != candidate->record_count) ||
        (measurement_cal_validate_set(&store->scan_set,
                                      requirements,
                                      MEASUREMENT_CAL_HARDWARE_REV1,
                                      MEASUREMENT_CAL_MODEL_VERSION_CURRENT)
             .status != MEASUREMENT_CAL_VALIDITY_VALID))
    {
        store->state = MEASUREMENT_CAL_STORE_ERROR;
        store->last_status = BSP_STATUS_ERROR;
        return BSP_STATUS_ERROR;
    }

    store->staged_validity = measurement_cal_validate_set(&store->scan_set,
                                                          requirements,
                                                          MEASUREMENT_CAL_HARDWARE_REV1,
                                                          MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
    store->expected_sequence = store->scan_set.sequence;
    store->expected_hardware_revision = store->scan_set.hardware_revision;
    store->expected_model_version = store->scan_set.model_version;
    store->expected_record_count = store->scan_set.record_count;
    store->expected_key_mask = expected_key_mask;
    store->image_size = written;
    store->program_offset = 0u;
    store->current_chunk = 0u;
    store->target_slot = target_slot;
    store->state = MEASUREMENT_CAL_STORE_ERASE_START;
    store->last_status = BSP_STATUS_BUSY;
    return BSP_STATUS_BUSY;
}

static size_t page_span(uint32_t address, size_t remaining)
{
    const uint32_t page_offset = address % PAGE_BYTES;
    const size_t page_remaining = (size_t)(PAGE_BYTES - page_offset);
    return (remaining < page_remaining) ? remaining : page_remaining;
}

static void prepare_program_chunk(measurement_cal_store_t *store, size_t begin, size_t end)
{
    if (store->program_offset < begin)
    {
        store->program_offset = begin;
    }
    if (store->program_offset < end)
    {
        const uint32_t address = store->slots[(uint8_t)store->target_slot].start +
                                 (uint32_t)store->program_offset;
        store->current_chunk = page_span(address, end - store->program_offset);
    }
    else
    {
        store->current_chunk = 0u;
    }
}

static bsp_status_t start_program_chunk(measurement_cal_store_t *store,
                                        size_t begin,
                                        size_t end,
                                        measurement_cal_store_state_t wait_state,
                                        measurement_cal_store_state_t next_state,
                                        uint32_t now_ms)
{
    prepare_program_chunk(store, begin, end);
    if (store->current_chunk == 0u)
    {
        store->state = next_state;
        return BSP_STATUS_BUSY;
    }
    const uint32_t address = store->slots[(uint8_t)store->target_slot].start +
                             (uint32_t)store->program_offset;
    const bsp_status_t status =
        store->io.program_start(address,
                                &store->image[store->program_offset],
                                store->current_chunk,
                                now_ms,
                                store->io.user);
    if ((status != BSP_STATUS_BUSY) && (status != BSP_STATUS_OK))
    {
        store->state = MEASUREMENT_CAL_STORE_ERROR;
        store->last_status = status;
        return status;
    }
    store->state = wait_state;
    return BSP_STATUS_BUSY;
}

static bsp_status_t wait_program_chunk(measurement_cal_store_t *store,
                                       size_t end,
                                       measurement_cal_store_state_t start_state,
                                       measurement_cal_store_state_t next_state,
                                       uint32_t now_ms)
{
    const bsp_status_t status = store->io.poll(now_ms, store->io.user);
    if (status == BSP_STATUS_BUSY)
    {
        return BSP_STATUS_BUSY;
    }
    if (status != BSP_STATUS_OK)
    {
        store->state = MEASUREMENT_CAL_STORE_ERROR;
        store->last_status = status;
        return status;
    }
    store->program_offset += store->current_chunk;
    store->current_chunk = 0u;
    store->state = (store->program_offset >= end) ? next_state : start_state;
    return BSP_STATUS_BUSY;
}

bsp_status_t measurement_cal_store_step(measurement_cal_store_t *store, uint32_t now_ms)
{
    if (store == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    switch (store->state)
    {
    case MEASUREMENT_CAL_STORE_IDLE:
    case MEASUREMENT_CAL_STORE_DONE:
        return BSP_STATUS_OK;
    case MEASUREMENT_CAL_STORE_ERROR:
        return store->last_status;
    case MEASUREMENT_CAL_STORE_ERASE_START:
    {
        const bsp_status_t status =
            store->io.erase_sector_start(store->slots[(uint8_t)store->target_slot].start,
                                         now_ms,
                                         store->io.user);
        if ((status != BSP_STATUS_BUSY) && (status != BSP_STATUS_OK))
        {
            store->state = MEASUREMENT_CAL_STORE_ERROR;
            store->last_status = status;
            return status;
        }
        store->state = MEASUREMENT_CAL_STORE_ERASE_WAIT;
        return BSP_STATUS_BUSY;
    }
    case MEASUREMENT_CAL_STORE_ERASE_WAIT:
    {
        const bsp_status_t status = store->io.poll(now_ms, store->io.user);
        if (status == BSP_STATUS_BUSY)
        {
            return BSP_STATUS_BUSY;
        }
        if (status != BSP_STATUS_OK)
        {
            store->state = MEASUREMENT_CAL_STORE_ERROR;
            store->last_status = status;
            return status;
        }
        store->program_offset = 0u;
        store->state = MEASUREMENT_CAL_STORE_PROGRAM_HEADER_START;
        return BSP_STATUS_BUSY;
    }
    case MEASUREMENT_CAL_STORE_PROGRAM_HEADER_START:
        return start_program_chunk(store,
                                   0u,
                                   COMMIT_OFFSET,
                                   MEASUREMENT_CAL_STORE_PROGRAM_HEADER_WAIT,
                                   MEASUREMENT_CAL_STORE_PROGRAM_PAYLOAD_START,
                                   now_ms);
    case MEASUREMENT_CAL_STORE_PROGRAM_HEADER_WAIT:
        return wait_program_chunk(store,
                                  COMMIT_OFFSET,
                                  MEASUREMENT_CAL_STORE_PROGRAM_HEADER_START,
                                  MEASUREMENT_CAL_STORE_PROGRAM_PAYLOAD_START,
                                  now_ms);
    case MEASUREMENT_CAL_STORE_PROGRAM_PAYLOAD_START:
        return start_program_chunk(store,
                                   MEASUREMENT_CAL_FRAME_HEADER_BYTES,
                                   store->image_size,
                                   MEASUREMENT_CAL_STORE_PROGRAM_PAYLOAD_WAIT,
                                   MEASUREMENT_CAL_STORE_PROGRAM_COMMIT_START,
                                   now_ms);
    case MEASUREMENT_CAL_STORE_PROGRAM_PAYLOAD_WAIT:
        return wait_program_chunk(store,
                                  store->image_size,
                                  MEASUREMENT_CAL_STORE_PROGRAM_PAYLOAD_START,
                                  MEASUREMENT_CAL_STORE_PROGRAM_COMMIT_START,
                                  now_ms);
    case MEASUREMENT_CAL_STORE_PROGRAM_COMMIT_START:
    {
        const uint32_t address = store->slots[(uint8_t)store->target_slot].start + COMMIT_OFFSET;
        const bsp_status_t status =
            store->io.program_start(address,
                                    &store->image[COMMIT_OFFSET],
                                    COMMIT_BYTES,
                                    now_ms,
                                    store->io.user);
        if ((status != BSP_STATUS_BUSY) && (status != BSP_STATUS_OK))
        {
            store->state = MEASUREMENT_CAL_STORE_ERROR;
            store->last_status = status;
            return status;
        }
        store->state = MEASUREMENT_CAL_STORE_PROGRAM_COMMIT_WAIT;
        return BSP_STATUS_BUSY;
    }
    case MEASUREMENT_CAL_STORE_PROGRAM_COMMIT_WAIT:
    {
        const bsp_status_t status = store->io.poll(now_ms, store->io.user);
        if (status == BSP_STATUS_BUSY)
        {
            return BSP_STATUS_BUSY;
        }
        if (status != BSP_STATUS_OK)
        {
            store->state = MEASUREMENT_CAL_STORE_ERROR;
            store->last_status = status;
            return status;
        }
        store->state = MEASUREMENT_CAL_STORE_VERIFY;
        return BSP_STATUS_BUSY;
    }
    case MEASUREMENT_CAL_STORE_VERIFY:
    {
        measurement_cal_store_slot_info_t info;
        if (!read_slot_info(store,
                            store->target_slot,
                            &store->scan_set,
                            &info,
                            NULL,
                            store->expected_hardware_revision,
                            store->expected_model_version) ||
            (store->scan_set.sequence != store->expected_sequence) ||
            (store->scan_set.hardware_revision != store->expected_hardware_revision) ||
            (store->scan_set.model_version != store->expected_model_version) ||
            (store->scan_set.record_count != store->expected_record_count) ||
            !set_matches_expected_mask(&store->scan_set, store->expected_key_mask) ||
            (info.validity.status != MEASUREMENT_CAL_VALIDITY_VALID))
        {
            store->state = MEASUREMENT_CAL_STORE_ERROR;
            store->last_status = BSP_STATUS_ERROR;
            return BSP_STATUS_ERROR;
        }
        store->state = MEASUREMENT_CAL_STORE_DONE;
        store->last_status = BSP_STATUS_OK;
        return BSP_STATUS_OK;
    }
    default:
        store->state = MEASUREMENT_CAL_STORE_ERROR;
        store->last_status = BSP_STATUS_ERROR;
        return BSP_STATUS_ERROR;
    }
}

measurement_cal_store_state_t measurement_cal_store_state(const measurement_cal_store_t *store)
{
    return (store == NULL) ? MEASUREMENT_CAL_STORE_ERROR : store->state;
}

uint32_t measurement_cal_store_context_size_bytes(void)
{
    return (uint32_t)sizeof(measurement_cal_store_t);
}
