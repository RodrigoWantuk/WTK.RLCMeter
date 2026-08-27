#ifndef WTK_APP_VERSION_H
#define WTK_APP_VERSION_H

#include <stdint.h>

typedef struct
{
    const char *project_name;
    const char *project_version;
    const char *git_commit;
    const char *build_type;
    const char *firmware_profile;
    const char *hardware_compatibility;
    uint32_t calibration_schema_version;
} wtk_app_version_info_t;

const wtk_app_version_info_t *wtk_app_version_get(void);

#endif
