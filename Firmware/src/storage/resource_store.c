#include "storage/resource_store.h"

#include <string.h>

#include "storage/storage_crc32.h"

enum
{
    RESOURCE_HEADER_MAGIC_OFFSET = 0u,
    RESOURCE_HEADER_SCHEMA_OFFSET = 4u,
    RESOURCE_HEADER_HEADER_SIZE_OFFSET = 6u,
    RESOURCE_HEADER_API_OFFSET = 8u,
    RESOURCE_HEADER_FLAGS_OFFSET = 10u,
    RESOURCE_HEADER_TOTAL_SIZE_OFFSET = 12u,
    RESOURCE_HEADER_ENTRY_COUNT_OFFSET = 16u,
    RESOURCE_HEADER_ENTRY_SIZE_OFFSET = 18u,
    RESOURCE_HEADER_TABLE_OFFSET = 20u,
    RESOURCE_HEADER_DATA_OFFSET = 24u,
    RESOURCE_HEADER_TABLE_CRC_OFFSET = 28u,
    RESOURCE_HEADER_CRC_OFFSET = 32u,
    RESOURCE_HEADER_RESERVED0_OFFSET = 36u,
    RESOURCE_HEADER_RESERVED1_OFFSET = 40u,
};

static void put_u16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8u) & 0xFFu);
}

static void put_u32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8u) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16u) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24u) & 0xFFu);
}

static uint16_t get_u16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8u));
}

static uint32_t get_u32(const uint8_t *src)
{
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8u) |
           ((uint32_t)src[2] << 16u) |
           ((uint32_t)src[3] << 24u);
}

static bool span_valid(uint32_t total, uint32_t offset, uint32_t size)
{
    return (size != 0u) && (offset < total) && (size <= (total - offset));
}

static bool span_valid_allow_zero(uint32_t total, uint32_t offset, uint32_t size)
{
    return (size == 0u) ? (offset == 0u) : span_valid(total, offset, size);
}

static resource_status_t status_from_bsp(bsp_status_t status)
{
    switch (status)
    {
    case BSP_STATUS_OK:
        return RESOURCE_STATUS_OK;
    case BSP_STATUS_BUSY:
        return RESOURCE_STATUS_DEFERRED;
    case BSP_STATUS_NOT_SUPPORTED:
        return RESOURCE_STATUS_MISSING;
    case BSP_STATUS_INVALID_ARG:
        return RESOURCE_STATUS_INVALID_ARG;
    case BSP_STATUS_TIMEOUT:
    case BSP_STATUS_ERROR:
    default:
        return RESOURCE_STATUS_CORRUPT;
    }
}

void resource_store_encode_header(uint8_t dst[RESOURCE_PACK_HEADER_SIZE],
                                  const resource_pack_header_t *header,
                                  bool include_crc)
{
    if ((dst == NULL) || (header == NULL))
    {
        return;
    }
    memset(dst, 0, RESOURCE_PACK_HEADER_SIZE);
    put_u32(&dst[RESOURCE_HEADER_MAGIC_OFFSET], header->magic);
    put_u16(&dst[RESOURCE_HEADER_SCHEMA_OFFSET], header->schema_version);
    put_u16(&dst[RESOURCE_HEADER_HEADER_SIZE_OFFSET], header->header_size);
    put_u16(&dst[RESOURCE_HEADER_API_OFFSET], header->resource_api_version);
    put_u16(&dst[RESOURCE_HEADER_FLAGS_OFFSET], header->flags);
    put_u32(&dst[RESOURCE_HEADER_TOTAL_SIZE_OFFSET], header->total_pack_size);
    put_u16(&dst[RESOURCE_HEADER_ENTRY_COUNT_OFFSET], header->entry_count);
    put_u16(&dst[RESOURCE_HEADER_ENTRY_SIZE_OFFSET], header->entry_wire_size);
    put_u32(&dst[RESOURCE_HEADER_TABLE_OFFSET], header->entry_table_offset);
    put_u32(&dst[RESOURCE_HEADER_DATA_OFFSET], header->data_offset);
    put_u32(&dst[RESOURCE_HEADER_TABLE_CRC_OFFSET], header->entry_table_crc32);
    put_u32(&dst[RESOURCE_HEADER_CRC_OFFSET], include_crc ? header->header_crc32 : 0u);
    put_u32(&dst[RESOURCE_HEADER_RESERVED0_OFFSET], header->reserved0);
    put_u32(&dst[RESOURCE_HEADER_RESERVED1_OFFSET], header->reserved1);
}

