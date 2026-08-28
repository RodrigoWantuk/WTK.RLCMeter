#include "app/app_settings_service.h"

#include <string.h>

#include "storage/storage_crc32.h"

#define SETTINGS_MAGIC UINT32_C(0x57545331)
#define SETTINGS_COMMIT_MARKER UINT32_C(0xC05E77ED)

enum
{
    SETTINGS_SCHEMA_VERSION = 1u,
    SETTINGS_HEADER_SIZE = 24u,
    SETTINGS_PAYLOAD_OFFSET = 24u,
    SETTINGS_PAYLOAD_SIZE = 8u,
    SETTINGS_COMMIT_OFFSET = 32u,
    SETTINGS_COMMIT_SIZE = 4u,
    SETTINGS_FRAME_SIZE = 36u,
    SETTINGS_CONTEXT_BUDGET_BYTES = 256u,
};

_Static_assert(sizeof(app_settings_service_t) <= SETTINGS_CONTEXT_BUDGET_BYTES,
               "settings service exceeded SRAM budget");

static void put_u16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8u) & 0xFFu);
}

static void put_u32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8u) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16u) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24u) & 0xFFu);
}

static uint16_t get_u16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8u));
}

static uint32_t get_u32(const uint8_t *src)
{
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8u) |
           ((uint32_t)src[2] << 16u) |
           ((uint32_t)src[3] << 24u);
}

static bool io_valid(const app_settings_store_io_t *io)
{
    return (io != NULL) &&
           (io->read != NULL) &&
           (io->erase_sector_start != NULL) &&
           (io->program_start != NULL) &&
           (io->poll != NULL);
}

static bool sequence_newer(uint32_t a, uint32_t b)
{
    return (a != b) && ((uint32_t)(a - b) < 0x80000000u);
}

static bool settings_equal(const app_settings_t *a, const app_settings_t *b)
{
    return (a != NULL) && (b != NULL) &&
           (a->brightness_percent == b->brightness_percent) &&
           (a->backlight_timeout == b->backlight_timeout) &&
           (a->sound_enabled == b->sound_enabled);
}

app_settings_t app_settings_defaults(void)
{
    return (app_settings_t){
        .brightness_percent = 25u,
        .backlight_timeout = APP_BACKLIGHT_TIMEOUT_60S,
        .sound_enabled = true,
    };
}

bool app_backlight_timeout_valid(app_backlight_timeout_t timeout)
{
    switch (timeout)
    {
    case APP_BACKLIGHT_TIMEOUT_OFF:
    case APP_BACKLIGHT_TIMEOUT_15S:
    case APP_BACKLIGHT_TIMEOUT_30S:
    case APP_BACKLIGHT_TIMEOUT_60S:
    case APP_BACKLIGHT_TIMEOUT_120S:
    case APP_BACKLIGHT_TIMEOUT_300S:
        return true;
    default:
        return false;
    }
}

app_backlight_timeout_t app_backlight_timeout_next(app_backlight_timeout_t timeout)
{
    switch (timeout)
    {
    case APP_BACKLIGHT_TIMEOUT_OFF:
        return APP_BACKLIGHT_TIMEOUT_15S;
    case APP_BACKLIGHT_TIMEOUT_15S:
        return APP_BACKLIGHT_TIMEOUT_30S;
    case APP_BACKLIGHT_TIMEOUT_30S:
        return APP_BACKLIGHT_TIMEOUT_60S;
    case APP_BACKLIGHT_TIMEOUT_60S:
        return APP_BACKLIGHT_TIMEOUT_120S;
    case APP_BACKLIGHT_TIMEOUT_120S:
        return APP_BACKLIGHT_TIMEOUT_300S;
    case APP_BACKLIGHT_TIMEOUT_300S:
    default:
        return APP_BACKLIGHT_TIMEOUT_OFF;
    }
}

