#include "app/app_version.h"

#include "wtk_build_config.h"

static const wtk_app_version_info_t g_wtk_app_version = {
    .project_name = WTK_PROJECT_NAME,
    .project_version = WTK_PROJECT_VERSION,
    .git_commit = WTK_GIT_COMMIT,
    .build_type = WTK_BUILD_TYPE,
    .hardware_compatibility = WTK_HARDWARE_COMPATIBILITY,
    .calibration_schema_version = WTK_CALIBRATION_SCHEMA_VERSION,
};

const wtk_app_version_info_t *wtk_app_version_get(void)
{
    return &g_wtk_app_version;
}