resource_status_t resource_store_decode_header(const uint8_t src[RESOURCE_PACK_HEADER_SIZE],
                                               uint32_t partition_size,
                                               resource_pack_header_t *header)
{
    if ((src == NULL) || (header == NULL) || (partition_size == 0u))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    *header = (resource_pack_header_t){
        .magic = get_u32(&src[RESOURCE_HEADER_MAGIC_OFFSET]),
        .schema_version = get_u16(&src[RESOURCE_HEADER_SCHEMA_OFFSET]),
        .header_size = get_u16(&src[RESOURCE_HEADER_HEADER_SIZE_OFFSET]),
        .resource_api_version = get_u16(&src[RESOURCE_HEADER_API_OFFSET]),
        .flags = get_u16(&src[RESOURCE_HEADER_FLAGS_OFFSET]),
        .total_pack_size = get_u32(&src[RESOURCE_HEADER_TOTAL_SIZE_OFFSET]),
        .entry_count = get_u16(&src[RESOURCE_HEADER_ENTRY_COUNT_OFFSET]),
        .entry_wire_size = get_u16(&src[RESOURCE_HEADER_ENTRY_SIZE_OFFSET]),
        .entry_table_offset = get_u32(&src[RESOURCE_HEADER_TABLE_OFFSET]),
        .data_offset = get_u32(&src[RESOURCE_HEADER_DATA_OFFSET]),
        .entry_table_crc32 = get_u32(&src[RESOURCE_HEADER_TABLE_CRC_OFFSET]),
        .header_crc32 = get_u32(&src[RESOURCE_HEADER_CRC_OFFSET]),
        .reserved0 = get_u32(&src[RESOURCE_HEADER_RESERVED0_OFFSET]),
        .reserved1 = get_u32(&src[RESOURCE_HEADER_RESERVED1_OFFSET]),
    };
    if (header->magic != RESOURCE_PACK_MAGIC)
    {
        return RESOURCE_STATUS_MISSING;
    }
    if ((header->schema_version != RESOURCE_PACK_SCHEMA_VERSION) ||
        (header->header_size != RESOURCE_PACK_HEADER_SIZE) ||
        (header->entry_wire_size != RESOURCE_PACK_ENTRY_WIRE_SIZE))
    {
        return RESOURCE_STATUS_INCOMPATIBLE_SCHEMA;
    }
    if (header->resource_api_version != RESOURCE_PACK_API_VERSION)
    {
        return RESOURCE_STATUS_INCOMPATIBLE_API;
    }
    if ((header->flags != 0u) || (header->reserved0 != 0u) || (header->reserved1 != 0u) ||
        (header->entry_count == 0u) || (header->total_pack_size > partition_size) ||
        (header->total_pack_size < RESOURCE_PACK_HEADER_SIZE))
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    const uint32_t table_bytes =
        (uint32_t)header->entry_count * (uint32_t)RESOURCE_PACK_ENTRY_WIRE_SIZE;
    if (!span_valid(header->total_pack_size, header->entry_table_offset, table_bytes) ||
        !span_valid(header->total_pack_size, header->data_offset, 1u) ||
        (header->entry_table_offset < RESOURCE_PACK_HEADER_SIZE) ||
        (header->data_offset < (header->entry_table_offset + table_bytes)))
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    uint8_t tmp[RESOURCE_PACK_HEADER_SIZE];
    memcpy(tmp, src, sizeof(tmp));
    put_u32(&tmp[RESOURCE_HEADER_CRC_OFFSET], 0u);
    if (storage_crc32(tmp, sizeof(tmp)) != header->header_crc32)
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    return RESOURCE_STATUS_OK;
}