app_backlight_timeout_t app_backlight_timeout_prev(app_backlight_timeout_t timeout)
{
    switch (timeout)
    {
    case APP_BACKLIGHT_TIMEOUT_OFF:
        return APP_BACKLIGHT_TIMEOUT_300S;
    case APP_BACKLIGHT_TIMEOUT_15S:
        return APP_BACKLIGHT_TIMEOUT_OFF;
    case APP_BACKLIGHT_TIMEOUT_30S:
        return APP_BACKLIGHT_TIMEOUT_15S;
    case APP_BACKLIGHT_TIMEOUT_60S:
        return APP_BACKLIGHT_TIMEOUT_30S;
    case APP_BACKLIGHT_TIMEOUT_120S:
        return APP_BACKLIGHT_TIMEOUT_60S;
    case APP_BACKLIGHT_TIMEOUT_300S:
    default:
        return APP_BACKLIGHT_TIMEOUT_120S;
    }
}

bool app_settings_validate(const app_settings_t *settings)
{
    return (settings != NULL) &&
           (settings->brightness_percent >= 5u) &&
           (settings->brightness_percent <= 100u) &&
           app_backlight_timeout_valid(settings->backlight_timeout);
}

static void serialize_payload(uint8_t *payload, const app_settings_t *settings)
{
    payload[0] = settings->brightness_percent;
    payload[1] = settings->sound_enabled ? 1u : 0u;
    payload[2] = 0u;
    payload[3] = 0u;
    put_u32(&payload[4], (uint32_t)settings->backlight_timeout);
}

static bool decode_payload(const uint8_t *payload, app_settings_t *settings)
{
    if ((payload == NULL) || (settings == NULL) || (payload[1] > 1u) ||
        (payload[2] != 0u) || (payload[3] != 0u))
    {
        return false;
    }
    *settings = (app_settings_t){
        .brightness_percent = payload[0],
        .sound_enabled = payload[1] != 0u,
        .backlight_timeout = (app_backlight_timeout_t)get_u32(&payload[4]),
    };
    return app_settings_validate(settings);
}

static void serialize_frame(uint8_t frame[SETTINGS_FRAME_SIZE],
                            const app_settings_t *settings,
                            uint32_t sequence,
                            bool committed)
{
    memset(frame, 0, SETTINGS_FRAME_SIZE);
    uint8_t payload[SETTINGS_PAYLOAD_SIZE] = {0};
    serialize_payload(payload, settings);
    put_u32(&frame[0], SETTINGS_MAGIC);
    put_u16(&frame[4], SETTINGS_SCHEMA_VERSION);
    put_u16(&frame[6], SETTINGS_HEADER_SIZE);
    put_u16(&frame[8], SETTINGS_FRAME_SIZE);
    put_u16(&frame[10], SETTINGS_PAYLOAD_SIZE);
    put_u32(&frame[12], sequence);
    put_u32(&frame[16], storage_crc32(payload, SETTINGS_PAYLOAD_SIZE));
    put_u32(&frame[20], 0u);
    memcpy(&frame[SETTINGS_PAYLOAD_OFFSET], payload, SETTINGS_PAYLOAD_SIZE);
    put_u32(&frame[SETTINGS_COMMIT_OFFSET], committed ? SETTINGS_COMMIT_MARKER : UINT32_MAX);
}

