#include "storage/storage_crc32.h"

#include <stdint.h>

uint32_t storage_crc32_update(uint32_t crc, const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t value = crc;

    if ((bytes == NULL) && (size != 0u))
    {
        return crc;
    }

    for (size_t i = 0u; i < size; i++)
    {
        value ^= (uint32_t)bytes[i];
        for (uint8_t bit = 0u; bit < 8u; bit++)
        {
            const uint32_t mask = (uint32_t)(0u - (value & 1u));
            value = (value >> 1u) ^ (0xEDB88320u & mask);
        }
    }

    return value;
}

uint32_t storage_crc32(const void *data, size_t size)
{
    return storage_crc32_update((uint32_t)STORAGE_CRC32_INIT, data, size) ^
           (uint32_t)STORAGE_CRC32_XOR_OUT;
}
