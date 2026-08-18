#ifndef WTK_RESOURCE_STORE_H
#define WTK_RESOURCE_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    RESOURCE_STORE_MAGIC = 0x574B5253u,
    RESOURCE_STORE_SCHEMA_VERSION = 1u,
};

typedef enum
{
    RESOURCE_FORMAT_RAW_RGB565 = 1,
    RESOURCE_FORMAT_GLYPH_A1 = 2,
    RESOURCE_FORMAT_GLYPH_A4 = 3,
} resource_format_t;

typedef struct
{
    uint32_t id;
    uint32_t offset;
    uint32_t size;
    uint16_t width;
    uint16_t height;
    uint16_t format;
    uint16_t flags;
    uint32_t metrics_offset;
    uint32_t metrics_size;
    uint32_t crc32;
} resource_entry_t;

typedef struct
{
    uint32_t magic;
    uint16_t schema_version;
    uint16_t entry_count;
    uint32_t table_offset;
    uint32_t blob_offset;
} resource_pack_header_t;

bool resource_pack_header_valid(const resource_pack_header_t *header, uint32_t capacity_bytes);
bool resource_entry_valid(const resource_pack_header_t *header,
                          const resource_entry_t *entry,
                          uint32_t capacity_bytes);

#endif