void resource_store_encode_entry(uint8_t dst[RESOURCE_PACK_ENTRY_WIRE_SIZE],
                                 const resource_entry_t *entry)
{
    if ((dst == NULL) || (entry == NULL))
    {
        return;
    }
    memset(dst, 0, RESOURCE_PACK_ENTRY_WIRE_SIZE);
    put_u32(&dst[0], entry->resource_id);
    put_u16(&dst[4], entry->resource_type);
    put_u16(&dst[6], entry->format);
    put_u32(&dst[8], entry->flags);
    put_u32(&dst[12], entry->payload_offset);
    put_u32(&dst[16], entry->payload_size);
    put_u32(&dst[20], entry->payload_crc32);
    put_u32(&dst[24], entry->aux_offset);
    put_u32(&dst[28], entry->aux_size);
}

resource_status_t resource_store_decode_entry(const uint8_t src[RESOURCE_PACK_ENTRY_WIRE_SIZE],
                                              const resource_pack_header_t *header,
                                              uint32_t partition_size,
                                              resource_entry_t *entry)
{
    if ((src == NULL) || (header == NULL) || (entry == NULL))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    *entry = (resource_entry_t){
        .resource_id = get_u32(&src[0]),
        .resource_type = get_u16(&src[4]),
        .format = get_u16(&src[6]),
        .flags = get_u32(&src[8]),
        .payload_offset = get_u32(&src[12]),
        .payload_size = get_u32(&src[16]),
        .payload_crc32 = get_u32(&src[20]),
        .aux_offset = get_u32(&src[24]),
        .aux_size = get_u32(&src[28]),
    };
    if ((partition_size == 0u) || (header->total_pack_size > partition_size) ||
        (entry->resource_id == 0u) || (entry->flags != 0u) ||
        !span_valid(header->total_pack_size, entry->payload_offset, entry->payload_size) ||
        (entry->payload_offset < header->data_offset) ||
        !span_valid_allow_zero(header->total_pack_size, entry->aux_offset, entry->aux_size))
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    return RESOURCE_STATUS_OK;
}

void resource_store_encode_text_header(uint8_t dst[RESOURCE_TEXT_TABLE_HEADER_SIZE],
                                       const resource_text_table_header_t *header)
{
    if ((dst == NULL) || (header == NULL))
    {
        return;
    }
    memset(dst, 0, RESOURCE_TEXT_TABLE_HEADER_SIZE);
    put_u32(&dst[0], header->magic);
    put_u16(&dst[4], header->version);
    put_u16(&dst[6], header->header_size);
    dst[8] = header->language_id;
    dst[9] = header->reserved0;
    put_u16(&dst[10], header->record_count);
    put_u32(&dst[12], header->index_offset);
    put_u32(&dst[16], header->blob_offset);
    put_u32(&dst[20], header->index_crc32);
}

resource_status_t resource_store_decode_text_header(const uint8_t src[RESOURCE_TEXT_TABLE_HEADER_SIZE],
                                                    uint32_t payload_size,
                                                    resource_text_table_header_t *header)
{
    if ((src == NULL) || (header == NULL))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    *header = (resource_text_table_header_t){
        .magic = get_u32(&src[0]),
        .version = get_u16(&src[4]),
        .header_size = get_u16(&src[6]),
        .language_id = src[8],
        .reserved0 = src[9],
        .record_count = get_u16(&src[10]),
        .index_offset = get_u32(&src[12]),
        .blob_offset = get_u32(&src[16]),
        .index_crc32 = get_u32(&src[20]),
    };
    const uint32_t index_bytes =
        (uint32_t)header->record_count * (uint32_t)RESOURCE_TEXT_TABLE_RECORD_SIZE;
    if ((header->magic != RESOURCE_TEXT_TABLE_MAGIC) ||
        (header->version != RESOURCE_TEXT_TABLE_VERSION) ||
        (header->header_size != RESOURCE_TEXT_TABLE_HEADER_SIZE) ||
        (header->reserved0 != 0u) ||
        (header->record_count == 0u) ||
        !span_valid(payload_size, header->index_offset, index_bytes) ||
        !span_valid(payload_size, header->blob_offset, 1u) ||
        (header->index_offset < RESOURCE_TEXT_TABLE_HEADER_SIZE) ||
        (header->blob_offset < (header->index_offset + index_bytes)))
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    return RESOURCE_STATUS_OK;
}

