#ifndef WTK_RESOURCE_STORE_H
#define WTK_RESOURCE_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_status.h"

enum
{
    RESOURCE_PACK_MAGIC = 0x32505257u, /* "WRP2" little-endian */
    RESOURCE_PACK_SCHEMA_VERSION = 2u,
    RESOURCE_PACK_API_VERSION = 1u,
    RESOURCE_PACK_HEADER_SIZE = 44u,
    RESOURCE_PACK_ENTRY_WIRE_SIZE = 32u,
    RESOURCE_TEXT_TABLE_HEADER_SIZE = 24u,
    RESOURCE_TEXT_TABLE_RECORD_SIZE = 8u,
    RESOURCE_TEXT_TABLE_MAGIC = 0x54585457u, /* "WTXT" little-endian */
    RESOURCE_TEXT_TABLE_VERSION = 1u,
};

typedef enum
{
    RESOURCE_STATUS_OK = 0,
    RESOURCE_STATUS_DEFERRED,
    RESOURCE_STATUS_MISSING,
    RESOURCE_STATUS_CORRUPT,
    RESOURCE_STATUS_INCOMPATIBLE_SCHEMA,
    RESOURCE_STATUS_INCOMPATIBLE_API,
    RESOURCE_STATUS_INVALID_ARG,
    RESOURCE_STATUS_NOT_FOUND,
    RESOURCE_STATUS_OUT_OF_RANGE,
    RESOURCE_STATUS_INVALID_UTF8,
} resource_status_t;

typedef enum
{
    RESOURCE_TYPE_TEXT_TABLE = 1u,
    RESOURCE_TYPE_FONT_BITMAP_A1 = 2u,
    RESOURCE_TYPE_FONT_BITMAP_A4 = 3u,
    RESOURCE_TYPE_RGB565_IMAGE = 4u,
} resource_type_t;

typedef enum
{
    RESOURCE_FORMAT_TEXT_TABLE_UTF8_V1 = 1u,
} resource_format_t;

typedef enum
{
    UI_LANGUAGE_EN = 1u,
    UI_LANGUAGE_PT_BR = 2u,
} ui_language_id_t;

typedef enum
{
    RESOURCE_ID_TEXT_EN = 0x00010001u,
    RESOURCE_ID_TEXT_PT_BR = 0x00010002u,
} resource_id_t;

typedef struct
{
    uint32_t magic;
    uint16_t schema_version;
    uint16_t header_size;
    uint16_t resource_api_version;
    uint16_t flags;
    uint32_t total_pack_size;
    uint16_t entry_count;
    uint16_t entry_wire_size;
    uint32_t entry_table_offset;
    uint32_t data_offset;
    uint32_t entry_table_crc32;
    uint32_t header_crc32;
    uint32_t reserved0;
    uint32_t reserved1;
} resource_pack_header_t;

typedef struct
{
    uint32_t resource_id;
    uint16_t resource_type;
    uint16_t format;
    uint32_t flags;
    uint32_t payload_offset;
    uint32_t payload_size;
    uint32_t payload_crc32;
    uint32_t aux_offset;
    uint32_t aux_size;
} resource_entry_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint8_t language_id;
    uint8_t reserved0;
    uint16_t record_count;
    uint32_t index_offset;
    uint32_t blob_offset;
    uint32_t index_crc32;
} resource_text_table_header_t;

typedef struct
{
    uint16_t text_id;
    uint16_t byte_length;
    uint32_t byte_offset;
} resource_text_record_t;

typedef bsp_status_t (*resource_read_fn)(uint32_t address, void *dst, size_t size, void *user);

typedef struct
{
    resource_read_fn read;
    void *user;
} resource_catalog_io_t;

typedef struct
{
    resource_catalog_io_t io;
    uint32_t partition_start;
    uint32_t partition_size;
    resource_pack_header_t header;
    bool mounted;
} resource_catalog_t;

void resource_store_encode_header(uint8_t dst[RESOURCE_PACK_HEADER_SIZE],
                                  const resource_pack_header_t *header,
                                  bool include_crc);
resource_status_t resource_store_decode_header(const uint8_t src[RESOURCE_PACK_HEADER_SIZE],
                                               uint32_t partition_size,
                                               resource_pack_header_t *header);
void resource_store_encode_entry(uint8_t dst[RESOURCE_PACK_ENTRY_WIRE_SIZE],
                                 const resource_entry_t *entry);
resource_status_t resource_store_decode_entry(const uint8_t src[RESOURCE_PACK_ENTRY_WIRE_SIZE],
                                              const resource_pack_header_t *header,
                                              uint32_t partition_size,
                                              resource_entry_t *entry);
void resource_store_encode_text_header(uint8_t dst[RESOURCE_TEXT_TABLE_HEADER_SIZE],
                                       const resource_text_table_header_t *header);
resource_status_t resource_store_decode_text_header(const uint8_t src[RESOURCE_TEXT_TABLE_HEADER_SIZE],
                                                    uint32_t payload_size,
                                                    resource_text_table_header_t *header);
void resource_store_encode_text_record(uint8_t dst[RESOURCE_TEXT_TABLE_RECORD_SIZE],
                                       const resource_text_record_t *record);
resource_status_t resource_store_decode_text_record(const uint8_t src[RESOURCE_TEXT_TABLE_RECORD_SIZE],
                                                    const resource_text_table_header_t *header,
                                                    uint32_t payload_size,
                                                    resource_text_record_t *record);

resource_status_t resource_catalog_mount(resource_catalog_t *catalog,
                                         const resource_catalog_io_t *io,
                                         uint32_t partition_start,
                                         uint32_t partition_size);
resource_status_t resource_catalog_lookup(resource_catalog_t *catalog,
                                          uint32_t resource_id,
                                          resource_entry_t *entry);
resource_status_t resource_catalog_read(const resource_catalog_t *catalog,
                                        const resource_entry_t *entry,
                                        uint32_t offset,
                                        void *dst,
                                        size_t size);
resource_status_t resource_catalog_verify_payload(const resource_catalog_t *catalog,
                                                  const resource_entry_t *entry);
const char *resource_status_string(resource_status_t status);
uint32_t resource_catalog_context_size_bytes(void);

#endif
