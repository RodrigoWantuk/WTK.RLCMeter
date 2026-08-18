#include "drivers/w25q.h"

#include <stdbool.h>
#include <stdio.h>

static int g_failures = 0;

static void expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        g_failures++;
    }
}

static void test_decode_supported_parts(void)
{
    w25q_part_info_t part = {0};
    expect_true(w25q_decode_jedec((w25q_jedec_id_t){0xEFu, 0x40u, 0x15u}, &part) == W25Q_STATUS_OK,
                "W25Q16 must decode");
    expect_true(part.capacity_bytes == (2u * 1024u * 1024u), "W25Q16 capacity");
    expect_true(w25q_decode_jedec((w25q_jedec_id_t){0xEFu, 0x40u, 0x17u}, &part) == W25Q_STATUS_OK,
                "W25Q64 must decode");
    expect_true(part.capacity_bytes == (8u * 1024u * 1024u), "W25Q64 capacity");
    expect_true(w25q_decode_jedec((w25q_jedec_id_t){0x20u, 0x40u, 0x17u}, &part) ==
                    W25Q_STATUS_UNSUPPORTED_DEVICE,
                "non-Winbond manufacturer rejected");
    expect_true(w25q_decode_jedec((w25q_jedec_id_t){0xEFu, 0x99u, 0x17u}, &part) ==
                    W25Q_STATUS_UNSUPPORTED_DEVICE,
                "unknown memory type rejected");
    expect_true(w25q_decode_jedec((w25q_jedec_id_t){0xEFu, 0x40u, 0x19u}, &part) ==
                    W25Q_STATUS_UNSUPPORTED_DEVICE,
                "unknown density rejected");
}

static void test_ranges_and_pages(void)
{
    const uint32_t capacity = 8u * 1024u * 1024u;
    expect_true(w25q_range_valid(capacity, 0u, 1u), "start of flash valid");
    expect_true(w25q_range_valid(capacity, capacity - 1u, 1u), "last byte valid");
    expect_true(!w25q_range_valid(capacity, capacity, 0u), "address at capacity invalid");
    expect_true(!w25q_range_valid(capacity, capacity - 1u, 2u), "range overflow invalid");
    expect_true(w25q_page_program_span(0u, 300u) == W25Q_PAGE_SIZE, "page span from aligned address");
    expect_true(w25q_page_program_span(250u, 16u) == 6u, "page span clipped at boundary");
    expect_true(w25q_reserved_test_sector_address(capacity) == (capacity - W25Q_SECTOR_SIZE),
                "reserved test sector is final sector");
}

int main(void)
{
    test_decode_supported_parts();
    test_ranges_and_pages();
    return (g_failures == 0) ? 0 : 1;
}
