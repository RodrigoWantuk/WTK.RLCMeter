#include "app/app_pc_link_protocol.h"

#include "storage/storage_crc32.h"

static void write_le16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void write_le32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24) & 0xFFu);
}

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

void app_pc_link_encode_header(uint8_t dst[APP_PC_LINK_HEADER_SIZE],
                               app_pc_link_frame_type_t type,
                               uint16_t flags,
                               uint16_t sequence,
                               const void *payload,
                               uint16_t payload_length)
{
    if (dst == 0)
    {
        return;
    }

    const uint32_t payload_crc =
        (payload_length == 0u) ? storage_crc32(0, 0u) : storage_crc32(payload, payload_length);
    write_le32(&dst[0], APP_PC_LINK_MAGIC);
    dst[4] = (uint8_t)APP_PC_LINK_VERSION;
    dst[5] = (uint8_t)type;
    write_le16(&dst[6], flags);
    write_le16(&dst[8], sequence);
    write_le16(&dst[10], payload_length);
    write_le32(&dst[12], payload_crc);
}

app_pc_link_status_t app_pc_link_decode_frame(const uint8_t *frame_bytes,
                                              size_t frame_size,
                                              app_pc_link_frame_t *frame)
{
    if ((frame_bytes == 0) || (frame == 0))
    {
        return APP_PC_LINK_STATUS_INVALID_ARG;
    }
    if (frame_size < APP_PC_LINK_HEADER_SIZE)
    {
        return APP_PC_LINK_STATUS_TRUNCATED;
    }
    if (read_le32(&frame_bytes[0]) != APP_PC_LINK_MAGIC)
    {
        return APP_PC_LINK_STATUS_BAD_MAGIC;
    }
    if (frame_bytes[4] != APP_PC_LINK_VERSION)
    {
        return APP_PC_LINK_STATUS_UNSUPPORTED_VERSION;
    }

    const uint16_t payload_length = read_le16(&frame_bytes[10]);
    if (payload_length > APP_PC_LINK_MAX_PAYLOAD_BYTES)
    {
        return APP_PC_LINK_STATUS_PAYLOAD_TOO_LARGE;
    }
    if (frame_size < ((size_t)APP_PC_LINK_HEADER_SIZE + (size_t)payload_length))
    {
        return APP_PC_LINK_STATUS_TRUNCATED;
    }

    const uint8_t *payload = &frame_bytes[APP_PC_LINK_HEADER_SIZE];
    const uint32_t expected_crc = read_le32(&frame_bytes[12]);
    const uint32_t actual_crc =
        (payload_length == 0u) ? storage_crc32(0, 0u) : storage_crc32(payload, payload_length);
    if (actual_crc != expected_crc)
    {
        return APP_PC_LINK_STATUS_CRC_MISMATCH;
    }

    *frame = (app_pc_link_frame_t){
        .type = (app_pc_link_frame_type_t)frame_bytes[5],
        .flags = read_le16(&frame_bytes[6]),
        .sequence = read_le16(&frame_bytes[8]),
        .payload_length = payload_length,
        .payload = payload,
    };
    return APP_PC_LINK_STATUS_OK;
}

void app_pc_link_resource_session_init(app_pc_link_resource_session_t *session)
{
    if (session != 0)
    {
        *session = (app_pc_link_resource_session_t){
            .running_crc32 = STORAGE_CRC32_INIT,
        };
    }
}

static app_pc_link_status_t apply_begin(app_pc_link_resource_session_t *session,
                                        const app_pc_link_frame_t *frame,
                                        uint16_t expected_api_version)
{
    if (frame->payload_length != APP_PC_LINK_RESOURCE_BEGIN_SIZE)
    {
        return APP_PC_LINK_STATUS_BAD_PAYLOAD;
    }
    const uint32_t total_size = read_le32(&frame->payload[0]);
    const uint32_t pack_crc32 = read_le32(&frame->payload[4]);
    const uint16_t api_version = read_le16(&frame->payload[8]);
    if ((total_size == 0u) || (api_version != expected_api_version))
    {
        return (api_version != expected_api_version) ?
                   APP_PC_LINK_STATUS_API_MISMATCH :
                   APP_PC_LINK_STATUS_BAD_PAYLOAD;
    }

    *session = (app_pc_link_resource_session_t){
        .active = true,
        .complete = false,
        .expected_size = total_size,
        .expected_crc32 = pack_crc32,
        .running_crc32 = STORAGE_CRC32_INIT,
        .received_size = 0u,
        .expected_api_version = expected_api_version,
    };
    return APP_PC_LINK_STATUS_OK;
}

