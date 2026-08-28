#ifndef WTK_APP_SETTINGS_SERVICE_H
#define WTK_APP_SETTINGS_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "storage/storage_layout.h"

typedef enum
{
    APP_BACKLIGHT_TIMEOUT_OFF = 0,
    APP_BACKLIGHT_TIMEOUT_15S = 15,
    APP_BACKLIGHT_TIMEOUT_30S = 30,
    APP_BACKLIGHT_TIMEOUT_60S = 60,
    APP_BACKLIGHT_TIMEOUT_120S = 120,
    APP_BACKLIGHT_TIMEOUT_300S = 300,
} app_backlight_timeout_t;

typedef struct
{
    uint8_t brightness_percent;
    app_backlight_timeout_t backlight_timeout;
    bool sound_enabled;
} app_settings_t;

typedef enum
{
    APP_SETTINGS_SLOT_A = 0,
    APP_SETTINGS_SLOT_B = 1,
} app_settings_slot_t;

typedef enum
{
    APP_SETTINGS_SLOT_ERASED = 0,
    APP_SETTINGS_SLOT_CORRUPT,
    APP_SETTINGS_SLOT_INCOMPATIBLE,
    APP_SETTINGS_SLOT_SEMANTIC_INVALID,
    APP_SETTINGS_SLOT_VALID,
} app_settings_slot_status_t;

typedef enum
{
    APP_SETTINGS_STATUS_DEFAULTS = 0,
    APP_SETTINGS_STATUS_LOADED,
    APP_SETTINGS_STATUS_DIRTY,
    APP_SETTINGS_STATUS_SAVING,
    APP_SETTINGS_STATUS_SAVE_FAILED,
    APP_SETTINGS_STATUS_STORAGE_UNAVAILABLE,
} app_settings_status_t;

typedef enum
{
    APP_SETTINGS_STORE_IDLE = 0,
    APP_SETTINGS_STORE_ERASE_START,
    APP_SETTINGS_STORE_ERASE_WAIT,
    APP_SETTINGS_STORE_PROGRAM_HEADER_START,
    APP_SETTINGS_STORE_PROGRAM_HEADER_WAIT,
    APP_SETTINGS_STORE_PROGRAM_PAYLOAD_START,
    APP_SETTINGS_STORE_PROGRAM_PAYLOAD_WAIT,
    APP_SETTINGS_STORE_VERIFY_PRE_COMMIT,
    APP_SETTINGS_STORE_PROGRAM_COMMIT_START,
    APP_SETTINGS_STORE_PROGRAM_COMMIT_WAIT,
    APP_SETTINGS_STORE_VERIFY_COMMIT,
    APP_SETTINGS_STORE_DONE,
    APP_SETTINGS_STORE_ERROR,
} app_settings_store_state_t;

typedef struct
{
    bsp_status_t (*read)(uint32_t address, void *dst, size_t size, void *user);
    bsp_status_t (*erase_sector_start)(uint32_t address, uint32_t now_ms, void *user);
    bsp_status_t (*program_start)(uint32_t address, const void *src, size_t size, uint32_t now_ms, void *user);
    bsp_status_t (*poll)(uint32_t now_ms, void *user);
    void *user;
} app_settings_store_io_t;

typedef struct
{
    app_settings_slot_status_t status;
    app_settings_slot_t slot;
    uint32_t sequence;
    app_settings_t settings;
} app_settings_slot_info_t;

typedef struct
{
    app_settings_store_io_t io;
    storage_partition_t slots[2];
    app_settings_t current;
    app_settings_t persisted;
    uint32_t persisted_sequence;
    uint32_t target_sequence;
    app_settings_slot_t active_slot;
    app_settings_slot_t target_slot;
    app_settings_store_state_t state;
    app_settings_status_t status;
    bsp_status_t last_error;
    uint8_t frame[36];
    uint8_t verify[36];
    bool initialized;
    bool storage_available;
    bool persisted_valid;
    bool dirty;
    bool save_failed;
} app_settings_service_t;

app_settings_t app_settings_defaults(void);
bool app_settings_validate(const app_settings_t *settings);
bool app_backlight_timeout_valid(app_backlight_timeout_t timeout);
app_backlight_timeout_t app_backlight_timeout_next(app_backlight_timeout_t timeout);
app_backlight_timeout_t app_backlight_timeout_prev(app_backlight_timeout_t timeout);

bsp_status_t app_settings_service_init(app_settings_service_t *service,
                                       const app_settings_store_io_t *io,
                                       uint32_t capacity_bytes);
bsp_status_t app_settings_service_load(app_settings_service_t *service,
                                       app_settings_slot_info_t diagnostics[2]);
void app_settings_service_use_defaults(app_settings_service_t *service);
bsp_status_t app_settings_service_set(app_settings_service_t *service,
                                      const app_settings_t *settings);
bsp_status_t app_settings_service_save_start(app_settings_service_t *service, uint32_t now_ms);
bsp_status_t app_settings_service_step(app_settings_service_t *service, uint32_t now_ms);
bsp_status_t app_settings_service_acknowledge(app_settings_service_t *service);
const app_settings_t *app_settings_service_current(const app_settings_service_t *service);
app_settings_status_t app_settings_service_status(const app_settings_service_t *service);
app_settings_store_state_t app_settings_service_store_state(const app_settings_service_t *service);
bsp_status_t app_settings_service_last_error(const app_settings_service_t *service);
bool app_settings_service_busy(const app_settings_service_t *service);
bool app_settings_service_dirty(const app_settings_service_t *service);
bool app_settings_service_save_failed(const app_settings_service_t *service);
bool app_settings_service_storage_available(const app_settings_service_t *service);
uint32_t app_settings_service_persisted_sequence(const app_settings_service_t *service);
app_settings_slot_t app_settings_service_active_slot(const app_settings_service_t *service);
uint32_t app_settings_service_context_size_bytes(void);

#endif
