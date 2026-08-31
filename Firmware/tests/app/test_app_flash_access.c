#include "app/app_flash_access.h"

#include <stdbool.h>
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

static int test_flash_access_policy(void)
{
    int failures = 0;
    app_flash_access_snapshot_t idle = {0};
    failures += expect_true(app_flash_access_allowed(&idle, APP_FLASH_ACCESS_RESOURCE_READ),
                            "resource read allowed when idle");
    failures += expect_true(app_flash_access_allowed(&idle, APP_FLASH_ACCESS_SETTINGS_MUTATION),
                            "settings mutation allowed when idle");
    failures += expect_true(app_flash_access_allowed(&idle, APP_FLASH_ACCESS_CALIBRATION_MUTATION),
                            "calibration mutation allowed when idle");
    app_flash_access_snapshot_t quiet = {.quiet = true};
    failures += expect_true(!app_flash_access_allowed(&quiet, APP_FLASH_ACCESS_RESOURCE_READ),
                            "resource read deferred during quiet");
    failures += expect_true(!app_flash_access_allowed(&quiet, APP_FLASH_ACCESS_SETTINGS_MUTATION),
                            "settings mutation deferred during quiet");
    app_flash_access_snapshot_t calibration = {.calibration_mutation = true};
    failures += expect_true(!app_flash_access_allowed(&calibration, APP_FLASH_ACCESS_RESOURCE_READ),
                            "resource read deferred during calibration mutation");
    failures += expect_true(!app_flash_access_allowed(&calibration, APP_FLASH_ACCESS_SETTINGS_MUTATION),
                            "settings mutation deferred during calibration mutation");
    app_flash_access_snapshot_t settings = {.settings_mutation = true};
    failures += expect_true(!app_flash_access_allowed(&settings, APP_FLASH_ACCESS_RESOURCE_READ),
                            "resource read deferred during settings mutation");
    failures += expect_true(!app_flash_access_allowed(&settings, APP_FLASH_ACCESS_CALIBRATION_MUTATION),
                            "calibration mutation deferred during settings mutation");
    failures += expect_true(app_flash_access_context_size_bytes() == 0u, "policy has no runtime state");
    return failures;
}

int main(void)
{
    return test_flash_access_policy();
}
