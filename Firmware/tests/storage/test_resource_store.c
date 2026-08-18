#include "storage/resource_store.h"
#include "ui/ui_resources.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static int g_failures = 0;
static uint8_t g_data[600];
static uint32_t g_written = 0u;

static void expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        g_failures++;
    }
}

static bool read_resource(void *context, const resource_entry_t *entry, uint32_t offset, uint8_t *dst, size_t size)
{
    (void)context;
    if ((entry == NULL) || (dst == NULL) || ((offset + size) > entry->size))
    {
        return false;
    }

    for (size_t i = 0u; i < size; i++)
    {
        dst[i] = g_data[offset + i];
    }

    return true;
}

static bool write_resource(void *context, const uint8_t *src, size_t size)
{
    (void)context;
    if (src == NULL)
    {
        return false;
    }

    g_written += (uint32_t)size;
    return true;
}

static void test_manifest_bounds(void)
{
    const resource_pack_header_t header = {
        .magic = RESOURCE_STORE_MAGIC,
        .schema_version = RESOURCE_STORE_SCHEMA_VERSION,
        .entry_count = 2u,
        .table_offset = 64u,
        .blob_offset = 256u,
    };
    const resource_entry_t entry = {
        .id = 1u,
        .offset = 300u,
        .size = 100u,
        .width = 10u,
        .height = 10u,
        .format = RESOURCE_FORMAT_RAW_RGB565,
        .flags = 0u,
        .metrics_offset = 0u,
        .metrics_size = 0u,
        .crc32 = 0u,
    };

    expect_true(resource_pack_header_valid(&header, 1024u), "valid header");
    expect_true(resource_entry_valid(&header, &entry, 1024u), "valid entry");
    expect_true(!resource_pack_header_valid(&header, 70u), "table overflow rejected");
    expect_true(!resource_entry_valid(&header, &entry, 350u), "entry overflow rejected");
}

static void test_stream_chunks(void)
{
    for (size_t i = 0u; i < sizeof(g_data); i++)
    {
        g_data[i] = (uint8_t)i;
    }

    const resource_entry_t entry = {
        .id = 2u,
        .offset = 256u,
        .size = 513u,
        .width = 0u,
        .height = 0u,
        .format = RESOURCE_FORMAT_GLYPH_A4,
        .flags = 0u,
        .metrics_offset = 0u,
        .metrics_size = 0u,
        .crc32 = 0u,
    };
    ui_resource_streamer_t streamer;
    ui_resource_stream_t stream;
    g_written = 0u;

    ui_resource_streamer_init(&streamer, read_resource, NULL, write_resource, NULL);
    ui_resource_stream_start(&stream, &entry);
    expect_true(ui_resource_stream_step(&streamer, &stream), "first chunk");
    expect_true(ui_resource_stream_step(&streamer, &stream), "second chunk");
    expect_true(ui_resource_stream_step(&streamer, &stream), "third chunk");
    expect_true(!stream.active, "stream complete");
    expect_true(g_written == entry.size, "all bytes streamed");
}

int main(void)
{
    test_manifest_bounds();
    test_stream_chunks();
    return (g_failures == 0) ? 0 : 1;
}
