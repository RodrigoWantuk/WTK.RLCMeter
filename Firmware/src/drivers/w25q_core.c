#include "drivers/w25q.h"

#include <stddef.h>

typedef struct
{
    uint8_t capacity_code;
    uint32_t capacity_bytes;
    const char *name;
} w25q_capacity_entry_t;

static const w25q_capacity_entry_t g_supported_parts[] = {
    {0x15u, 2u * 1024u * 1024u, "W25Q16"},
    {0x16u, 4u * 1024u * 1024u, "W25Q32"},
    {0x17u, 8u * 1024u * 1024u, "W25Q64"},
    {0x18u, 16u * 1024u * 1024u, "W25Q128"},
};

w25q_status_t w25q_decode_jedec(w25q_jedec_id_t jedec, w25q_part_info_t *part)
{
    if (part == NULL)
    {
        return W25Q_STATUS_INVALID_ARG;
    }

    if (jedec.manufacturer_id != 0xEFu)
    {
        return W25Q_STATUS_UNSUPPORTED_DEVICE;
    }

    for (size_t i = 0u; i < (sizeof(g_supported_parts) / sizeof(g_supported_parts[0])); i++)
    {
        if (g_supported_parts[i].capacity_code == jedec.capacity_code)
        {
            part->jedec = jedec;
            part->capacity_bytes = g_supported_parts[i].capacity_bytes;
            part->name = g_supported_parts[i].name;
            return W25Q_STATUS_OK;
        }
    }

    return W25Q_STATUS_UNSUPPORTED_DEVICE;
}

bool w25q_range_valid(uint32_t capacity_bytes, uint32_t address, size_t size)
{
    if ((capacity_bytes == 0u) || (address >= capacity_bytes))
    {
        return false;
    }

    if (size == 0u)
    {
        return true;
    }

    const uint32_t remaining = capacity_bytes - address;
    return size <= remaining;
}

size_t w25q_page_program_span(uint32_t address, size_t requested)
{
    const uint32_t page_offset = address % W25Q_PAGE_SIZE;
    const size_t page_remaining = (size_t)(W25Q_PAGE_SIZE - page_offset);

    return (requested < page_remaining) ? requested : page_remaining;
}

uint32_t w25q_reserved_test_sector_address(uint32_t capacity_bytes)
{
    if (capacity_bytes < W25Q_SECTOR_SIZE)
    {
        return 0u;
    }

    return capacity_bytes - W25Q_BRINGUP_TEST_SECTOR_OFFSET_FROM_END;
}

const char *w25q_status_string(w25q_status_t status)
{
    switch (status)
    {
    case W25Q_STATUS_OK:
        return "OK";
    case W25Q_STATUS_BUSY:
        return "BUSY";
    case W25Q_STATUS_ERROR:
        return "ERROR";
    case W25Q_STATUS_TIMEOUT:
        return "TIMEOUT";
    case W25Q_STATUS_INVALID_ARG:
        return "INVALID_ARG";
    case W25Q_STATUS_UNSUPPORTED_DEVICE:
        return "UNSUPPORTED_DEVICE";
    case W25Q_STATUS_OUT_OF_RANGE:
        return "OUT_OF_RANGE";
    default:
        return "UNKNOWN";
    }
}
