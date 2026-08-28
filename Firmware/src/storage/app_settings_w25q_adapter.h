#ifndef WTK_APP_SETTINGS_W25Q_ADAPTER_H
#define WTK_APP_SETTINGS_W25Q_ADAPTER_H

#include "app/app_settings_service.h"
#include "drivers/w25q.h"

app_settings_store_io_t app_settings_w25q_store_io(w25q_device_t *flash);

#endif
