#include "ui/ui_text_catalog.h"

#include <string.h>

#include "ui/ui_utf8.h"

static resource_status_t read_table(const ui_text_catalog_t *text,
                                    uint32_t offset,
                                    void *dst,
                                    size_t size)
{
    return resource_catalog_read(text->catalog, &text->entry, offset, dst, size);
}

static resource_status_t read_record(const ui_text_catalog_t *text,
                                     uint16_t index,
                                     resource_text_record_t *record,
                                     uint8_t bytes[RESOURCE_TEXT_TABLE_RECORD_SIZE])
{
    const uint32_t offset = text->table.index_offset +
                            ((uint32_t)index * RESOURCE_TEXT_TABLE_RECORD_SIZE);
    resource_status_t status = read_table(text, offset, bytes, RESOURCE_TEXT_TABLE_RECORD_SIZE);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    return resource_store_decode_text_record(bytes, &text->table, text->entry.payload_size, record);
}

static resource_status_t lookup_record(const ui_text_catalog_t *text,
                                       ui_text_id_t id,
                                       resource_text_record_t *record)
{
    uint16_t lo = 0u;
    uint16_t hi = text->table.record_count;
    uint8_t bytes[RESOURCE_TEXT_TABLE_RECORD_SIZE];
    while (lo < hi)
    {
        const uint16_t mid = (uint16_t)(lo + ((hi - lo) / 2u));
        resource_text_record_t current;
        resource_status_t status = read_record(text, mid, &current, bytes);
        if (status != RESOURCE_STATUS_OK)
        {
            return status;
        }
        if (current.text_id == (uint16_t)id)
        {
            *record = current;
            return RESOURCE_STATUS_OK;
        }
        if (current.text_id < (uint16_t)id)
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

void ui_text_catalog_init(ui_text_catalog_t *text)
{
    if (text != NULL)
    {
        *text = (ui_text_catalog_t){0};
    }
}

resource_status_t ui_text_catalog_select_language(ui_text_catalog_t *text,
                                                  resource_catalog_t *catalog,
                                                  uint8_t language_id)
{
    if ((text == NULL) || (catalog == NULL) || !ui_language_valid(language_id))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    *text = (ui_text_catalog_t){
        .catalog = catalog,
        .language_id = language_id,
    };
    resource_status_t status =
        resource_catalog_lookup(catalog, ui_language_text_resource_id(language_id), &text->entry);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    if ((text->entry.resource_type != RESOURCE_TYPE_TEXT_TABLE) ||
        (text->entry.format != RESOURCE_FORMAT_TEXT_TABLE_UTF8_V1))
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    status = resource_catalog_verify_payload(catalog, &text->entry);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    uint8_t header_bytes[RESOURCE_TEXT_TABLE_HEADER_SIZE];
    status = read_table(text, 0u, header_bytes, sizeof(header_bytes));
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    status = resource_store_decode_text_header(header_bytes, text->entry.payload_size, &text->table);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    if (text->table.language_id != language_id)
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    text->ready = true;
    return RESOURCE_STATUS_OK;
}

resource_status_t ui_text_catalog_resolve(ui_text_catalog_t *text,
                                          ui_text_id_t id,
                                          char *dst,
                                          size_t capacity)
{
    if ((text == NULL) || !text->ready || (dst == NULL) || (capacity == 0u))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    resource_text_record_t record;
    resource_status_t status = lookup_record(text, id, &record);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    if ((size_t)record.byte_length >= capacity)
    {
        return RESOURCE_STATUS_OUT_OF_RANGE;
    }
    status = read_table(text,
                        text->table.blob_offset + record.byte_offset,
                        dst,
                        record.byte_length);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    dst[record.byte_length] = '\0';
    if (!ui_utf8_validate(dst, record.byte_length))
    {
        dst[0] = '\0';
        return RESOURCE_STATUS_INVALID_UTF8;
    }
    return RESOURCE_STATUS_OK;
}

uint8_t ui_text_catalog_language(const ui_text_catalog_t *text)
{
    return (text == NULL) ? 0u : text->language_id;
}

bool ui_text_catalog_ready(const ui_text_catalog_t *text)
{
    return (text != NULL) && text->ready;
}

uint32_t ui_text_catalog_context_size_bytes(void)
{
    return (uint32_t)sizeof(ui_text_catalog_t);
}
