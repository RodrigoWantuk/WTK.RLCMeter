#include "storage/resource_store.h"

#include "storage/storage_crc32.h"
#include "ui/ui_text.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum
{
    PACK_BYTES = 512u,
};

static int g_failures = 0;
static uint8_t g_pack[PACK_BYTES];
static bool g_defer_next_read = false;

static void expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        g_failures++;
    }
}

static bsp_status_t mem_read(uint32_t address, void *dst, size_t size, void *user)
{
    const uint8_t *bytes = (const uint8_t *)user;
    if (g_defer_next_read)
    {
        g_defer_next_read = false;
        return BSP_STATUS_BUSY;
    }
    if ((bytes == NULL) || (dst == NULL) || (size > PACK_BYTES) || (address > (PACK_BYTES - size)))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    (void)memcpy(dst, &bytes[address], size);
    return BSP_STATUS_OK;
}

static void write_entry(uint32_t offset, resource_entry_t entry)
{
    resource_store_encode_entry(&g_pack[offset], &entry);
}

static uint32_t write_text_payload(uint32_t offset, uint8_t language, const char *a, const char *b)
{
    const uint32_t index_offset = RESOURCE_TEXT_TABLE_HEADER_SIZE;
    const uint32_t blob_offset = index_offset + (2u * RESOURCE_TEXT_TABLE_RECORD_SIZE);
    resource_text_record_t records[2] = {
        {.text_id = UI_TEXT_ID_READY, .byte_length = (uint16_t)strlen(a), .byte_offset = 0u},
        {.text_id = UI_TEXT_ID_BACK, .byte_length = (uint16_t)strlen(b), .byte_offset = (uint32_t)strlen(a)},
    };
    resource_text_table_header_t header = {
        .magic = RESOURCE_TEXT_TABLE_MAGIC,
        .version = RESOURCE_TEXT_TABLE_VERSION,
        .header_size = RESOURCE_TEXT_TABLE_HEADER_SIZE,
        .language_id = language,
        .record_count = 2u,
        .index_offset = index_offset,
        .blob_offset = blob_offset,
    };
    uint8_t index[2u * RESOURCE_TEXT_TABLE_RECORD_SIZE];
    resource_store_encode_text_record(&index[0], &records[0]);
    resource_store_encode_text_record(&index[RESOURCE_TEXT_TABLE_RECORD_SIZE], &records[1]);
    header.index_crc32 = storage_crc32(index, sizeof(index));
    resource_store_encode_text_header(&g_pack[offset], &header);
    (void)memcpy(&g_pack[offset + index_offset], index, sizeof(index));
    (void)memcpy(&g_pack[offset + blob_offset], a, strlen(a));
    (void)memcpy(&g_pack[offset + blob_offset + strlen(a)], b, strlen(b));
    return blob_offset + (uint32_t)strlen(a) + (uint32_t)strlen(b);
}

