#ifndef WTK_APP_IO_WORKSPACE_H
#define WTK_APP_IO_WORKSPACE_H

#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "hardware/hw_metrology_raw.h"
#include "measurement/measurement_calibration_store.h"

typedef enum
{
    APP_IO_WORKSPACE_OWNER_FREE = 0,
    APP_IO_WORKSPACE_OWNER_METROLOGY,
    APP_IO_WORKSPACE_OWNER_CALIBRATION_STORE,
} app_io_workspace_owner_t;

typedef union
{
    uint32_t metrology_raw_words[HW_METROLOGY_RAW_WORD_COUNT];
    uint8_t calibration_frame[MEASUREMENT_CAL_MAX_FRAME_BYTES];
} app_io_workspace_storage_t;

typedef struct
{
    app_io_workspace_storage_t storage;
    app_io_workspace_owner_t owner;
} app_io_workspace_t;

void app_io_workspace_init(app_io_workspace_t *workspace);
bsp_status_t app_io_workspace_acquire(app_io_workspace_t *workspace,
                                      app_io_workspace_owner_t owner);
bsp_status_t app_io_workspace_release(app_io_workspace_t *workspace,
                                      app_io_workspace_owner_t owner);
app_io_workspace_owner_t app_io_workspace_owner(const app_io_workspace_t *workspace);
uint32_t *app_io_workspace_metrology_raw_words(app_io_workspace_t *workspace);
uint8_t *app_io_workspace_calibration_frame(app_io_workspace_t *workspace);
size_t app_io_workspace_metrology_word_count(void);
size_t app_io_workspace_calibration_frame_bytes(void);
uint32_t app_io_workspace_storage_size_bytes(void);
uint32_t app_io_workspace_context_size_bytes(void);

#endif
