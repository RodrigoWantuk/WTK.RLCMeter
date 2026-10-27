#ifndef WTK_LEAD_PROFILE_STORE_H
#define WTK_LEAD_PROFILE_STORE_H
#include "storage/storage_async_io.h"
#include "storage/storage_layout.h"
#include "measurement/measurement_lead_compensation.h"
enum { LEAD_PROFILE_FRAME_BYTES = 100u, LEAD_PROFILE_COMMIT_OFFSET = 96u };
typedef enum
{
    LEAD_STORE_IDLE = 0, LEAD_STORE_ERASE_START, LEAD_STORE_ERASE_WAIT,
    LEAD_STORE_WRITE_START, LEAD_STORE_WRITE_WAIT, LEAD_STORE_VERIFY,
    LEAD_STORE_COMMIT_START, LEAD_STORE_COMMIT_WAIT, LEAD_STORE_VERIFY_COMMIT,
    LEAD_STORE_DONE, LEAD_STORE_ERROR
} lead_store_state_t;
typedef struct
{
    storage_async_io_t io;
    storage_partition_t slots[2];
    measurement_lead_profile_t active;
    uint8_t frame[LEAD_PROFILE_FRAME_BYTES], verify[LEAD_PROFILE_FRAME_BYTES];
    uint32_t started_ms;
    lead_store_state_t state;
    bsp_status_t last_error;
    uint8_t active_slot, target_slot;
    bool initialized, loaded, active_valid, commit_uncertain;
} lead_profile_store_t;
bool lead_profile_encode(const measurement_lead_profile_t *profile, bool committed,
                          uint8_t frame[LEAD_PROFILE_FRAME_BYTES]);
bool lead_profile_decode(const uint8_t frame[LEAD_PROFILE_FRAME_BYTES], bool require_commit,
                          measurement_lead_profile_t *profile);
bsp_status_t lead_profile_store_init(lead_profile_store_t *store, const storage_async_io_t *io, uint32_t capacity);
bsp_status_t lead_profile_store_load(lead_profile_store_t *store);
bsp_status_t lead_profile_store_save(lead_profile_store_t *store, const measurement_lead_profile_t *profile, uint32_t now_ms);
bsp_status_t lead_profile_store_step(lead_profile_store_t *store, uint32_t now_ms);
bool lead_profile_store_busy(const lead_profile_store_t *store);
#endif
