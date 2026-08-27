#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app/app_version.h"
#include "measurement/measurement_calibration.h"

static int expect_true(const int condition, const char *const message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }

    return 0;
}

int main(void)
{
    int failures = 0;
    const wtk_app_version_info_t *const version = wtk_app_version_get();

    failures += expect_true(version != NULL, "version pointer is valid");
    failures += expect_true(strcmp(version->project_name, "WTK.RLCMeter") == 0, "project name matches");
    failures += expect_true(strlen(version->project_version) > 0u, "project version is populated");
    failures += expect_true(strlen(version->git_commit) > 0u, "git commit fallback is populated");
    failures += expect_true(strlen(version->build_type) > 0u, "build type is populated");
    failures += expect_true(strlen(version->firmware_profile) > 0u, "firmware profile is populated");
    failures += expect_true(strcmp(version->hardware_compatibility, "Rev1-STM32F103C8T6-BluePill") == 0,
                            "hardware compatibility label matches");
    failures += expect_true(version->calibration_schema_version == MEASUREMENT_CAL_SCHEMA_VERSION,
                            "calibration schema is versioned");

    return failures;
}
