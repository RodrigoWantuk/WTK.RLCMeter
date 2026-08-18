#include "storage/resource_store.h"

bool resource_pack_header_valid(const resource_pack_header_t *header, uint32_t capacity_bytes)
{
    if ((header == NULL) || (capacity_bytes == 0u))
    {
        return false;
    }

    if ((header->magic != RESOURCE_STORE_MAGIC) ||
        (header->schema_version != RESOURCE_STORE_SCHEMA_VERSION) ||
        (header->entry_count == 0u))
    {
        return false;
    }

    if ((header->table_offset >= capacity_bytes) || (header->blob_offset >= capacity_bytes))
    {
        return false;
    }

    const uint32_t table_bytes = (uint32_t)header->entry_count * (uint32_t)sizeof(resource_entry_t);
    return table_bytes <= (capacity_bytes - header->table_offset);
}

bool resource_entry_valid(const resource_pack_header_t *header,
                          const resource_entry_t *entry,
                          uint32_t capacity_bytes)
{
    if ((header == NULL) || (entry == NULL) || !resource_pack_header_valid(header, capacity_bytes))
    {
        return false;
    }

    if ((entry->size == 0u) || (entry->offset < header->blob_offset) || (entry->offset >= capacity_bytes))
    {
        return false;
    }

    if (entry->size > (capacity_bytes - entry->offset))
    {
        return false;
    }

    if (entry->metrics_size == 0u)
    {
        return true;
    }

    if ((entry->metrics_offset < header->blob_offset) || (entry->metrics_offset >= capacity_bytes))
    {
        return false;
    }

    return entry->metrics_size <= (capacity_bytes - entry->metrics_offset);
}
