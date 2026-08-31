#include "ui/ui_utf8.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int expect_u32(uint32_t actual, uint32_t expected, const char *message)
{
    if (actual != expected)
    {
        (void)fprintf(stderr, "FAIL: %s (got %lu expected %lu)\n",
                      message,
                      (unsigned long)actual,
                      (unsigned long)expected);
        return 1;
    }
    return 0;
}

static int test_valid_utf8_and_codepoint_decode(void)
{
    int failures = 0;
    const char text[] = "PORTUGU\xC3\x8AS";
    size_t offset = 7u;
    uint32_t codepoint = 0u;
    failures += expect_true(ui_utf8_validate(text, strlen(text)), "valid UTF-8 accepted");
    failures += expect_true(ui_utf8_decode_next(text, strlen(text), &offset, &codepoint) == UI_UTF8_STATUS_OK,
                            "decode multibyte codepoint");
    failures += expect_u32(codepoint, 0x00CAu, "decoded Ê");
    failures += expect_u32((uint32_t)offset, 9u, "offset advanced by two bytes");
    return failures;
}

static int test_invalid_utf8_rejected(void)
{
    int failures = 0;
    const uint8_t overlong[] = {0xC0u, 0xAFu, 0u};
    const uint8_t surrogate[] = {0xEDu, 0xA0u, 0x80u, 0u};
    failures += expect_true(!ui_utf8_validate((const char *)overlong, 2u), "overlong sequence rejected");
    failures += expect_true(!ui_utf8_validate((const char *)surrogate, 3u), "surrogate rejected");
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_valid_utf8_and_codepoint_decode();
    failures += test_invalid_utf8_rejected();
    return failures;
}
