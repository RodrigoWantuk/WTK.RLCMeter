#ifndef WTK_APP_FLASH_ACCESS_H
#define WTK_APP_FLASH_ACCESS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    APP_FLASH_ACCESS_CALIBRATION_MUTATION = 0,
    APP_FLASH_ACCESS_SETTINGS_MUTATION,
    APP_FLASH_ACCESS_RESOURCE_READ,
    APP_FLASH_ACCESS_GENERIC_POLL,
} app_flash_access_operation_t;

typedef struct
{
    bool quiet;
    bool calibration_mutation;
    bool settings_mutation;
} app_flash_access_snapshot_t;

bool app_flash_access_allowed(const app_flash_access_snapshot_t *snapshot,
                              app_flash_access_operation_t operation);
uint32_t app_flash_access_context_size_bytes(void);

#endif
