#include "app/app_io_workspace.h"

#include <stdalign.h>

_Static_assert(sizeof(app_io_workspace_storage_t) == HW_METROLOGY_RAW_BUFFER_BYTES,
               "shared workspace must fit the canonical raw capture");
_Static_assert(MEASUREMENT_CAL_MAX_FRAME_BYTES <= sizeof(app_io_workspace_storage_t),
               "shared workspace must fit one calibration frame image");
_Static_assert(alignof(app_io_workspace_storage_t) >= alignof(uint32_t),
               "shared workspace must preserve uint32_t raw-buffer alignment");

void app_io_workspace_init(app_io_workspace_t *workspace)
{
    if (workspace != NULL)
    {
        *workspace = (app_io_workspace_t){0};
        workspace->owner = APP_IO_WORKSPACE_OWNER_FREE;
    }
}

bsp_status_t app_io_workspace_acquire(app_io_workspace_t *workspace,
                                      app_io_workspace_owner_t owner)
{
    if ((workspace == NULL) || (owner == APP_IO_WORKSPACE_OWNER_FREE))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (workspace->owner != APP_IO_WORKSPACE_OWNER_FREE)
    {
        return BSP_STATUS_BUSY;
    }
    workspace->owner = owner;
    return BSP_STATUS_OK;
}

bsp_status_t app_io_workspace_release(app_io_workspace_t *workspace,
                                      app_io_workspace_owner_t owner)
{
    if ((workspace == NULL) || (owner == APP_IO_WORKSPACE_OWNER_FREE))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (workspace->owner != owner)
    {
        return BSP_STATUS_BUSY;
    }
    workspace->owner = APP_IO_WORKSPACE_OWNER_FREE;
    return BSP_STATUS_OK;
}

app_io_workspace_owner_t app_io_workspace_owner(const app_io_workspace_t *workspace)
{
    return (workspace == NULL) ? APP_IO_WORKSPACE_OWNER_FREE : workspace->owner;
}

uint32_t *app_io_workspace_metrology_raw_words(app_io_workspace_t *workspace)
{
    return (workspace == NULL) ? NULL : workspace->storage.metrology_raw_words;
}

uint8_t *app_io_workspace_calibration_frame(app_io_workspace_t *workspace)
{
    return (workspace == NULL) ? NULL : workspace->storage.calibration_frame;
}

size_t app_io_workspace_metrology_word_count(void)
{
    return HW_METROLOGY_RAW_WORD_COUNT;
}

size_t app_io_workspace_calibration_frame_bytes(void)
{
    return MEASUREMENT_CAL_MAX_FRAME_BYTES;
}

uint32_t app_io_workspace_storage_size_bytes(void)
{
    return (uint32_t)sizeof(app_io_workspace_storage_t);
}

uint32_t app_io_workspace_context_size_bytes(void)
{
    return (uint32_t)sizeof(app_io_workspace_t);
}
