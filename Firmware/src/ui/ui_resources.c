#include "ui/ui_resources.h"

#include <stddef.h>

void ui_resource_streamer_init(ui_resource_streamer_t *streamer,
                               ui_resource_read_fn read,
                               void *read_context,
                               ui_resource_write_fn write,
                               void *write_context)
{
    if (streamer == NULL)
    {
        return;
    }

    streamer->read = read;
    streamer->write = write;
    streamer->read_context = read_context;
    streamer->write_context = write_context;
}

void ui_resource_stream_start(ui_resource_stream_t *stream, const resource_entry_t *entry)
{
    if (stream == NULL)
    {
        return;
    }

    stream->entry = entry;
    stream->offset = 0u;
    stream->active = (entry != NULL) && (entry->size > 0u);
}

bool ui_resource_stream_step(ui_resource_streamer_t *streamer, ui_resource_stream_t *stream)
{
    if ((streamer == NULL) || (stream == NULL) || !stream->active ||
        (streamer->read == NULL) || (streamer->write == NULL) || (stream->entry == NULL))
    {
        return false;
    }

    size_t chunk = UI_RESOURCE_SCRATCH_BYTES;
    const uint32_t remaining = stream->entry->size - stream->offset;
    if (chunk > remaining)
    {
        chunk = remaining;
    }

    if (!streamer->read(streamer->read_context, stream->entry, stream->offset, streamer->scratch, chunk))
    {
        stream->active = false;
        return false;
    }

    if (!streamer->write(streamer->write_context, streamer->scratch, chunk))
    {
        stream->active = false;
        return false;
    }

    stream->offset += (uint32_t)chunk;
    if (stream->offset >= stream->entry->size)
    {
        stream->active = false;
    }

    return true;
}
