#include "app/app_resource_update.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "storage/resource_store.h"
#include "storage/storage_crc32.h"

enum
{
    FAKE_FLASH_BYTES = STORAGE_LAYOUT_W25Q_SECTOR_SIZE * 2u,
};

typedef struct
{
    uint8_t flash[FAKE_FLASH_BYTES];
    bool busy;
    uint32_t erase_addresses[4];
    uint8_t erase_count;
    uint32_t program_addresses[8];
    uint16_t program_sizes[8];
    uint8_t program_count;
    bool fail_next_program;
} fake_flash_t;

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

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

static size_t make_frame(app_pc_link_frame_type_t type,
                         uint16_t sequence,
                         const uint8_t *payload,
                         uint16_t payload_length,
                         uint8_t *dst)
{
    app_pc_link_encode_header(dst, type, 0u, sequence, payload, payload_length);
    if (payload_length > 0u)
    {
        (void)memcpy(&dst[APP_PC_LINK_HEADER_SIZE], payload, payload_length);
    }
    return (size_t)APP_PC_LINK_HEADER_SIZE + (size_t)payload_length;
}

static app_pc_link_frame_t decode_ok(const uint8_t *bytes, size_t size)
{
    app_pc_link_frame_t frame;
    if (app_pc_link_decode_frame(bytes, size, &frame) != APP_PC_LINK_STATUS_OK)
    {
        abort();
    }
    return frame;
}