void resource_store_encode_text_record(uint8_t dst[RESOURCE_TEXT_TABLE_RECORD_SIZE],
                                       const resource_text_record_t *record)
{
    if ((dst == NULL) || (record == NULL))
    {
        return;
    }
    put_u16(&dst[0], record->text_id);
    put_u16(&dst[2], record->byte_length);
    put_u32(&dst[4], record->byte_offset);
}

resource_status_t resource_store_decode_text_record(const uint8_t src[RESOURCE_TEXT_TABLE_RECORD_SIZE],
                                                    const resource_text_table_header_t *header,
                                                    uint32_t payload_size,
                                                    resource_text_record_t *record)
{
    if ((src == NULL) || (header == NULL) || (record == NULL))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    *record = (resource_text_record_t){
        .text_id = get_u16(&src[0]),
        .byte_length = get_u16(&src[2]),
        .byte_offset = get_u32(&src[4]),
    };
    const uint32_t blob_size = payload_size - header->blob_offset;
    if ((record->text_id == 0u) ||
        (record->byte_length == 0u) ||
        !span_valid(blob_size, record->byte_offset, record->byte_length))
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    return RESOURCE_STATUS_OK;
}

resource_status_t resource_catalog_mount(resource_catalog_t *catalog,
                                         const resource_catalog_io_t *io,
                                         uint32_t partition_start,
                                         uint32_t partition_size)
{
    if ((catalog == NULL) || (io == NULL) || (io->read == NULL) ||
        (partition_size < RESOURCE_PACK_HEADER_SIZE))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    *catalog = (resource_catalog_t){0};
    catalog->io = *io;
    catalog->partition_start = partition_start;
    catalog->partition_size = partition_size;
    uint8_t header_bytes[RESOURCE_PACK_HEADER_SIZE];
    resource_status_t status =
        status_from_bsp(io->read(partition_start, header_bytes, sizeof(header_bytes), io->user));
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    status = resource_store_decode_header(header_bytes, partition_size, &catalog->header);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    uint8_t entry_bytes[RESOURCE_PACK_ENTRY_WIRE_SIZE];
    uint32_t crc = STORAGE_CRC32_INIT;
    uint32_t previous_id = 0u;
    for (uint16_t i = 0u; i < catalog->header.entry_count; i++)
    {
        const uint32_t offset = catalog->header.entry_table_offset +
                                ((uint32_t)i * RESOURCE_PACK_ENTRY_WIRE_SIZE);
        status = status_from_bsp(io->read(partition_start + offset,
                                          entry_bytes,
                                          sizeof(entry_bytes),
                                          io->user));
        if (status != RESOURCE_STATUS_OK)
        {
            return status;
        }
        crc = storage_crc32_update(crc, entry_bytes, sizeof(entry_bytes));
        resource_entry_t entry;
        status = resource_store_decode_entry(entry_bytes, &catalog->header, partition_size, &entry);
        if (status != RESOURCE_STATUS_OK)
        {
            return status;
        }
        if ((i != 0u) && (entry.resource_id <= previous_id))
        {
            return RESOURCE_STATUS_CORRUPT;
        }
        previous_id = entry.resource_id;
    }
    if ((crc ^ STORAGE_CRC32_XOR_OUT) != catalog->header.entry_table_crc32)
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    catalog->mounted = true;
    return RESOURCE_STATUS_OK;
}