static app_settings_slot_status_t inspect_frame(const uint8_t frame[SETTINGS_FRAME_SIZE],
                                                bool require_commit,
                                                app_settings_t *settings,
                                                uint32_t *sequence)
{
    bool erased = true;
    for (size_t i = 0u; i < SETTINGS_FRAME_SIZE; i++)
    {
        erased = erased && (frame[i] == 0xFFu);
    }
    if (erased)
    {
        return APP_SETTINGS_SLOT_ERASED;
    }
    if (get_u32(&frame[0]) != SETTINGS_MAGIC)
    {
        return APP_SETTINGS_SLOT_CORRUPT;
    }
    if ((get_u16(&frame[4]) != SETTINGS_SCHEMA_VERSION) ||
        (get_u16(&frame[6]) != SETTINGS_HEADER_SIZE) ||
        (get_u16(&frame[8]) != SETTINGS_FRAME_SIZE) ||
        (get_u16(&frame[10]) != SETTINGS_PAYLOAD_SIZE))
    {
        return APP_SETTINGS_SLOT_INCOMPATIBLE;
    }
    if (require_commit && (get_u32(&frame[SETTINGS_COMMIT_OFFSET]) != SETTINGS_COMMIT_MARKER))
    {
        return APP_SETTINGS_SLOT_CORRUPT;
    }
    if (get_u32(&frame[16]) != storage_crc32(&frame[SETTINGS_PAYLOAD_OFFSET], SETTINGS_PAYLOAD_SIZE))
    {
        return APP_SETTINGS_SLOT_CORRUPT;
    }
    app_settings_t decoded;
    if (!decode_payload(&frame[SETTINGS_PAYLOAD_OFFSET], &decoded))
    {
        return APP_SETTINGS_SLOT_SEMANTIC_INVALID;
    }
    if (settings != NULL)
    {
        *settings = decoded;
    }
    if (sequence != NULL)
    {
        *sequence = get_u32(&frame[12]);
    }
    return APP_SETTINGS_SLOT_VALID;
}

static bsp_status_t read_slot(app_settings_service_t *service,
                              app_settings_slot_t slot,
                              app_settings_slot_info_t *info)
{
    if ((service == NULL) || (slot > APP_SETTINGS_SLOT_B))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    bsp_status_t status = service->io.read(service->slots[(uint8_t)slot].start,
                                           service->verify,
                                           SETTINGS_FRAME_SIZE,
                                           service->io.user);
    if (status != BSP_STATUS_OK)
    {
        if (info != NULL)
        {
            *info = (app_settings_slot_info_t){.status = APP_SETTINGS_SLOT_CORRUPT, .slot = slot};
        }
        return status;
    }
    app_settings_t settings = app_settings_defaults();
    uint32_t sequence = 0u;
    const app_settings_slot_status_t slot_status =
        inspect_frame(service->verify, true, &settings, &sequence);
    if (info != NULL)
    {
        *info = (app_settings_slot_info_t){
            .status = slot_status,
            .slot = slot,
            .sequence = sequence,
            .settings = settings,
        };
    }
    return BSP_STATUS_OK;
}

void app_settings_service_use_defaults(app_settings_service_t *service)
{
    if (service != NULL)
    {
        service->initialized = true;
        service->current = app_settings_defaults();
        service->dirty = false;
        service->save_failed = false;
        service->status = service->storage_available ?
                              APP_SETTINGS_STATUS_DEFAULTS :
                              APP_SETTINGS_STATUS_STORAGE_UNAVAILABLE;
    }
}

bsp_status_t app_settings_service_init(app_settings_service_t *service,
                                       const app_settings_store_io_t *io,
                                       uint32_t capacity_bytes)
{
    if ((service == NULL) || !io_valid(io))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *service = (app_settings_service_t){0};
    service->io = *io;
    service->current = app_settings_defaults();
    service->persisted = service->current;
    service->last_error = BSP_STATUS_OK;
    service->storage_available =
        storage_layout_partition(capacity_bytes, STORAGE_PARTITION_SETTINGS_A, &service->slots[0]) &&
        storage_layout_partition(capacity_bytes, STORAGE_PARTITION_SETTINGS_B, &service->slots[1]);
    if (!service->storage_available)
    {
        service->state = APP_SETTINGS_STORE_ERROR;
        service->status = APP_SETTINGS_STATUS_STORAGE_UNAVAILABLE;
        service->last_error = BSP_STATUS_NOT_SUPPORTED;
        service->initialized = true;
        return BSP_STATUS_NOT_SUPPORTED;
    }
    service->state = APP_SETTINGS_STORE_IDLE;
    service->status = APP_SETTINGS_STATUS_DEFAULTS;
    service->initialized = true;
    return BSP_STATUS_OK;
}