static app_pc_link_status_t apply_chunk(app_pc_link_resource_session_t *session,
                                        const app_pc_link_frame_t *frame)
{
    if (!session->active)
    {
        return APP_PC_LINK_STATUS_NOT_ACTIVE;
    }
    if (frame->payload_length < APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE)
    {
        return APP_PC_LINK_STATUS_BAD_PAYLOAD;
    }

    const uint32_t offset = read_le32(&frame->payload[0]);
    const uint16_t byte_count = read_le16(&frame->payload[4]);
    if (((uint16_t)(frame->payload_length - APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE)) != byte_count)
    {
        return APP_PC_LINK_STATUS_BAD_PAYLOAD;
    }
    if (offset != session->received_size)
    {
        return APP_PC_LINK_STATUS_WRONG_OFFSET;
    }
    if (((uint32_t)byte_count > session->expected_size) ||
        (session->received_size > (session->expected_size - (uint32_t)byte_count)))
    {
        return APP_PC_LINK_STATUS_SIZE_MISMATCH;
    }

    const uint8_t *data = &frame->payload[APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE];
    session->running_crc32 = storage_crc32_update(session->running_crc32, data, byte_count);
    session->received_size += (uint32_t)byte_count;
    return APP_PC_LINK_STATUS_OK;
}

static app_pc_link_status_t apply_end(app_pc_link_resource_session_t *session,
                                      const app_pc_link_frame_t *frame)
{
    if (!session->active)
    {
        return APP_PC_LINK_STATUS_NOT_ACTIVE;
    }
    if (frame->payload_length != APP_PC_LINK_RESOURCE_END_SIZE)
    {
        return APP_PC_LINK_STATUS_BAD_PAYLOAD;
    }

    const uint32_t total_size = read_le32(&frame->payload[0]);
    const uint32_t pack_crc32 = read_le32(&frame->payload[4]);
    const uint32_t final_crc = session->running_crc32 ^ STORAGE_CRC32_XOR_OUT;
    if ((total_size != session->expected_size) || (session->received_size != session->expected_size))
    {
        return APP_PC_LINK_STATUS_SIZE_MISMATCH;
    }
    if ((pack_crc32 != session->expected_crc32) || (final_crc != session->expected_crc32))
    {
        return APP_PC_LINK_STATUS_CRC_MISMATCH;
    }

    session->active = false;
    session->complete = true;
    return APP_PC_LINK_STATUS_OK;
}

app_pc_link_status_t app_pc_link_resource_apply_frame(app_pc_link_resource_session_t *session,
                                                      const app_pc_link_frame_t *frame,
                                                      uint16_t expected_api_version)
{
    if ((session == 0) || (frame == 0))
    {
        return APP_PC_LINK_STATUS_INVALID_ARG;
    }

    switch (frame->type)
    {
    case APP_PC_LINK_FRAME_RESOURCE_BEGIN:
        return apply_begin(session, frame, expected_api_version);
    case APP_PC_LINK_FRAME_RESOURCE_CHUNK:
        return apply_chunk(session, frame);
    case APP_PC_LINK_FRAME_RESOURCE_END:
        return apply_end(session, frame);
    case APP_PC_LINK_FRAME_ABORT:
        app_pc_link_resource_session_init(session);
        return APP_PC_LINK_STATUS_OK;
    case APP_PC_LINK_FRAME_HELLO:
    case APP_PC_LINK_FRAME_STATUS:
    default:
        return APP_PC_LINK_STATUS_UNEXPECTED_FRAME;
    }
}

const char *app_pc_link_status_string(app_pc_link_status_t status)
{
    switch (status)
    {
    case APP_PC_LINK_STATUS_OK:
        return "OK";
    case APP_PC_LINK_STATUS_INVALID_ARG:
        return "INVALID_ARG";
    case APP_PC_LINK_STATUS_BAD_MAGIC:
        return "BAD_MAGIC";
    case APP_PC_LINK_STATUS_UNSUPPORTED_VERSION:
        return "UNSUPPORTED_VERSION";
    case APP_PC_LINK_STATUS_TRUNCATED:
        return "TRUNCATED";
    case APP_PC_LINK_STATUS_PAYLOAD_TOO_LARGE:
        return "PAYLOAD_TOO_LARGE";
    case APP_PC_LINK_STATUS_CRC_MISMATCH:
        return "CRC_MISMATCH";
    case APP_PC_LINK_STATUS_UNEXPECTED_FRAME:
        return "UNEXPECTED_FRAME";
    case APP_PC_LINK_STATUS_BAD_PAYLOAD:
        return "BAD_PAYLOAD";
    case APP_PC_LINK_STATUS_WRONG_OFFSET:
        return "WRONG_OFFSET";
    case APP_PC_LINK_STATUS_SIZE_MISMATCH:
        return "SIZE_MISMATCH";
    case APP_PC_LINK_STATUS_API_MISMATCH:
        return "API_MISMATCH";
    case APP_PC_LINK_STATUS_NOT_ACTIVE:
        return "NOT_ACTIVE";
    default:
        return "UNKNOWN";
    }
}
