#include "ui/ui_utf8.h"

static bool continuation(uint8_t byte)
{
    return (byte & 0xC0u) == 0x80u;
}

ui_utf8_status_t ui_utf8_decode_next(const char *text,
                                      size_t length,
                                      size_t *offset,
                                      uint32_t *codepoint)
{
    if ((text == NULL) || (offset == NULL) || (codepoint == NULL) || (*offset > length))
    {
        return UI_UTF8_STATUS_INVALID;
    }
    if ((*offset == length) || (text[*offset] == '\0'))
    {
        return UI_UTF8_STATUS_END;
    }
    const uint8_t b0 = (uint8_t)text[*offset];
    if (b0 < 0x80u)
    {
        *codepoint = b0;
        *offset += 1u;
        return UI_UTF8_STATUS_OK;
    }
    if ((b0 >= 0xC2u) && (b0 <= 0xDFu))
    {
        if ((*offset + 1u) >= length)
        {
            return UI_UTF8_STATUS_INVALID;
        }
        const uint8_t b1 = (uint8_t)text[*offset + 1u];
        if (!continuation(b1))
        {
            return UI_UTF8_STATUS_INVALID;
        }
        *codepoint = ((uint32_t)(b0 & 0x1Fu) << 6u) | (uint32_t)(b1 & 0x3Fu);
        *offset += 2u;
        return UI_UTF8_STATUS_OK;
    }
    if ((b0 >= 0xE0u) && (b0 <= 0xEFu))
    {
        if ((*offset + 2u) >= length)
        {
            return UI_UTF8_STATUS_INVALID;
        }
        const uint8_t b1 = (uint8_t)text[*offset + 1u];
        const uint8_t b2 = (uint8_t)text[*offset + 2u];
        if (!continuation(b1) || !continuation(b2) ||
            ((b0 == 0xE0u) && (b1 < 0xA0u)) ||
            ((b0 == 0xEDu) && (b1 >= 0xA0u)))
        {
            return UI_UTF8_STATUS_INVALID;
        }
        *codepoint = ((uint32_t)(b0 & 0x0Fu) << 12u) |
                     ((uint32_t)(b1 & 0x3Fu) << 6u) |
                     (uint32_t)(b2 & 0x3Fu);
        *offset += 3u;
        return UI_UTF8_STATUS_OK;
    }
    if ((b0 >= 0xF0u) && (b0 <= 0xF4u))
    {
        if ((*offset + 3u) >= length)
        {
            return UI_UTF8_STATUS_INVALID;
        }
        const uint8_t b1 = (uint8_t)text[*offset + 1u];
        const uint8_t b2 = (uint8_t)text[*offset + 2u];
        const uint8_t b3 = (uint8_t)text[*offset + 3u];
        if (!continuation(b1) || !continuation(b2) || !continuation(b3) ||
            ((b0 == 0xF0u) && (b1 < 0x90u)) ||
            ((b0 == 0xF4u) && (b1 >= 0x90u)))
        {
            return UI_UTF8_STATUS_INVALID;
        }
        *codepoint = ((uint32_t)(b0 & 0x07u) << 18u) |
                     ((uint32_t)(b1 & 0x3Fu) << 12u) |
                     ((uint32_t)(b2 & 0x3Fu) << 6u) |
                     (uint32_t)(b3 & 0x3Fu);
        *offset += 4u;
        return UI_UTF8_STATUS_OK;
    }
    return UI_UTF8_STATUS_INVALID;
}

bool ui_utf8_validate(const char *text, size_t length)
{
    size_t offset = 0u;
    uint32_t codepoint = 0u;
    while (offset < length)
    {
        const ui_utf8_status_t status = ui_utf8_decode_next(text, length, &offset, &codepoint);
        if (status == UI_UTF8_STATUS_END)
        {
            return true;
        }
        if (status != UI_UTF8_STATUS_OK)
        {
            return false;
        }
    }
    return true;
}
