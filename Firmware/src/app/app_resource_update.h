#ifndef WTK_APP_RESOURCE_UPDATE_H
#define WTK_APP_RESOURCE_UPDATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/app_pc_link_protocol.h"
#include "bsp/bsp_status.h"
#include "storage/storage_layout.h"

enum
{
    APP_RESOURCE_UPDATE_PAGE_BYTES = 256u,
};

typedef enum
{
    APP_RESOURCE_UPDATE_IDLE = 0,
    APP_RESOURCE_UPDATE_ERASE_START,
    APP_RESOURCE_UPDATE_ERASE_POLL,
    APP_RESOURCE_UPDATE_READY,
    APP_RESOURCE_UPDATE_PROGRAM_START,
    APP_RESOURCE_UPDATE_PROGRAM_POLL,
    APP_RESOURCE_UPDATE_COMPLETE,
    APP_RESOURCE_UPDATE_ERROR,
} app_resource_update_state_t;

typedef enum
{
    APP_RESOURCE_UPDATE_STATUS_OK = 0,
    APP_RESOURCE_UPDATE_STATUS_BUSY,
    APP_RESOURCE_UPDATE_STATUS_COMPLETE,
    APP_RESOURCE_UPDATE_STATUS_INVALID_ARG,
    APP_RESOURCE_UPDATE_STATUS_PROTOCOL,
    APP_RESOURCE_UPDATE_STATUS_OUT_OF_RANGE,
    APP_RESOURCE_UPDATE_STATUS_FLASH,
} app_resource_update_status_t;

typedef bsp_status_t (*app_resource_update_erase_start_fn)(uint32_t address,
                                                           uint32_t now_ms,
                                                           void *user);
typedef bsp_status_t (*app_resource_update_program_start_fn)(uint32_t address,
                                                             const void *src,
                                                             size_t size,
                                                             uint32_t now_ms,
                                                             void *user);
typedef bsp_status_t (*app_resource_update_poll_fn)(uint32_t now_ms, void *user);

typedef struct
{
    app_resource_update_erase_start_fn erase_start;
    app_resource_update_program_start_fn program_start;
    app_resource_update_poll_fn poll;
    void *user;
} app_resource_update_io_t;

typedef struct
{
    app_resource_update_io_t io;
    storage_partition_t partition;
    uint8_t *scratch;
    size_t scratch_capacity;
    app_pc_link_resource_session_t pc_session;
    app_resource_update_state_t state;
    app_resource_update_status_t last_error;
    app_pc_link_status_t last_protocol_status;
    uint32_t erase_offset;
    uint32_t erase_total;
    uint32_t chunk_offset;
    uint16_t chunk_payload_length;
    uint16_t chunk_written;
    uint16_t active_program_size;
} app_resource_update_t;

void app_resource_update_init(app_resource_update_t *update,
                              const app_resource_update_io_t *io,
                              const storage_partition_t *partition,
                              uint8_t *scratch,
                              size_t scratch_capacity);
app_resource_update_status_t app_resource_update_accept_frame(app_resource_update_t *update,
                                                              const app_pc_link_frame_t *frame,
                                                              uint32_t now_ms);
app_resource_update_status_t app_resource_update_step(app_resource_update_t *update,
                                                      uint32_t now_ms);
void app_resource_update_abort(app_resource_update_t *update);
bool app_resource_update_busy(const app_resource_update_t *update);
bool app_resource_update_complete(const app_resource_update_t *update);
app_resource_update_state_t app_resource_update_state(const app_resource_update_t *update);
app_resource_update_status_t app_resource_update_last_error(const app_resource_update_t *update);
app_pc_link_status_t app_resource_update_last_protocol_status(const app_resource_update_t *update);
uint32_t app_resource_update_context_size_bytes(void);
const char *app_resource_update_status_string(app_resource_update_status_t status);

#endif