static void build_pack(void)
{
    (void)memset(g_pack, 0xFF, sizeof(g_pack));
    const uint32_t table_offset = RESOURCE_PACK_HEADER_SIZE;
    const uint32_t data_offset = table_offset + (2u * RESOURCE_PACK_ENTRY_WIRE_SIZE);
    const uint32_t en_offset = data_offset;
    const uint32_t en_size = write_text_payload(en_offset, (uint8_t)UI_LANGUAGE_EN, "READY", "BACK");
    const uint32_t pt_offset = en_offset + en_size;
    const uint32_t pt_size = write_text_payload(pt_offset, (uint8_t)UI_LANGUAGE_PT_BR, "PRONTO", "VOLTAR");
    resource_entry_t en = {
        .resource_id = RESOURCE_ID_TEXT_EN,
        .resource_type = RESOURCE_TYPE_TEXT_TABLE,
        .format = RESOURCE_FORMAT_TEXT_TABLE_UTF8_V1,
        .payload_offset = en_offset,
        .payload_size = en_size,
        .payload_crc32 = storage_crc32(&g_pack[en_offset], en_size),
    };
    resource_entry_t pt = {
        .resource_id = RESOURCE_ID_TEXT_PT_BR,
        .resource_type = RESOURCE_TYPE_TEXT_TABLE,
        .format = RESOURCE_FORMAT_TEXT_TABLE_UTF8_V1,
        .payload_offset = pt_offset,
        .payload_size = pt_size,
        .payload_crc32 = storage_crc32(&g_pack[pt_offset], pt_size),
    };
    write_entry(table_offset, en);
    write_entry(table_offset + RESOURCE_PACK_ENTRY_WIRE_SIZE, pt);
    resource_pack_header_t header = {
        .magic = RESOURCE_PACK_MAGIC,
        .schema_version = RESOURCE_PACK_SCHEMA_VERSION,
        .header_size = RESOURCE_PACK_HEADER_SIZE,
        .resource_api_version = RESOURCE_PACK_API_VERSION,
        .total_pack_size = pt_offset + pt_size,
        .entry_count = 2u,
        .entry_wire_size = RESOURCE_PACK_ENTRY_WIRE_SIZE,
        .entry_table_offset = table_offset,
        .data_offset = data_offset,
        .entry_table_crc32 = storage_crc32(&g_pack[table_offset], 2u * RESOURCE_PACK_ENTRY_WIRE_SIZE),
    };
    uint8_t header_bytes[RESOURCE_PACK_HEADER_SIZE];
    resource_store_encode_header(header_bytes, &header, false);
    header.header_crc32 = storage_crc32(header_bytes, sizeof(header_bytes));
    resource_store_encode_header(g_pack, &header, true);
}

static void test_catalog_mount_lookup_and_defer(void)
{
    build_pack();
    const resource_catalog_io_t io = {.read = mem_read, .user = g_pack};
    resource_catalog_t catalog;
    expect_true(resource_catalog_mount(&catalog, &io, 0u, PACK_BYTES) == RESOURCE_STATUS_OK,
                "valid v2 catalog mounts");
    resource_entry_t entry;
    expect_true(resource_catalog_lookup(&catalog, RESOURCE_ID_TEXT_PT_BR, &entry) == RESOURCE_STATUS_OK,
                "PT-BR table lookup");
    expect_true(resource_catalog_verify_payload(&catalog, &entry) == RESOURCE_STATUS_OK,
                "payload CRC verifies");
    g_defer_next_read = true;
    expect_true(resource_catalog_lookup(&catalog, RESOURCE_ID_TEXT_EN, &entry) == RESOURCE_STATUS_DEFERRED,
                "busy read maps to deferred");
}

static void test_header_and_entry_reject_bad_wire_values(void)
{
    build_pack();
    resource_pack_header_t header;
    expect_true(resource_store_decode_header(g_pack, PACK_BYTES, &header) == RESOURCE_STATUS_OK,
                "header decodes");
    uint8_t bad_header[RESOURCE_PACK_HEADER_SIZE];
    (void)memcpy(bad_header, g_pack, sizeof(bad_header));
    bad_header[6] = 0u;
    expect_true(resource_store_decode_header(bad_header, PACK_BYTES, &header) == RESOURCE_STATUS_INCOMPATIBLE_SCHEMA,
                "wrong header size rejected");
    uint8_t entry_bytes[RESOURCE_PACK_ENTRY_WIRE_SIZE];
    (void)memcpy(entry_bytes, &g_pack[RESOURCE_PACK_HEADER_SIZE], sizeof(entry_bytes));
    entry_bytes[0] = 0u;
    entry_bytes[1] = 0u;
    entry_bytes[2] = 0u;
    entry_bytes[3] = 0u;
    resource_entry_t entry;
    expect_true(resource_store_decode_entry(entry_bytes, &header, PACK_BYTES, &entry) == RESOURCE_STATUS_CORRUPT,
                "zero resource id rejected");
}

int main(void)
{
    test_catalog_mount_lookup_and_defer();
    test_header_and_entry_reject_bad_wire_values();
    return (g_failures == 0) ? 0 : 1;
}
