#include "drivers/ili9341_geometry.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int test_rotation_sizes(void)
{
    int failures = 0;
    const ili9341_size_t r0 = ili9341_logical_size(0u);
    const ili9341_size_t r1 = ili9341_logical_size(1u);
    const ili9341_size_t r2 = ili9341_logical_size(2u);
    const ili9341_size_t r3 = ili9341_logical_size(3u);

    failures += expect_true((r0.width == 240u) && (r0.height == 320u), "rotation 0 is 240x320");
    failures += expect_true((r1.width == 320u) && (r1.height == 240u), "rotation 1 is 320x240");
    failures += expect_true((r2.width == 240u) && (r2.height == 320u), "rotation 2 is 240x320");
    failures += expect_true((r3.width == 320u) && (r3.height == 240u), "rotation 3 is 320x240");

    return failures;
}

static int test_rect_validation_rotation_0(void)
{
    int failures = 0;
    failures += expect_true(ili9341_rect_is_valid(0u, 239u, 319u, 1u, 1u),
                            "rotation 0 accepts last pixel");
    failures += expect_true(!ili9341_rect_is_valid(0u, 240u, 0u, 1u, 1u), "rotation 0 rejects x edge");
    failures += expect_true(!ili9341_rect_is_valid(0u, 0u, 320u, 1u, 1u), "rotation 0 rejects y edge");
    failures += expect_true(!ili9341_rect_is_valid(0u, 239u, 0u, 2u, 1u),
                            "rotation 0 rejects x overflow");
    failures += expect_true(!ili9341_rect_is_valid(0u, 0u, 319u, 1u, 2u),
                            "rotation 0 rejects y overflow");
    failures += expect_true(!ili9341_rect_is_valid(0u, 0u, 0u, 0u, 1u), "rotation 0 rejects zero width");
    failures += expect_true(!ili9341_rect_is_valid(0u, 0u, 0u, 1u, 0u), "rotation 0 rejects zero height");

    return failures;
}

static int test_rect_validation_landscape(void)
{
    int failures = 0;
    failures += expect_true(ili9341_rect_is_valid(1u, 319u, 239u, 1u, 1u),
                            "rotation 1 accepts last landscape pixel");
    failures += expect_true(ili9341_rect_is_valid(3u, 319u, 239u, 1u, 1u),
                            "rotation 3 accepts last landscape pixel");
    failures += expect_true(!ili9341_rect_is_valid(1u, 320u, 0u, 1u, 1u), "rotation 1 rejects x edge");
    failures += expect_true(!ili9341_rect_is_valid(1u, 0u, 240u, 1u, 1u), "rotation 1 rejects y edge");
    failures += expect_true(!ili9341_rect_is_valid(1u, 319u, 0u, 2u, 1u),
                            "rotation 1 rejects x overflow");
    failures += expect_true(!ili9341_rect_is_valid(1u, 0u, 239u, 1u, 2u),
                            "rotation 1 rejects y overflow");

    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_rotation_sizes();
    failures += test_rect_validation_rotation_0();
    failures += test_rect_validation_landscape();

    return (failures == 0) ? 0 : 1;
}
