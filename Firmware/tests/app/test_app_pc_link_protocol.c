#include "app/app_pc_link_protocol.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "storage/resource_store.h"
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

static void require_bool(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static size_t make_frame(app_pc_link_frame_type_t type,
                         uint16_t sequence,
                         const uint8_t *payload,
                         uint16_t payload_length,
                         uint8_t *dst)
{
    app_pc_link_encode_header(dst, type, 0u, sequence, payload, payload_length);
    if (payload_length > 0u)
    {
        memcpy(&dst[APP_PC_LINK_HEADER_SIZE], payload, payload_length);
    }
    return (size_t)APP_PC_LINK_HEADER_SIZE + (size_t)payload_length;
}

static app_pc_link_frame_t decode_ok(const uint8_t *bytes, size_t size)
{
    app_pc_link_frame_t frame;
    const app_pc_link_status_t status = app_pc_link_decode_frame(bytes, size, &frame);
    require_bool(status == APP_PC_LINK_STATUS_OK);
    return frame;
}

static void test_decode_rejects_bad_header_and_crc(void)
{
    const uint8_t payload[] = {1u, 2u, 3u};
    uint8_t bytes[APP_PC_LINK_HEADER_SIZE + sizeof(payload)];
    const size_t size = make_frame(APP_PC_LINK_FRAME_HELLO, 7u, payload, sizeof(payload), bytes);

    app_pc_link_frame_t frame;
    require_bool(app_pc_link_decode_frame(bytes, size, &frame) == APP_PC_LINK_STATUS_OK);
    require_bool(frame.type == APP_PC_LINK_FRAME_HELLO);
    require_bool(frame.sequence == 7u);
    require_bool(frame.payload_length == sizeof(payload));
    require_bool(memcmp(frame.payload, payload, sizeof(payload)) == 0);

    bytes[0] ^= 0x55u;
    require_bool(app_pc_link_decode_frame(bytes, size, &frame) == APP_PC_LINK_STATUS_BAD_MAGIC);
    bytes[0] ^= 0x55u;

    bytes[APP_PC_LINK_HEADER_SIZE + 1u] ^= 0x40u;
    require_bool(app_pc_link_decode_frame(bytes, size, &frame) == APP_PC_LINK_STATUS_CRC_MISMATCH);
}

static void test_resource_session_accepts_ordered_chunks(void)
{
    const uint8_t pack[] = {
        0x50u, 0x41u, 0x43u, 0x4Bu, 0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u,
    };
    const uint32_t pack_crc = storage_crc32(pack, sizeof(pack));
    uint8_t payload[APP_PC_LINK_MAX_PAYLOAD_BYTES];
    uint8_t bytes[APP_PC_LINK_HEADER_SIZE + APP_PC_LINK_MAX_PAYLOAD_BYTES];
    app_pc_link_resource_session_t session;
    app_pc_link_resource_session_init(&session);

    write_le32(&payload[0], (uint32_t)sizeof(pack));
    write_le32(&payload[4], pack_crc);
    write_le16(&payload[8], RESOURCE_PACK_API_VERSION);
    write_le16(&payload[10], 0u);
    size_t size = make_frame(APP_PC_LINK_FRAME_RESOURCE_BEGIN,
                             1u,
                             payload,
                             APP_PC_LINK_RESOURCE_BEGIN_SIZE,
                             bytes);
    app_pc_link_frame_t frame = decode_ok(bytes, size);
    require_bool(app_pc_link_resource_apply_frame(&session, &frame, RESOURCE_PACK_API_VERSION) ==
           APP_PC_LINK_STATUS_OK);
    require_bool(session.active);
    require_bool(!session.complete);

    write_le32(&payload[0], 0u);
    write_le16(&payload[4], 4u);
    write_le16(&payload[6], 0u);
    memcpy(&payload[APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE], pack, 4u);
    size = make_frame(APP_PC_LINK_FRAME_RESOURCE_CHUNK,
                      2u,
                      payload,
                      APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE + 4u,
                      bytes);
    frame = decode_ok(bytes, size);
    require_bool(app_pc_link_resource_apply_frame(&session, &frame, RESOURCE_PACK_API_VERSION) ==
           APP_PC_LINK_STATUS_OK);
    require_bool(session.received_size == 4u);

    write_le32(&payload[0], 4u);
    write_le16(&payload[4], 6u);
    write_le16(&payload[6], 0u);
    memcpy(&payload[APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE], &pack[4], 6u);
    size = make_frame(APP_PC_LINK_FRAME_RESOURCE_CHUNK,
                      3u,
                      payload,
                      APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE + 6u,
                      bytes);
    frame = decode_ok(bytes, size);
    require_bool(app_pc_link_resource_apply_frame(&session, &frame, RESOURCE_PACK_API_VERSION) ==
           APP_PC_LINK_STATUS_OK);
    require_bool(session.received_size == sizeof(pack));

    write_le32(&payload[0], (uint32_t)sizeof(pack));
    write_le32(&payload[4], pack_crc);
    size = make_frame(APP_PC_LINK_FRAME_RESOURCE_END,
                      4u,
                      payload,
                      APP_PC_LINK_RESOURCE_END_SIZE,
                      bytes);
    frame = decode_ok(bytes, size);
    require_bool(app_pc_link_resource_apply_frame(&session, &frame, RESOURCE_PACK_API_VERSION) ==
           APP_PC_LINK_STATUS_OK);
    require_bool(!session.active);
    require_bool(session.complete);
}

static void test_resource_session_rejects_mismatch_cases(void)
{
    const uint8_t pack[] = {1u, 2u, 3u, 4u};
    uint8_t payload[APP_PC_LINK_MAX_PAYLOAD_BYTES];
    uint8_t bytes[APP_PC_LINK_HEADER_SIZE + APP_PC_LINK_MAX_PAYLOAD_BYTES];
    app_pc_link_resource_session_t session;
    app_pc_link_resource_session_init(&session);

    write_le32(&payload[0], (uint32_t)sizeof(pack));
    write_le32(&payload[4], storage_crc32(pack, sizeof(pack)));
    write_le16(&payload[8], (uint16_t)(RESOURCE_PACK_API_VERSION + 1u));
    write_le16(&payload[10], 0u);
    size_t size = make_frame(APP_PC_LINK_FRAME_RESOURCE_BEGIN,
                             1u,
                             payload,
                             APP_PC_LINK_RESOURCE_BEGIN_SIZE,
                             bytes);
    app_pc_link_frame_t frame = decode_ok(bytes, size);
    require_bool(app_pc_link_resource_apply_frame(&session, &frame, RESOURCE_PACK_API_VERSION) ==
           APP_PC_LINK_STATUS_API_MISMATCH);

    write_le16(&payload[8], RESOURCE_PACK_API_VERSION);
    size = make_frame(APP_PC_LINK_FRAME_RESOURCE_BEGIN,
                      2u,
                      payload,
                      APP_PC_LINK_RESOURCE_BEGIN_SIZE,
                      bytes);
    frame = decode_ok(bytes, size);
    require_bool(app_pc_link_resource_apply_frame(&session, &frame, RESOURCE_PACK_API_VERSION) ==
           APP_PC_LINK_STATUS_OK);

    write_le32(&payload[0], 1u);
    write_le16(&payload[4], 1u);
    write_le16(&payload[6], 0u);
    payload[8] = 9u;
    size = make_frame(APP_PC_LINK_FRAME_RESOURCE_CHUNK,
                      3u,
                      payload,
                      APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE + 1u,
                      bytes);
    frame = decode_ok(bytes, size);
    require_bool(app_pc_link_resource_apply_frame(&session, &frame, RESOURCE_PACK_API_VERSION) ==
           APP_PC_LINK_STATUS_WRONG_OFFSET);

    frame.type = APP_PC_LINK_FRAME_ABORT;
    frame.payload_length = 0u;
    frame.payload = 0;
    require_bool(app_pc_link_resource_apply_frame(&session, &frame, RESOURCE_PACK_API_VERSION) ==
           APP_PC_LINK_STATUS_OK);
    require_bool(!session.active);
    require_bool(!session.complete);
}

int main(void)
{
    test_decode_rejects_bad_header_and_crc();
    test_resource_session_accepts_ordered_chunks();
    test_resource_session_rejects_mismatch_cases();
    return 0;
}
