#ifndef WTK_UI_RESOURCES_H
#define WTK_UI_RESOURCES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "storage/resource_store.h"

enum
{
    UI_RESOURCE_SCRATCH_BYTES = 256u,
};

typedef enum
{
    UI_RESOURCE_STATUS_OK = 0,
    UI_RESOURCE_STATUS_DEFERRED,
    UI_RESOURCE_STATUS_ERROR,
} ui_resource_status_t;

typedef ui_resource_status_t (*ui_resource_read_fn)(void *context,
                                                    const resource_entry_t *entry,
                                                    uint32_t offset,
                                                    uint8_t *dst,
                                                    size_t size);

typedef ui_resource_status_t (*ui_resource_write_fn)(void *context, const uint8_t *src, size_t size);

typedef struct
{
    uint8_t scratch[UI_RESOURCE_SCRATCH_BYTES];
    ui_resource_read_fn read;
    ui_resource_write_fn write;
    void *read_context;
    void *write_context;
} ui_resource_streamer_t;

typedef struct
{
    const resource_entry_t *entry;
    uint32_t offset;
    bool active;
} ui_resource_stream_t;

void ui_resource_streamer_init(ui_resource_streamer_t *streamer,
                               ui_resource_read_fn read,
                               void *read_context,
                               ui_resource_write_fn write,
                               void *write_context);
void ui_resource_stream_start(ui_resource_stream_t *stream, const resource_entry_t *entry);
ui_resource_status_t ui_resource_stream_step(ui_resource_streamer_t *streamer, ui_resource_stream_t *stream);

#endif
