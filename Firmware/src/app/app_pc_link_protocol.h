#ifndef WTK_APP_PC_LINK_PROTOCOL_H
#define WTK_APP_PC_LINK_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    APP_PC_LINK_MAGIC = 0x31434C50u, /* "PLC1" little-endian */
    APP_PC_LINK_VERSION = 1u,
    APP_PC_LINK_HEADER_SIZE = 16u,
    APP_PC_LINK_MAX_PAYLOAD_BYTES = 128u,
    APP_PC_LINK_RESOURCE_BEGIN_SIZE = 12u,
    APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE = 8u,
    APP_PC_LINK_RESOURCE_END_SIZE = 8u,
};

typedef enum
{
    APP_PC_LINK_FRAME_HELLO = 1u,
    APP_PC_LINK_FRAME_STATUS = 2u,
    APP_PC_LINK_FRAME_RESOURCE_BEGIN = 16u,
    APP_PC_LINK_FRAME_RESOURCE_CHUNK = 17u,
    APP_PC_LINK_FRAME_RESOURCE_END = 18u,
    APP_PC_LINK_FRAME_ABORT = 19u,
} app_pc_link_frame_type_t;

typedef enum
{
    APP_PC_LINK_STATUS_OK = 0,
    APP_PC_LINK_STATUS_INVALID_ARG,
    APP_PC_LINK_STATUS_BAD_MAGIC,
    APP_PC_LINK_STATUS_UNSUPPORTED_VERSION,
    APP_PC_LINK_STATUS_TRUNCATED,
    APP_PC_LINK_STATUS_PAYLOAD_TOO_LARGE,
    APP_PC_LINK_STATUS_CRC_MISMATCH,
    APP_PC_LINK_STATUS_UNEXPECTED_FRAME,
    APP_PC_LINK_STATUS_BAD_PAYLOAD,
    APP_PC_LINK_STATUS_WRONG_OFFSET,
    APP_PC_LINK_STATUS_SIZE_MISMATCH,
    APP_PC_LINK_STATUS_API_MISMATCH,
    APP_PC_LINK_STATUS_NOT_ACTIVE,
} app_pc_link_status_t;

typedef struct
{
    app_pc_link_frame_type_t type;
    uint16_t flags;
    uint16_t sequence;
    uint16_t payload_length;
    const uint8_t *payload;
} app_pc_link_frame_t;

typedef struct
{
    bool active;
    bool complete;
    uint32_t expected_size;
    uint32_t expected_crc32;
    uint32_t running_crc32;
    uint32_t received_size;
    uint16_t expected_api_version;
} app_pc_link_resource_session_t;

void app_pc_link_encode_header(uint8_t dst[APP_PC_LINK_HEADER_SIZE],
                               app_pc_link_frame_type_t type,
                               uint16_t flags,
                               uint16_t sequence,
                               const void *payload,
                               uint16_t payload_length);
app_pc_link_status_t app_pc_link_decode_frame(const uint8_t *frame_bytes,
                                              size_t frame_size,
                                              app_pc_link_frame_t *frame);
void app_pc_link_resource_session_init(app_pc_link_resource_session_t *session);
app_pc_link_status_t app_pc_link_resource_apply_frame(app_pc_link_resource_session_t *session,
                                                      const app_pc_link_frame_t *frame,
                                                      uint16_t expected_api_version);
const char *app_pc_link_status_string(app_pc_link_status_t status);

#endif