bsp_status_t app_settings_service_load(app_settings_service_t *service,
                                       app_settings_slot_info_t diagnostics[2])
{
    if ((service == NULL) || !service->initialized)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!service->storage_available)
    {
        app_settings_service_use_defaults(service);
        return BSP_STATUS_NOT_SUPPORTED;
    }
    app_settings_slot_info_t local[2];
    bsp_status_t status_a = read_slot(service, APP_SETTINGS_SLOT_A, &local[0]);
    bsp_status_t status_b = read_slot(service, APP_SETTINGS_SLOT_B, &local[1]);
    if (diagnostics != NULL)
    {
        diagnostics[0] = local[0];
        diagnostics[1] = local[1];
    }
    const bool valid_a = (status_a == BSP_STATUS_OK) && (local[0].status == APP_SETTINGS_SLOT_VALID);
    const bool valid_b = (status_b == BSP_STATUS_OK) && (local[1].status == APP_SETTINGS_SLOT_VALID);
    if (!valid_a && !valid_b)
    {
        app_settings_service_use_defaults(service);
        return ((status_a == BSP_STATUS_OK) || (status_b == BSP_STATUS_OK)) ? BSP_STATUS_OK : BSP_STATUS_ERROR;
    }
    const app_settings_slot_info_t *chosen = &local[0];
    if (valid_b && (!valid_a || sequence_newer(local[1].sequence, local[0].sequence)))
    {
        chosen = &local[1];
    }
    service->current = chosen->settings;
    service->persisted = chosen->settings;
    service->persisted_sequence = chosen->sequence;
    service->active_slot = chosen->slot;
    service->persisted_valid = true;
    service->dirty = false;
    service->save_failed = false;
    service->status = APP_SETTINGS_STATUS_LOADED;
    service->last_error = BSP_STATUS_OK;
    return BSP_STATUS_OK;
}

