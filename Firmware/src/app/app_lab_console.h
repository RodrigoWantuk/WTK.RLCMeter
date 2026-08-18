#ifndef WTK_APP_LAB_CONSOLE_H
#define WTK_APP_LAB_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/ili9341.h"
#include "drivers/w25q.h"
#include "wtk_build_config.h"

enum
{
    APP_LAB_CONSOLE_LINE_CAPACITY = 48u,
    APP_LAB_FLASH_TEST_SIZE = 64u,
};

typedef enum
{
    APP_LAB_FLASH_TEST_IDLE = 0,
    APP_LAB_FLASH_TEST_ERASE_START,
    APP_LAB_FLASH_TEST_ERASE_WAIT,
    APP_LAB_FLASH_TEST_VERIFY_ERASE,
    APP_LAB_FLASH_TEST_PROGRAM_START,
    APP_LAB_FLASH_TEST_PROGRAM_WAIT,
    APP_LAB_FLASH_TEST_READBACK,
    APP_LAB_FLASH_TEST_VERIFY,
    APP_LAB_FLASH_TEST_CLEANUP_ERASE_START,
    APP_LAB_FLASH_TEST_CLEANUP_ERASE_WAIT,
    APP_LAB_FLASH_TEST_COMPLETE,
    APP_LAB_FLASH_TEST_ERROR,
} app_lab_flash_test_state_t;

typedef struct
{
#if WTK_ENABLE_LAB_DIAGNOSTICS
    char line[APP_LAB_CONSOLE_LINE_CAPACITY];
    uint8_t line_length;
    app_lab_flash_test_state_t flash_test_state;
    uint32_t test_sector_address;
    uint8_t pattern[APP_LAB_FLASH_TEST_SIZE];
    uint8_t readback[APP_LAB_FLASH_TEST_SIZE];
    bool busy_observed;
#else
    uint8_t unused;
#endif
} app_lab_console_t;

void app_lab_console_init(app_lab_console_t *console);
void app_lab_console_step(app_lab_console_t *console, w25q_device_t *flash, ili9341_t *display, uint32_t now_ms);
bool app_lab_console_flash_busy(const app_lab_console_t *console);

#endif
