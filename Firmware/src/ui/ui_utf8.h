#ifndef WTK_UI_UTF8_H
#define WTK_UI_UTF8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    UI_UTF8_STATUS_OK = 0,
    UI_UTF8_STATUS_END,
    UI_UTF8_STATUS_INVALID,
} ui_utf8_status_t;

ui_utf8_status_t ui_utf8_decode_next(const char *text,
                                      size_t length,
                                      size_t *offset,
                                      uint32_t *codepoint);
bool ui_utf8_validate(const char *text, size_t length);

#endif
