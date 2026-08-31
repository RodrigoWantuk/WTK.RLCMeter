#ifndef WTK_RESOURCE_W25Q_ADAPTER_H
#define WTK_RESOURCE_W25Q_ADAPTER_H

#include "app/app_flash_access.h"
#include "drivers/w25q.h"
#include "storage/resource_store.h"

typedef app_flash_access_snapshot_t (*resource_w25q_policy_snapshot_fn)(void *user);

typedef struct
{
    const w25q_device_t *flash;
    resource_w25q_policy_snapshot_fn policy_snapshot;
    void *policy_user;
} resource_w25q_reader_t;

resource_catalog_io_t resource_w25q_catalog_io(resource_w25q_reader_t *reader);

#endif
