#ifndef WTK_UI_TEXT_CATALOG_H
#define WTK_UI_TEXT_CATALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "storage/resource_store.h"
#include "ui/ui_text.h"

typedef struct
{
    resource_catalog_t *catalog;
    resource_entry_t entry;
    resource_text_table_header_t table;
    uint8_t language_id;
    bool ready;
} ui_text_catalog_t;

void ui_text_catalog_init(ui_text_catalog_t *text);
resource_status_t ui_text_catalog_select_language(ui_text_catalog_t *text,
                                                  resource_catalog_t *catalog,
                                                  uint8_t language_id);
resource_status_t ui_text_catalog_validate_required_languages(resource_catalog_t *catalog);
resource_status_t ui_text_catalog_resolve(ui_text_catalog_t *text,
                                          ui_text_id_t id,
                                          char *dst,
                                          size_t capacity);
uint8_t ui_text_catalog_language(const ui_text_catalog_t *text);
bool ui_text_catalog_ready(const ui_text_catalog_t *text);
uint32_t ui_text_catalog_context_size_bytes(void);

#endif