bsp_status_t app_settings_service_set(app_settings_service_t *service,
                                      const app_settings_t *settings)
{
    if ((service == NULL) || (settings == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!app_settings_validate(settings))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    service->current = *settings;
    service->dirty = !service->persisted_valid ||
                     !settings_equal(&service->current, &service->persisted);
    service->save_failed = false;
    service->status = service->dirty ? APP_SETTINGS_STATUS_DIRTY :
                                      (service->persisted_valid ? APP_SETTINGS_STATUS_LOADED :
                                                                  APP_SETTINGS_STATUS_DEFAULTS);
    return BSP_STATUS_OK;
}

bsp_status_t app_settings_service_save_start(app_settings_service_t *service, uint32_t now_ms)
{
    (void)now_ms;
    if ((service == NULL) || !service->initialized)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (app_settings_service_busy(service))
    {
        return BSP_STATUS_BUSY;
    }
    if (!service->dirty)
    {
        return BSP_STATUS_OK;
    }
    if (!service->storage_available)
    {
        service->save_failed = true;
        service->status = APP_SETTINGS_STATUS_SAVE_FAILED;
        service->last_error = BSP_STATUS_NOT_SUPPORTED;
        return BSP_STATUS_NOT_SUPPORTED;
    }
    service->target_slot =
        (!service->persisted_valid || (service->active_slot == APP_SETTINGS_SLOT_B)) ?
            APP_SETTINGS_SLOT_A :
            APP_SETTINGS_SLOT_B;
    service->target_sequence = service->persisted_valid ? (service->persisted_sequence + 1u) : 1u;
    serialize_frame(service->frame, &service->current, service->target_sequence, false);
    service->state = APP_SETTINGS_STORE_ERASE_START;
    service->status = APP_SETTINGS_STATUS_SAVING;
    service->last_error = BSP_STATUS_OK;
    return BSP_STATUS_OK;
}

static void set_error(app_settings_service_t *service, bsp_status_t status)
{
    service->state = APP_SETTINGS_STORE_ERROR;
    service->status = APP_SETTINGS_STATUS_SAVE_FAILED;
    service->last_error = status;
    service->save_failed = true;
}

bsp_status_t app_settings_service_step(app_settings_service_t *service, uint32_t now_ms)
{
    if ((service == NULL) || !service->initialized)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!app_settings_service_busy(service))
    {
        return BSP_STATUS_OK;
    }
    const uint32_t base = service->slots[(uint8_t)service->target_slot].start;
    bsp_status_t status = BSP_STATUS_OK;
    switch (service->state)
    {
    case APP_SETTINGS_STORE_ERASE_START:
        status = service->io.erase_sector_start(base, now_ms, service->io.user);
        service->state = (status == BSP_STATUS_OK) ? APP_SETTINGS_STORE_ERASE_WAIT : service->state;
        break;
    case APP_SETTINGS_STORE_ERASE_WAIT:
        status = service->io.poll(now_ms, service->io.user);
        if (status == BSP_STATUS_OK)
        {
            service->state = APP_SETTINGS_STORE_PROGRAM_HEADER_START;
        }
        break;
    case APP_SETTINGS_STORE_PROGRAM_HEADER_START:
        status = service->io.program_start(base, service->frame, SETTINGS_HEADER_SIZE, now_ms, service->io.user);
        service->state = (status == BSP_STATUS_OK) ? APP_SETTINGS_STORE_PROGRAM_HEADER_WAIT : service->state;
        break;
    case APP_SETTINGS_STORE_PROGRAM_HEADER_WAIT:
        status = service->io.poll(now_ms, service->io.user);
        if (status == BSP_STATUS_OK)
        {
            service->state = APP_SETTINGS_STORE_PROGRAM_PAYLOAD_START;
        }
        break;
    case APP_SETTINGS_STORE_PROGRAM_PAYLOAD_START:
        status = service->io.program_start(base + SETTINGS_PAYLOAD_OFFSET,
                                           &service->frame[SETTINGS_PAYLOAD_OFFSET],
                                           SETTINGS_PAYLOAD_SIZE,
                                           now_ms,
                                           service->io.user);
        service->state = (status == BSP_STATUS_OK) ? APP_SETTINGS_STORE_PROGRAM_PAYLOAD_WAIT : service->state;
        break;
    case APP_SETTINGS_STORE_PROGRAM_PAYLOAD_WAIT:
        status = service->io.poll(now_ms, service->io.user);
        if (status == BSP_STATUS_OK)
        {
            service->state = APP_SETTINGS_STORE_VERIFY_PRE_COMMIT;
        }
        break;
    case APP_SETTINGS_STORE_VERIFY_PRE_COMMIT:
        status = service->io.read(base, service->verify, SETTINGS_FRAME_SIZE, service->io.user);
        if (status == BSP_STATUS_OK)
        {
            app_settings_t decoded;
            uint32_t sequence = 0u;
            if ((memcmp(service->verify, service->frame, SETTINGS_COMMIT_OFFSET) != 0) ||
                (inspect_frame(service->verify, false, &decoded, &sequence) != APP_SETTINGS_SLOT_VALID) ||
                (sequence != service->target_sequence) ||
                (get_u32(&service->verify[SETTINGS_COMMIT_OFFSET]) == SETTINGS_COMMIT_MARKER))
            {
                status = BSP_STATUS_ERROR;
            }
            else
            {
                service->state = APP_SETTINGS_STORE_PROGRAM_COMMIT_START;
            }
        }
        break;
    case APP_SETTINGS_STORE_PROGRAM_COMMIT_START:
        put_u32(&service->frame[SETTINGS_COMMIT_OFFSET], SETTINGS_COMMIT_MARKER);
        status = service->io.program_start(base + SETTINGS_COMMIT_OFFSET,
                                           &service->frame[SETTINGS_COMMIT_OFFSET],
                                           SETTINGS_COMMIT_SIZE,
                                           now_ms,
                                           service->io.user);
        service->state = (status == BSP_STATUS_OK) ? APP_SETTINGS_STORE_PROGRAM_COMMIT_WAIT : service->state;
        break;
    case APP_SETTINGS_STORE_PROGRAM_COMMIT_WAIT:
        status = service->io.poll(now_ms, service->io.user);
        if (status == BSP_STATUS_OK)
        {
            service->state = APP_SETTINGS_STORE_VERIFY_COMMIT;
        }
        break;
    case APP_SETTINGS_STORE_VERIFY_COMMIT:
        status = service->io.read(base, service->verify, SETTINGS_FRAME_SIZE, service->io.user);
        if (status == BSP_STATUS_OK)
        {
            app_settings_t decoded;
            uint32_t sequence = 0u;
            if ((inspect_frame(service->verify, true, &decoded, &sequence) != APP_SETTINGS_SLOT_VALID) ||
                (sequence != service->target_sequence) ||
                !settings_equal(&decoded, &service->current))
            {
                status = BSP_STATUS_ERROR;
            }
            else
            {
                service->persisted = decoded;
                service->persisted_sequence = sequence;
                service->active_slot = service->target_slot;
                service->persisted_valid = true;
                service->dirty = false;
                service->save_failed = false;
                service->state = APP_SETTINGS_STORE_DONE;
                service->status = APP_SETTINGS_STATUS_LOADED;
            }
        }
        break;
    default:
        status = BSP_STATUS_ERROR;
        break;
    }
    if (status == BSP_STATUS_BUSY)
    {
        return BSP_STATUS_BUSY;
    }
    if (status != BSP_STATUS_OK)
    {
        set_error(service, status);
        return status;
    }
    return app_settings_service_busy(service) ? BSP_STATUS_BUSY : BSP_STATUS_OK;
}