resource_status_t resource_catalog_lookup(resource_catalog_t *catalog,
                                          uint32_t resource_id,
                                          resource_entry_t *entry)
{
    if ((catalog == NULL) || !catalog->mounted || (entry == NULL) || (resource_id == 0u))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    uint16_t lo = 0u;
    uint16_t hi = catalog->header.entry_count;
    uint8_t bytes[RESOURCE_PACK_ENTRY_WIRE_SIZE];
    while (lo < hi)
    {
        const uint16_t mid = (uint16_t)(lo + ((hi - lo) / 2u));
        const uint32_t offset = catalog->header.entry_table_offset +
                                ((uint32_t)mid * RESOURCE_PACK_ENTRY_WIRE_SIZE);
        resource_status_t status =
            status_from_bsp(catalog->io.read(catalog->partition_start + offset,
                                             bytes,
                                             sizeof(bytes),
                                             catalog->io.user));
        if (status != RESOURCE_STATUS_OK)
        {
            return status;
        }
        resource_entry_t decoded;
        status = resource_store_decode_entry(bytes, &catalog->header, catalog->partition_size, &decoded);
        if (status != RESOURCE_STATUS_OK)
        {
            return status;
        }
        if (decoded.resource_id == resource_id)
        {
            *entry = decoded;
            return RESOURCE_STATUS_OK;
        }
        if (decoded.resource_id < resource_id)
        {
            lo = (uint16_t)(mid + 1u);
        }
        else
        {
            hi = mid;
        }
    }
    return RESOURCE_STATUS_NOT_FOUND;
}

resource_status_t resource_catalog_read(const resource_catalog_t *catalog,
                                        const resource_entry_t *entry,
                                        uint32_t offset,
                                        void *dst,
                                        size_t size)
{
    if ((catalog == NULL) || !catalog->mounted || (entry == NULL) ||
        (dst == NULL) || (size == 0u))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    if ((offset >= entry->payload_size) || ((uint32_t)size > (entry->payload_size - offset)))
    {
        return RESOURCE_STATUS_OUT_OF_RANGE;
    }
    return status_from_bsp(catalog->io.read(catalog->partition_start + entry->payload_offset + offset,
                                            dst,
                                            size,
                                            catalog->io.user));
}

resource_status_t resource_catalog_verify_payload(const resource_catalog_t *catalog,
                                                  const resource_entry_t *entry)
{
    if ((catalog == NULL) || !catalog->mounted || (entry == NULL))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    uint8_t scratch[32];
    uint32_t crc = STORAGE_CRC32_INIT;
    uint32_t offset = 0u;
    while (offset < entry->payload_size)
    {
        uint32_t chunk = entry->payload_size - offset;
        if (chunk > sizeof(scratch))
        {
            chunk = sizeof(scratch);
        }
        const resource_status_t status = resource_catalog_read(catalog, entry, offset, scratch, chunk);
        if (status != RESOURCE_STATUS_OK)
        {
            return status;
        }
        crc = storage_crc32_update(crc, scratch, chunk);
        offset += chunk;
    }
    return ((crc ^ STORAGE_CRC32_XOR_OUT) == entry->payload_crc32) ?
               RESOURCE_STATUS_OK :
               RESOURCE_STATUS_CORRUPT;
}

const char *resource_status_string(resource_status_t status)
{
    switch (status)
    {
    case RESOURCE_STATUS_OK:
        return "OK";
    case RESOURCE_STATUS_DEFERRED:
        return "DEFERRED";
    case RESOURCE_STATUS_MISSING:
        return "MISSING";
    case RESOURCE_STATUS_CORRUPT:
        return "CORRUPT";
    case RESOURCE_STATUS_INCOMPATIBLE_SCHEMA:
        return "INCOMPATIBLE_SCHEMA";
    case RESOURCE_STATUS_INCOMPATIBLE_API:
        return "INCOMPATIBLE_API";
    case RESOURCE_STATUS_INVALID_ARG:
        return "INVALID_ARG";
    case RESOURCE_STATUS_NOT_FOUND:
        return "NOT_FOUND";
    case RESOURCE_STATUS_OUT_OF_RANGE:
        return "OUT_OF_RANGE";
    case RESOURCE_STATUS_INVALID_UTF8:
        return "INVALID_UTF8";
    default:
        return "ERROR";
    }
}

uint32_t resource_catalog_context_size_bytes(void)
{
    return (uint32_t)sizeof(resource_catalog_t);
}
