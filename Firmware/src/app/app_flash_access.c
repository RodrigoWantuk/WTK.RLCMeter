#include "app/app_flash_access.h"

#include <stddef.h>

bool app_flash_access_allowed(const app_flash_access_snapshot_t *snapshot,
                              app_flash_access_operation_t operation)
{
    if (snapshot == NULL)
    {
        return false;
    }
    switch (operation)
    {
    case APP_FLASH_ACCESS_CALIBRATION_MUTATION:
        return !snapshot->quiet && !snapshot->settings_mutation;
    case APP_FLASH_ACCESS_SETTINGS_MUTATION:
        return !snapshot->quiet && !snapshot->calibration_mutation;
    case APP_FLASH_ACCESS_RESOURCE_READ:
        return !snapshot->quiet && !snapshot->calibration_mutation && !snapshot->settings_mutation;
    case APP_FLASH_ACCESS_GENERIC_POLL:
        return !snapshot->quiet && !snapshot->calibration_mutation && !snapshot->settings_mutation;
    default:
        return false;
    }
}

uint32_t app_flash_access_context_size_bytes(void)
{
    return 0u;
}
