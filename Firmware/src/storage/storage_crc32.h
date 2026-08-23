#ifndef WTK_STORAGE_CRC32_H
#define WTK_STORAGE_CRC32_H

#include <stddef.h>
#include <stdint.h>

#define STORAGE_CRC32_INIT UINT32_C(0xFFFFFFFF)
#define STORAGE_CRC32_XOR_OUT UINT32_C(0xFFFFFFFF)

uint32_t storage_crc32_update(uint32_t crc, const void *data, size_t size);
uint32_t storage_crc32(const void *data, size_t size);

#endif
