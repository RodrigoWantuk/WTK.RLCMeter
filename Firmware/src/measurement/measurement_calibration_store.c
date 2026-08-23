#include "measurement/measurement_calibration_store.h"

#include <string.h>

enum
{
    COMMIT_OFFSET = 60u,
    COMMIT_BYTES = 4u,
    PAGE_BYTES = 256u,
};

static bool io_valid(const measurement_cal_store_io_t *io)
{
    return (io != NULL) &&
           (io->read != NULL) &&
           (io->erase_sector != NULL) &&
           (io->program != NULL) &&
           (io->poll != NULL);
}

bool measurement_cal_store_sequence_newer(uint32_t a, uint32_t b)
{
    return (a != b) && ((uint32_t)(a - b) < 0x80000000u);
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

static bool read_slot(measurement_cal_store_t *store,
                      measurement_cal_store_slot_t slot,
                      measurement_cal_set_t *set)
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
        return false;
    }
    return measurement_cal_decode_set(store->image, MEASUREMENT_CAL_MAX_FRAME_BYTES, set, NULL);
}

bsp_status_t measurement_cal_store_load_newest(measurement_cal_store_t *store,
                                               measurement_cal_set_t *set,
                                               measurement_cal_store_slot_t *slot)
{
    if ((store == NULL) || (set == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    measurement_cal_set_t set_a;
    measurement_cal_set_t set_b;
    const bool valid_a = read_slot(store, MEASUREMENT_CAL_STORE_SLOT_A, &set_a);
    const bool valid_b = read_slot(store, MEASUREMENT_CAL_STORE_SLOT_B, &set_b);
    if (!valid_a && !valid_b)
    {
        return BSP_STATUS_ERROR;
    }
    if (valid_a && (!valid_b || measurement_cal_store_sequence_newer(set_a.sequence, set_b.sequence)))
    {
        *set = set_a;
        if (slot != NULL)
        {
            *slot = MEASUREMENT_CAL_STORE_SLOT_A;
        }
        return BSP_STATUS_OK;
    }
    *set = set_b;
    if (slot != NULL)
    {
        *slot = MEASUREMENT_CAL_STORE_SLOT_B;
    }
    return BSP_STATUS_OK;
}

bsp_status_t measurement_cal_store_write_start(measurement_cal_store_t *store,
                                               const measurement_cal_set_t *candidate)
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

    measurement_cal_set_t current;
    measurement_cal_store_slot_t current_slot = MEASUREMENT_CAL_STORE_SLOT_B;
    measurement_cal_store_slot_t target_slot = MEASUREMENT_CAL_STORE_SLOT_A;
    measurement_cal_set_t staged = *candidate;
    if (measurement_cal_store_load_newest(store, &current, NULL) == BSP_STATUS_OK)
    {
        staged.sequence = current.sequence + 1u;
    }
    else if (staged.sequence == 0u)
    {
        staged.sequence = 1u;
    }
    if (measurement_cal_store_load_newest(store, &current, &current_slot) == BSP_STATUS_OK)
    {
        target_slot = (current_slot == MEASUREMENT_CAL_STORE_SLOT_A) ? MEASUREMENT_CAL_STORE_SLOT_B :
                                                                       MEASUREMENT_CAL_STORE_SLOT_A;
    }

    size_t written = 0u;
    if (!measurement_cal_serialize_set(&staged, store->image, sizeof(store->image), &written))
    {
        store->state = MEASUREMENT_CAL_STORE_ERROR;
        store->last_status = BSP_STATUS_INVALID_ARG;
        return BSP_STATUS_INVALID_ARG;
    }

    store->image_size = written;
    store->program_offset = 0u;
    store->target_slot = target_slot;
    store->state = MEASUREMENT_CAL_STORE_ERASE;
    store->last_status = BSP_STATUS_BUSY;
    return BSP_STATUS_BUSY;
}

static size_t page_span(uint32_t address, size_t remaining)
{
    const uint32_t page_offset = address % PAGE_BYTES;
    const size_t page_remaining = (size_t)(PAGE_BYTES - page_offset);
    return (remaining < page_remaining) ? remaining : page_remaining;
}

static bsp_status_t program_segment(measurement_cal_store_t *store,
                                    size_t begin,
                                    size_t end,
                                    measurement_cal_store_state_t next_state)
{
    if (store->program_offset < begin)
    {
        store->program_offset = begin;
    }
    if (store->program_offset >= end)
    {
        store->state = next_state;
        return BSP_STATUS_BUSY;
    }
    const uint32_t address = store->slots[(uint8_t)store->target_slot].start +
                             (uint32_t)store->program_offset;
    const size_t remaining = end - store->program_offset;
    const size_t chunk = page_span(address, remaining);
    const bsp_status_t status =
        store->io.program(address, &store->image[store->program_offset], chunk, store->io.user);
    if (status != BSP_STATUS_OK)
    {
        store->state = MEASUREMENT_CAL_STORE_ERROR;
        store->last_status = status;
        return status;
    }
    store->program_offset += chunk;
    return BSP_STATUS_BUSY;
}

bsp_status_t measurement_cal_store_step(measurement_cal_store_t *store)
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
    case MEASUREMENT_CAL_STORE_ERASE:
    {
        const bsp_status_t status =
            store->io.erase_sector(store->slots[(uint8_t)store->target_slot].start, store->io.user);
        if (status != BSP_STATUS_OK)
        {
            store->state = MEASUREMENT_CAL_STORE_ERROR;
            store->last_status = status;
            return status;
        }
        store->program_offset = 0u;
        store->state = MEASUREMENT_CAL_STORE_PROGRAM_HEADER;
        return BSP_STATUS_BUSY;
    }
    case MEASUREMENT_CAL_STORE_PROGRAM_HEADER:
        return program_segment(store,
                               0u,
                               COMMIT_OFFSET,
                               MEASUREMENT_CAL_STORE_PROGRAM_PAYLOAD);
    case MEASUREMENT_CAL_STORE_PROGRAM_PAYLOAD:
        return program_segment(store,
                               MEASUREMENT_CAL_FRAME_HEADER_BYTES,
                               store->image_size,
                               MEASUREMENT_CAL_STORE_PROGRAM_COMMIT);
    case MEASUREMENT_CAL_STORE_PROGRAM_COMMIT:
    {
        const uint32_t address = store->slots[(uint8_t)store->target_slot].start + COMMIT_OFFSET;
        const bsp_status_t status =
            store->io.program(address, &store->image[COMMIT_OFFSET], COMMIT_BYTES, store->io.user);
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
        measurement_cal_set_t verify;
        if (!read_slot(store, store->target_slot, &verify))
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