static bsp_status_t fake_erase_start(uint32_t address, uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_flash_t *flash = (fake_flash_t *)user;
    if ((flash == NULL) || flash->busy ||
        (address >= FAKE_FLASH_BYTES) ||
        ((address % STORAGE_LAYOUT_W25Q_SECTOR_SIZE) != 0u))
    {
        return BSP_STATUS_ERROR;
    }
    if (flash->erase_count < (uint8_t)(sizeof(flash->erase_addresses) / sizeof(flash->erase_addresses[0])))
    {
        flash->erase_addresses[flash->erase_count] = address;
        flash->erase_count++;
    }
    (void)memset(&flash->flash[address], 0xFF, STORAGE_LAYOUT_W25Q_SECTOR_SIZE);
    flash->busy = true;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_program_start(uint32_t address,
                                       const void *src,
                                       size_t size,
                                       uint32_t now_ms,
                                       void *user)
{
    (void)now_ms;
    fake_flash_t *flash = (fake_flash_t *)user;
    if ((flash == NULL) || flash->busy || (src == NULL) || (size == 0u) ||
        (address >= FAKE_FLASH_BYTES) || (size > (FAKE_FLASH_BYTES - address)))
    {
        return BSP_STATUS_ERROR;
    }
    if (flash->fail_next_program)
    {
        flash->fail_next_program = false;
        return BSP_STATUS_ERROR;
    }
    if (flash->program_count < (uint8_t)(sizeof(flash->program_addresses) / sizeof(flash->program_addresses[0])))
    {
        flash->program_addresses[flash->program_count] = address;
        flash->program_sizes[flash->program_count] = (uint16_t)size;
        flash->program_count++;
    }
    (void)memcpy(&flash->flash[address], src, size);
    flash->busy = true;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_poll(uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_flash_t *flash = (fake_flash_t *)user;
    if ((flash == NULL) || !flash->busy)
    {
        return BSP_STATUS_OK;
    }
    flash->busy = false;
    return BSP_STATUS_OK;
}

static app_resource_update_t make_update(fake_flash_t *flash, uint8_t *scratch)
{
    (void)memset(flash->flash, 0x00, sizeof(flash->flash));
    const app_resource_update_io_t io = {
        .erase_start = fake_erase_start,
        .program_start = fake_program_start,
        .poll = fake_poll,
        .user = flash,
    };
    const storage_partition_t partition = {
        .id = STORAGE_PARTITION_RESOURCE_PACK,
        .start = 0u,
        .size = FAKE_FLASH_BYTES,
    };
    app_resource_update_t update;
    app_resource_update_init(&update, &io, &partition, scratch, APP_PC_LINK_MAX_PAYLOAD_BYTES);
    return update;
}

static void step_until_ready(app_resource_update_t *update)
{
    for (uint8_t i = 0u; i < 64u; i++)
    {
        const app_resource_update_status_t status = app_resource_update_step(update, i);
        if ((status == APP_RESOURCE_UPDATE_STATUS_OK) &&
            (app_resource_update_state(update) == APP_RESOURCE_UPDATE_READY))
        {
            return;
        }
    }
    abort();
}

static int send_begin(app_resource_update_t *update,
                      uint32_t pack_size,
                      uint32_t pack_crc,
                      uint8_t *frame_bytes)
{
    uint8_t payload[APP_PC_LINK_RESOURCE_BEGIN_SIZE] = {0};
    write_le32(&payload[0], pack_size);
    write_le32(&payload[4], pack_crc);
    write_le16(&payload[8], RESOURCE_PACK_API_VERSION);
    const size_t size =
        make_frame(APP_PC_LINK_FRAME_RESOURCE_BEGIN, 1u, payload, sizeof(payload), frame_bytes);
    const app_pc_link_frame_t frame = decode_ok(frame_bytes, size);
    return expect_true(app_resource_update_accept_frame(update, &frame, 0u) ==
                           APP_RESOURCE_UPDATE_STATUS_OK,
                       "begin accepted");
}

static int send_chunk(app_resource_update_t *update,
                      uint32_t offset,
                      const uint8_t *data,
                      uint16_t size,
                      uint16_t sequence,
                      uint8_t *frame_bytes)
{
    uint8_t payload[APP_PC_LINK_MAX_PAYLOAD_BYTES] = {0};
    write_le32(&payload[0], offset);
    write_le16(&payload[4], size);
    (void)memcpy(&payload[APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE], data, size);
    const size_t frame_size =
        make_frame(APP_PC_LINK_FRAME_RESOURCE_CHUNK,
                   sequence,
                   payload,
                   (uint16_t)(APP_PC_LINK_RESOURCE_CHUNK_HEADER_SIZE + size),
                   frame_bytes);
    const app_pc_link_frame_t frame = decode_ok(frame_bytes, frame_size);
    return expect_true(app_resource_update_accept_frame(update, &frame, 0u) ==
                           APP_RESOURCE_UPDATE_STATUS_OK,
                       "chunk accepted");
}

static int send_end(app_resource_update_t *update,
                    uint32_t pack_size,
                    uint32_t pack_crc,
                    uint8_t *frame_bytes)
{
    uint8_t payload[APP_PC_LINK_RESOURCE_END_SIZE] = {0};
    write_le32(&payload[0], pack_size);
    write_le32(&payload[4], pack_crc);
    const size_t size =
        make_frame(APP_PC_LINK_FRAME_RESOURCE_END, 9u, payload, sizeof(payload), frame_bytes);
    const app_pc_link_frame_t frame = decode_ok(frame_bytes, size);
    return expect_true(app_resource_update_accept_frame(update, &frame, 0u) ==
                           APP_RESOURCE_UPDATE_STATUS_COMPLETE,
                       "end accepted");
}

static int test_resource_update_erases_programs_and_completes(void)
{
    int failures = 0;
    fake_flash_t flash = {0};
    uint8_t scratch[APP_PC_LINK_MAX_PAYLOAD_BYTES];
    uint8_t frame_bytes[APP_PC_LINK_HEADER_SIZE + APP_PC_LINK_MAX_PAYLOAD_BYTES];
    app_resource_update_t update = make_update(&flash, scratch);
    uint8_t pack[280];
    for (uint16_t i = 0u; i < (uint16_t)sizeof(pack); i++)
    {
        pack[i] = (uint8_t)(i + 3u);
    }
    const uint32_t crc = storage_crc32(pack, sizeof(pack));

    failures += send_begin(&update, sizeof(pack), crc, frame_bytes);
    step_until_ready(&update);
    failures += expect_true(flash.erase_count == 1u, "one resource sector erased");

    failures += send_chunk(&update, 0u, &pack[0], 120u, 2u, frame_bytes);
    step_until_ready(&update);
    failures += send_chunk(&update, 120u, &pack[120], 120u, 3u, frame_bytes);
    step_until_ready(&update);
    failures += send_chunk(&update, 240u, &pack[240], 40u, 4u, frame_bytes);
    step_until_ready(&update);

    failures += expect_true(flash.program_count == 4u, "page boundary split produced four programs");
    failures += expect_true((flash.program_addresses[2] == 240u) && (flash.program_sizes[2] == 16u),
                            "third chunk first program stops at page boundary");
    failures += expect_true((flash.program_addresses[3] == 256u) && (flash.program_sizes[3] == 24u),
                            "third chunk second program resumes on next page");
    failures += expect_true(memcmp(flash.flash, pack, sizeof(pack)) == 0,
                            "programmed resource bytes match source");

    failures += send_end(&update, sizeof(pack), crc, frame_bytes);
    failures += expect_true(app_resource_update_complete(&update), "update reports complete");
    return failures;
}

static int test_resource_update_rejects_oversize_and_latches_flash_error(void)
{
    int failures = 0;
    fake_flash_t flash = {0};
    uint8_t scratch[APP_PC_LINK_MAX_PAYLOAD_BYTES];
    uint8_t frame_bytes[APP_PC_LINK_HEADER_SIZE + APP_PC_LINK_MAX_PAYLOAD_BYTES];
    app_resource_update_t update = make_update(&flash, scratch);

    uint8_t begin_payload[APP_PC_LINK_RESOURCE_BEGIN_SIZE] = {0};
    write_le32(&begin_payload[0], FAKE_FLASH_BYTES + 1u);
    write_le32(&begin_payload[4], 0u);
    write_le16(&begin_payload[8], RESOURCE_PACK_API_VERSION);
    const size_t begin_size =
        make_frame(APP_PC_LINK_FRAME_RESOURCE_BEGIN,
                   1u,
                   begin_payload,
                   sizeof(begin_payload),
                   frame_bytes);
    app_pc_link_frame_t frame = decode_ok(frame_bytes, begin_size);
    failures += expect_true(app_resource_update_accept_frame(&update, &frame, 0u) ==
                                APP_RESOURCE_UPDATE_STATUS_OUT_OF_RANGE,
                            "oversize begin rejected");
    failures += expect_true(app_resource_update_state(&update) == APP_RESOURCE_UPDATE_ERROR,
                            "oversize begin latches error");
    failures += expect_true(app_resource_update_last_error(&update) ==
                                APP_RESOURCE_UPDATE_STATUS_OUT_OF_RANGE,
                            "oversize error is out of range");

    update = make_update(&flash, scratch);
    const uint8_t pack[4] = {1u, 2u, 3u, 4u};
    const uint32_t crc = storage_crc32(pack, sizeof(pack));
    failures += send_begin(&update, sizeof(pack), crc, frame_bytes);
    step_until_ready(&update);
    flash.fail_next_program = true;
    failures += send_chunk(&update, 0u, pack, sizeof(pack), 2u, frame_bytes);
    failures += expect_true(app_resource_update_step(&update, 10u) ==
                                APP_RESOURCE_UPDATE_STATUS_FLASH,
                            "program failure surfaces");
    failures += expect_true(app_resource_update_state(&update) == APP_RESOURCE_UPDATE_ERROR,
                            "program failure latches error");
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_resource_update_erases_programs_and_completes();
    failures += test_resource_update_rejects_oversize_and_latches_flash_error();
    return failures;
}
