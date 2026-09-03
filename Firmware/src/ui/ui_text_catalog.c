#include "ui/ui_text_catalog.h"

#include <string.h>

#include "storage/storage_crc32.h"
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

static resource_status_t text_entry_lookup(resource_catalog_t *catalog,
                                           uint8_t language_id,
                                           resource_entry_t *entry)
{
    if ((catalog == NULL) || (entry == NULL) || !ui_language_valid(language_id))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    resource_status_t status =
        resource_catalog_lookup(catalog, ui_language_text_resource_id(language_id), entry);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    return ((entry->resource_type == RESOURCE_TYPE_TEXT_TABLE) &&
            (entry->format == RESOURCE_FORMAT_TEXT_TABLE_UTF8_V1)) ?
               RESOURCE_STATUS_OK :
               RESOURCE_STATUS_CORRUPT;
}

static resource_status_t validate_record_text(const ui_text_catalog_t *text,
                                              const resource_text_record_t *record)
{
    if ((text == NULL) || (record == NULL))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    if (record->byte_length > UI_TEXT_MAX_BYTES)
    {
        return RESOURCE_STATUS_OUT_OF_RANGE;
    }
    char bytes[UI_TEXT_MAX_BYTES + 1u];
    resource_status_t status = read_table(text,
                                          text->table.blob_offset + record->byte_offset,
                                          bytes,
                                          record->byte_length);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    bytes[record->byte_length] = '\0';
    return ui_utf8_validate(bytes, record->byte_length) ? RESOURCE_STATUS_OK :
                                                          RESOURCE_STATUS_INVALID_UTF8;
}

static resource_status_t validate_text_table(ui_text_catalog_t *text)
{
    if ((text == NULL) || (text->catalog == NULL))
    {
        return RESOURCE_STATUS_INVALID_ARG;
    }
    resource_status_t status = resource_catalog_verify_payload(text->catalog, &text->entry);
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
    const uint32_t expected_blob_offset =
        RESOURCE_TEXT_TABLE_HEADER_SIZE +
        ((uint32_t)UI_TEXT_ID_COUNT * RESOURCE_TEXT_TABLE_RECORD_SIZE);
    if ((text->table.language_id != text->language_id) ||
        (text->table.record_count != (uint16_t)UI_TEXT_ID_COUNT) ||
        (text->table.index_offset != RESOURCE_TEXT_TABLE_HEADER_SIZE) ||
        (text->table.blob_offset != expected_blob_offset))
    {
        return RESOURCE_STATUS_CORRUPT;
    }

    uint8_t record_bytes[RESOURCE_TEXT_TABLE_RECORD_SIZE];
    uint32_t index_crc = STORAGE_CRC32_INIT;
    uint32_t expected_blob_cursor = 0u;
    for (uint16_t i = 0u; i < text->table.record_count; i++)
    {
        resource_text_record_t record;
        status = read_record(text, i, &record, record_bytes);
        if (status != RESOURCE_STATUS_OK)
        {
            return status;
        }
        index_crc = storage_crc32_update(index_crc, record_bytes, sizeof(record_bytes));
        if ((record.text_id != (uint16_t)(UI_TEXT_ID_FIRST + i)) ||
            (record.byte_offset != expected_blob_cursor))
        {
            return RESOURCE_STATUS_CORRUPT;
        }
        status = validate_record_text(text, &record);
        if (status != RESOURCE_STATUS_OK)
        {
            return status;
        }
        expected_blob_cursor += record.byte_length;
    }
    if ((text->table.blob_offset + expected_blob_cursor) != text->entry.payload_size)
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    if ((index_crc ^ STORAGE_CRC32_XOR_OUT) != text->table.index_crc32)
    {
        return RESOURCE_STATUS_CORRUPT;
    }
    return RESOURCE_STATUS_OK;
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
    resource_status_t status = text_entry_lookup(catalog, language_id, &text->entry);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    status = validate_text_table(text);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    text->ready = true;
    return RESOURCE_STATUS_OK;
}

resource_status_t ui_text_catalog_validate_required_languages(resource_catalog_t *catalog)
{
    ui_text_catalog_t text;
    resource_status_t status = ui_text_catalog_select_language(&text,
                                                               catalog,
                                                               (uint8_t)UI_LANGUAGE_EN);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    return ui_text_catalog_select_language(&text, catalog, (uint8_t)UI_LANGUAGE_PT_BR);
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
    if (((uint16_t)id < UI_TEXT_ID_FIRST) || ((uint16_t)id > UI_TEXT_ID_LAST))
    {
        return RESOURCE_STATUS_NOT_FOUND;
    }
    resource_text_record_t record;
    uint8_t bytes[RESOURCE_TEXT_TABLE_RECORD_SIZE];
    const uint16_t index = (uint16_t)((uint16_t)id - UI_TEXT_ID_FIRST);
    resource_status_t status = read_record(text, index, &record, bytes);
    if (status != RESOURCE_STATUS_OK)
    {
        return status;
    }
    if (record.text_id != (uint16_t)id)
    {
        return RESOURCE_STATUS_CORRUPT;
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
