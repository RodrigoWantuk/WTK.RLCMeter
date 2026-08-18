#include <stdbool.h>

#include "app/app_version.h"

int main(void)
{
    const wtk_app_version_info_t *const version = wtk_app_version_get();
    (void)version;

    while (true)
    {
    }
}