bsp_status_t app_settings_service_acknowledge(app_settings_service_t *service)
{
    if (service == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (service->state == APP_SETTINGS_STORE_DONE)
    {
        service->state = APP_SETTINGS_STORE_IDLE;
        service->last_error = BSP_STATUS_OK;
        service->status = service->dirty ? APP_SETTINGS_STATUS_DIRTY : APP_SETTINGS_STATUS_LOADED;
        return BSP_STATUS_OK;
    }
    if (service->state == APP_SETTINGS_STORE_ERROR)
    {
        service->state = APP_SETTINGS_STORE_IDLE;
        service->status = service->save_failed ? APP_SETTINGS_STATUS_SAVE_FAILED : APP_SETTINGS_STATUS_DEFAULTS;
        return BSP_STATUS_OK;
    }
    return app_settings_service_busy(service) ? BSP_STATUS_BUSY : BSP_STATUS_OK;
}

const app_settings_t *app_settings_service_current(const app_settings_service_t *service)
{
    return (service == NULL) ? NULL : &service->current;
}

app_settings_status_t app_settings_service_status(const app_settings_service_t *service)
{
    return (service == NULL) ? APP_SETTINGS_STATUS_STORAGE_UNAVAILABLE : service->status;
}

app_settings_store_state_t app_settings_service_store_state(const app_settings_service_t *service)
{
    return (service == NULL) ? APP_SETTINGS_STORE_ERROR : service->state;
}

bsp_status_t app_settings_service_last_error(const app_settings_service_t *service)
{
    return (service == NULL) ? BSP_STATUS_INVALID_ARG : service->last_error;
}

bool app_settings_service_busy(const app_settings_service_t *service)
{
    if (service == NULL)
    {
        return false;
    }
    return (service->state >= APP_SETTINGS_STORE_ERASE_START) &&
           (service->state <= APP_SETTINGS_STORE_VERIFY_COMMIT);
}

bool app_settings_service_dirty(const app_settings_service_t *service)
{
    return (service != NULL) && service->dirty;
}

bool app_settings_service_save_failed(const app_settings_service_t *service)
{
    return (service != NULL) && service->save_failed;
}

bool app_settings_service_storage_available(const app_settings_service_t *service)
{
    return (service != NULL) && service->storage_available;
}

uint32_t app_settings_service_persisted_sequence(const app_settings_service_t *service)
{
    return (service == NULL) ? 0u : service->persisted_sequence;
}

app_settings_slot_t app_settings_service_active_slot(const app_settings_service_t *service)
{
    return (service == NULL) ? APP_SETTINGS_SLOT_A : service->active_slot;
}

uint32_t app_settings_service_context_size_bytes(void)
{
    return (uint32_t)sizeof(app_settings_service_t);
}
