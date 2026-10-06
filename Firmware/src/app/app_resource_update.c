#include "app/app_resource_update.h"

#include <string.h>

#include "storage/resource_store.h"

static uint16_t read_le16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static uint32_t read_le32(const uint8_t *src)
{
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static app_resource_update_status_t status_from_bsp(bsp_status_t status)
{
    if (status == BSP_STATUS_OK)
    {
        return APP_RESOURCE_UPDATE_STATUS_OK;
    }
    if (status == BSP_STATUS_BUSY)
    {
        return APP_RESOURCE_UPDATE_STATUS_BUSY;
    }
    return APP_RESOURCE_UPDATE_STATUS_FLASH;
}

static void set_error(app_resource_update_t *update, app_resource_update_status_t status)
{
    update->last_error = status;
    update->state = APP_RESOURCE_UPDATE_ERROR;
}

void app_resource_update_init(app_resource_update_t *update,
                              const app_resource_update_io_t *io,
                              const storage_partition_t *partition,
                              uint8_t *scratch,
                              size_t scratch_capacity)
{
    if (update == NULL)
    {
        return;
    }
    *update = (app_resource_update_t){0};
    if (io != NULL)
    {
        update->io = *io;
    }
    if (partition != NULL)
    {
        update->partition = *partition;
    }
    update->scratch = scratch;
    update->scratch_capacity = scratch_capacity;
    update->state = APP_RESOURCE_UPDATE_IDLE;
    update->last_error = APP_RESOURCE_UPDATE_STATUS_OK;
    app_pc_link_resource_session_init(&update->pc_session);
}

static bool io_valid(const app_resource_update_t *update)
{
    return (update != NULL) &&
           (update->io.erase_start != NULL) &&
           (update->io.program_start != NULL) &&
           (update->io.poll != NULL) &&
           (update->scratch != NULL) &&
           (update->scratch_capacity >= APP_PC_LINK_MAX_PAYLOAD_BYTES);
}

static app_resource_update_status_t begin_update(app_resource_update_t *update,
                                                 const app_pc_link_frame_t *frame)
{
    app_pc_link_resource_session_t candidate;
    app_pc_link_resource_session_init(&candidate);
    const app_pc_link_status_t pc_status =
        app_pc_link_resource_apply_frame(&candidate, frame, RESOURCE_PACK_API_VERSION);
    update->last_protocol_status = pc_status;
    if (pc_status != APP_PC_LINK_STATUS_OK)
    {
        return APP_RESOURCE_UPDATE_STATUS_PROTOCOL;
    }
    const uint32_t erase_total =
        ((candidate.expected_size + (STORAGE_LAYOUT_W25Q_SECTOR_SIZE - 1u)) /
         STORAGE_LAYOUT_W25Q_SECTOR_SIZE) *
        STORAGE_LAYOUT_W25Q_SECTOR_SIZE;
    if ((erase_total == 0u) || (erase_total > update->partition.size))
    {
        return APP_RESOURCE_UPDATE_STATUS_OUT_OF_RANGE;
    }

    update->pc_session = candidate;
    update->erase_offset = 0u;
    update->erase_total = erase_total;
    update->chunk_offset = 0u;
    update->chunk_payload_length = 0u;
    update->chunk_written = 0u;
    update->active_program_size = 0u;
    update->state = APP_RESOURCE_UPDATE_ERASE_START;
    update->last_error = APP_RESOURCE_UPDATE_STATUS_OK;
    return APP_RESOURCE_UPDATE_STATUS_OK;
}

static app_resource_update_status_t accept_chunk(app_resource_update_t *update,
                                                 const app_pc_link_frame_t *frame)
{
    if (update->state != APP_RESOURCE_UPDATE_READY)
    {
        return APP_RESOURCE_UPDATE_STATUS_BUSY;
    }
    if ((frame->payload == NULL) ||
        (frame->payload_length < APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE) ||
        (frame->payload_length > update->scratch_capacity))
    {
        update->last_protocol_status = APP_PC_LINK_STATUS_BAD_PAYLOAD;
        return APP_RESOURCE_UPDATE_STATUS_PROTOCOL;
    }

    const uint32_t offset = read_le32(&frame->payload[0]);
    const uint16_t byte_count = read_le16(&frame->payload[4]);
    if (((uint16_t)(frame->payload_length - APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE)) != byte_count)
    {
        update->last_protocol_status = APP_PC_LINK_STATUS_BAD_PAYLOAD;
        return APP_RESOURCE_UPDATE_STATUS_PROTOCOL;
    }
    if (offset != update->pc_session.received_size)
    {
        update->last_protocol_status = APP_PC_LINK_STATUS_WRONG_OFFSET;
        return APP_RESOURCE_UPDATE_STATUS_PROTOCOL;
    }
    if (((uint32_t)byte_count > update->pc_session.expected_size) ||
        (offset > (update->pc_session.expected_size - (uint32_t)byte_count)))
    {
        update->last_protocol_status = APP_PC_LINK_STATUS_SIZE_MISMATCH;
        return APP_RESOURCE_UPDATE_STATUS_PROTOCOL;
    }

    if (update->scratch != frame->payload)
    {
        (void)memcpy(update->scratch, frame->payload, frame->payload_length);
    }
    update->chunk_offset = offset;
    update->chunk_payload_length = frame->payload_length;
    update->chunk_written = 0u;
    update->active_program_size = 0u;
    update->state = APP_RESOURCE_UPDATE_PROGRAM_START;
    update->last_protocol_status = APP_PC_LINK_STATUS_OK;
    return APP_RESOURCE_UPDATE_STATUS_OK;
}

static app_resource_update_status_t end_update(app_resource_update_t *update,
                                               const app_pc_link_frame_t *frame)
{
    if (update->state != APP_RESOURCE_UPDATE_READY)
    {
        return APP_RESOURCE_UPDATE_STATUS_BUSY;
    }
    const app_pc_link_status_t pc_status =
        app_pc_link_resource_apply_frame(&update->pc_session, frame, RESOURCE_PACK_API_VERSION);
    update->last_protocol_status = pc_status;
    if (pc_status != APP_PC_LINK_STATUS_OK)
    {
        return APP_RESOURCE_UPDATE_STATUS_PROTOCOL;
    }
    update->state = APP_RESOURCE_UPDATE_COMPLETE;
    return APP_RESOURCE_UPDATE_STATUS_COMPLETE;
}

app_resource_update_status_t app_resource_update_accept_frame(app_resource_update_t *update,
                                                              const app_pc_link_frame_t *frame,
                                                              uint32_t now_ms)
{
    (void)now_ms;
    if (!io_valid(update) || (frame == NULL))
    {
        return APP_RESOURCE_UPDATE_STATUS_INVALID_ARG;
    }
    if (frame->type == APP_PC_LINK_FRAME_ABORT)
    {
        app_resource_update_abort(update);
        return APP_RESOURCE_UPDATE_STATUS_OK;
    }
    if ((update->state != APP_RESOURCE_UPDATE_IDLE) &&
        (update->state != APP_RESOURCE_UPDATE_READY) &&
        (update->state != APP_RESOURCE_UPDATE_COMPLETE))
    {
        return APP_RESOURCE_UPDATE_STATUS_BUSY;
    }

    app_resource_update_status_t status = APP_RESOURCE_UPDATE_STATUS_PROTOCOL;
    switch (frame->type)
    {
    case APP_PC_LINK_FRAME_RESOURCE_BEGIN:
        status = begin_update(update, frame);
        break;
    case APP_PC_LINK_FRAME_RESOURCE_CHUNK:
        status = accept_chunk(update, frame);
        break;
    case APP_PC_LINK_FRAME_RESOURCE_END:
        status = end_update(update, frame);
        break;
    case APP_PC_LINK_FRAME_HELLO:
    case APP_PC_LINK_FRAME_STATUS:
    default:
        update->last_protocol_status = APP_PC_LINK_STATUS_UNEXPECTED_FRAME;
        status = APP_RESOURCE_UPDATE_STATUS_PROTOCOL;
        break;
    }
    if ((status != APP_RESOURCE_UPDATE_STATUS_OK) &&
        (status != APP_RESOURCE_UPDATE_STATUS_BUSY) &&
        (status != APP_RESOURCE_UPDATE_STATUS_COMPLETE))
    {
        set_error(update, status);
    }
    return status;
}

static app_resource_update_status_t step_erase_start(app_resource_update_t *update, uint32_t now_ms)
{
    const uint32_t address = update->partition.start + update->erase_offset;
    const app_resource_update_status_t status =
        status_from_bsp(update->io.erase_start(address, now_ms, update->io.user));
    if (status == APP_RESOURCE_UPDATE_STATUS_BUSY)
    {
        update->state = APP_RESOURCE_UPDATE_ERASE_POLL;
        return APP_RESOURCE_UPDATE_STATUS_BUSY;
    }
    if (status == APP_RESOURCE_UPDATE_STATUS_OK)
    {
        update->erase_offset += STORAGE_LAYOUT_W25Q_SECTOR_SIZE;
        update->state = (update->erase_offset >= update->erase_total) ?
                            APP_RESOURCE_UPDATE_READY :
                            APP_RESOURCE_UPDATE_ERASE_START;
        return APP_RESOURCE_UPDATE_STATUS_OK;
    }
    set_error(update, status);
    return status;
}

static app_resource_update_status_t step_erase_poll(app_resource_update_t *update, uint32_t now_ms)
{
    const app_resource_update_status_t status = status_from_bsp(update->io.poll(now_ms, update->io.user));
    if (status == APP_RESOURCE_UPDATE_STATUS_BUSY)
    {
        return APP_RESOURCE_UPDATE_STATUS_BUSY;
    }
    if (status == APP_RESOURCE_UPDATE_STATUS_OK)
    {
        update->erase_offset += STORAGE_LAYOUT_W25Q_SECTOR_SIZE;
        update->state = (update->erase_offset >= update->erase_total) ?
                            APP_RESOURCE_UPDATE_READY :
                            APP_RESOURCE_UPDATE_ERASE_START;
        return APP_RESOURCE_UPDATE_STATUS_OK;
    }
    set_error(update, status);
    return status;
}

static app_resource_update_status_t step_program_start(app_resource_update_t *update, uint32_t now_ms)
{
    const uint16_t data_size =
        (uint16_t)(update->chunk_payload_length - APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE);
    const uint32_t absolute = update->partition.start + update->chunk_offset +
                              (uint32_t)update->chunk_written;
    uint16_t span = (uint16_t)(data_size - update->chunk_written);
    const uint16_t page_remaining =
        (uint16_t)(APP_RESOURCE_UPDATE_PAGE_BYTES -
                   (uint16_t)(absolute % APP_RESOURCE_UPDATE_PAGE_BYTES));
    if (span > page_remaining)
    {
        span = page_remaining;
    }
    const uint8_t *src = &update->scratch[APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE +
                                          update->chunk_written];
    const app_resource_update_status_t status =
        status_from_bsp(update->io.program_start(absolute, src, span, now_ms, update->io.user));
    if (status == APP_RESOURCE_UPDATE_STATUS_BUSY)
    {
        update->active_program_size = span;
        update->state = APP_RESOURCE_UPDATE_PROGRAM_POLL;
        return APP_RESOURCE_UPDATE_STATUS_BUSY;
    }
    if (status == APP_RESOURCE_UPDATE_STATUS_OK)
    {
        update->chunk_written = (uint16_t)(update->chunk_written + span);
        update->state = (update->chunk_written >= data_size) ?
                            APP_RESOURCE_UPDATE_READY :
                            APP_RESOURCE_UPDATE_PROGRAM_START;
        return APP_RESOURCE_UPDATE_STATUS_OK;
    }
    set_error(update, status);
    return status;
}

static app_resource_update_status_t step_program_poll(app_resource_update_t *update, uint32_t now_ms)
{
    const app_resource_update_status_t status = status_from_bsp(update->io.poll(now_ms, update->io.user));
    if (status == APP_RESOURCE_UPDATE_STATUS_BUSY)
    {
        return APP_RESOURCE_UPDATE_STATUS_BUSY;
    }
    if (status != APP_RESOURCE_UPDATE_STATUS_OK)
    {
        set_error(update, status);
        return status;
    }

    update->chunk_written = (uint16_t)(update->chunk_written + update->active_program_size);
    update->active_program_size = 0u;
    const uint16_t data_size =
        (uint16_t)(update->chunk_payload_length - APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE);
    if (update->chunk_written < data_size)
    {
        update->state = APP_RESOURCE_UPDATE_PROGRAM_START;
        return APP_RESOURCE_UPDATE_STATUS_OK;
    }

    const app_pc_link_frame_t applied = {
        .type = APP_PC_LINK_FRAME_RESOURCE_CHUNK,
        .flags = 0u,
        .sequence = 0u,
        .payload_length = update->chunk_payload_length,
        .payload = update->scratch,
    };
    const app_pc_link_status_t pc_status =
        app_pc_link_resource_apply_frame(&update->pc_session, &applied, RESOURCE_PACK_API_VERSION);
    update->last_protocol_status = pc_status;
    if (pc_status != APP_PC_LINK_STATUS_OK)
    {
        set_error(update, APP_RESOURCE_UPDATE_STATUS_PROTOCOL);
        return APP_RESOURCE_UPDATE_STATUS_PROTOCOL;
    }
    update->state = APP_RESOURCE_UPDATE_READY;
    return APP_RESOURCE_UPDATE_STATUS_OK;
}

app_resource_update_status_t app_resource_update_step(app_resource_update_t *update,
                                                      uint32_t now_ms)
{
    if (!io_valid(update))
    {
        return APP_RESOURCE_UPDATE_STATUS_INVALID_ARG;
    }
    switch (update->state)
    {
    case APP_RESOURCE_UPDATE_IDLE:
    case APP_RESOURCE_UPDATE_READY:
        return APP_RESOURCE_UPDATE_STATUS_OK;
    case APP_RESOURCE_UPDATE_ERASE_START:
        return step_erase_start(update, now_ms);
    case APP_RESOURCE_UPDATE_ERASE_POLL:
        return step_erase_poll(update, now_ms);
    case APP_RESOURCE_UPDATE_PROGRAM_START:
        return step_program_start(update, now_ms);
    case APP_RESOURCE_UPDATE_PROGRAM_POLL:
        return step_program_poll(update, now_ms);
    case APP_RESOURCE_UPDATE_COMPLETE:
        return APP_RESOURCE_UPDATE_STATUS_COMPLETE;
    case APP_RESOURCE_UPDATE_ERROR:
    default:
        return update->last_error;
    }
}

void app_resource_update_abort(app_resource_update_t *update)
{
    if (update != NULL)
    {
        app_pc_link_resource_session_init(&update->pc_session);
        update->state = APP_RESOURCE_UPDATE_IDLE;
        update->last_error = APP_RESOURCE_UPDATE_STATUS_OK;
        update->last_protocol_status = APP_PC_LINK_STATUS_OK;
        update->erase_offset = 0u;
        update->erase_total = 0u;
        update->chunk_offset = 0u;
        update->chunk_payload_length = 0u;
        update->chunk_written = 0u;
        update->active_program_size = 0u;
    }
}

bool app_resource_update_busy(const app_resource_update_t *update)
{
    if (update == NULL)
    {
        return false;
    }
    return (update->state == APP_RESOURCE_UPDATE_ERASE_START) ||
           (update->state == APP_RESOURCE_UPDATE_ERASE_POLL) ||
           (update->state == APP_RESOURCE_UPDATE_PROGRAM_START) ||
           (update->state == APP_RESOURCE_UPDATE_PROGRAM_POLL);
}

bool app_resource_update_active(const app_resource_update_t *update)
{
    if (update == NULL)
    {
        return false;
    }
    return (update->state != APP_RESOURCE_UPDATE_IDLE) &&
           (update->state != APP_RESOURCE_UPDATE_COMPLETE) &&
           (update->state != APP_RESOURCE_UPDATE_ERROR);
}

bool app_resource_update_complete(const app_resource_update_t *update)
{
    return (update != NULL) && (update->state == APP_RESOURCE_UPDATE_COMPLETE);
}

app_resource_update_state_t app_resource_update_state(const app_resource_update_t *update)
{
    return (update == NULL) ? APP_RESOURCE_UPDATE_ERROR : update->state;
}

app_resource_update_status_t app_resource_update_last_error(const app_resource_update_t *update)
{
    return (update == NULL) ? APP_RESOURCE_UPDATE_STATUS_INVALID_ARG : update->last_error;
}

app_pc_link_status_t app_resource_update_last_protocol_status(const app_resource_update_t *update)
{
    return (update == NULL) ? APP_PC_LINK_STATUS_INVALID_ARG : update->last_protocol_status;
}

uint32_t app_resource_update_context_size_bytes(void)
{
    return (uint32_t)sizeof(app_resource_update_t);
}

const char *app_resource_update_status_string(app_resource_update_status_t status)
{
    switch (status)
    {
    case APP_RESOURCE_UPDATE_STATUS_OK:
        return "OK";
    case APP_RESOURCE_UPDATE_STATUS_BUSY:
        return "BUSY";
    case APP_RESOURCE_UPDATE_STATUS_COMPLETE:
        return "COMPLETE";
    case APP_RESOURCE_UPDATE_STATUS_INVALID_ARG:
        return "INVALID_ARG";
    case APP_RESOURCE_UPDATE_STATUS_PROTOCOL:
        return "PROTOCOL";
    case APP_RESOURCE_UPDATE_STATUS_OUT_OF_RANGE:
        return "OUT_OF_RANGE";
    case APP_RESOURCE_UPDATE_STATUS_FLASH:
    default:
        return "FLASH";
    }
}
