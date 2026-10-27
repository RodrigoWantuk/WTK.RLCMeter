#include <assert.h>
#include <string.h>
#include "app/app_calibration_service.h"

static uint8_t slots[8192];
static unsigned reads, fail_read;
static bsp_status_t read_flash(uint32_t address, void *dst, size_t bytes, void *user)
{
    (void)user;
    ++reads;
    if (reads == fail_read) return BSP_STATUS_ERROR;
    assert(address <= sizeof(slots) - bytes);
    memcpy(dst, slots + address, bytes);
    return BSP_STATUS_OK;
}
static void fill_slot(unsigned slot, uint32_t sequence, bool complete)
{
    measurement_cal_set_t set;
    measurement_cal_set_init(&set, MEASUREMENT_CAL_HARDWARE_REV1, MEASUREMENT_CAL_MODEL_VERSION_CURRENT, sequence);
    for (hw_range_id_t r = HW_RANGE_ID_10R; r <= HW_RANGE_ID_1M; ++r)
        for (hw_excitation_freq_t f = HW_EXCITATION_FREQ_10HZ; f <= HW_EXCITATION_FREQ_10KHZ; ++f)
            for (hw_excitation_amp_t a = HW_EXCITATION_AMP_100MVRMS; a <= HW_EXCITATION_AMP_500MVRMS; ++a)
                if (measurement_cal_condition_allowed(r, f, a))
                {
                    const measurement_cal_key_t key = measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                        MEASUREMENT_CAL_MODEL_VERSION_CURRENT, r, f, a);
                    measurement_cal_record_t record = measurement_cal_make_ideal_record(&key);
                    record.correction.load_z_ohms = measurement_dsp_config_ideal(r).zref_ohms;
                    record.correction.flags |= MEASUREMENT_CAL_FLAG_QUALIFIED;
                    assert(measurement_cal_set_add_record(&set, &record));
                }
    assert(measurement_cal_validate_rev1_full_set(&set).status == MEASUREMENT_CAL_VALIDITY_VALID);
    if (!complete) --set.record_count;
    size_t written = 0u;
    assert(measurement_cal_serialize_set(&set, slots + slot*4096u, 4096u, &written));
}
int main(void)
{
    _Static_assert(sizeof(app_calibration_service_t) < sizeof(measurement_cal_set_t) + 512u,
                   "factory service must not duplicate the active set");
    memset(slots, 255, sizeof(slots));
    app_calibration_service_t service;
    app_io_workspace_t workspace;
    app_io_workspace_init(&workspace);
    app_calibration_service_init(&service);
    app_calibration_service_attach_workspace(&service, &workspace);
    const measurement_cal_store_io_t io = {.read = read_flash};
    fill_slot(0u, 5u, true);
    fill_slot(1u, 6u, false);
    assert(app_calibration_service_load(&service, &io, 32768u) == BSP_STATUS_OK);
    assert(app_calibration_service_active_sequence(&service) == 5u);
    assert(app_io_workspace_owner(&workspace) == APP_IO_WORKSPACE_OWNER_FREE);
    fill_slot(1u, 6u, true);
    assert(app_calibration_service_load(&service, &io, 32768u) == BSP_STATUS_OK);
    assert(app_calibration_service_active_sequence(&service) == 6u);
    slots[4096u + 100u] ^= 0x01u;
    assert(app_calibration_service_load(&service, &io, 32768u) == BSP_STATUS_OK);
    assert(app_calibration_service_active_sequence(&service) == 5u);
    reads = 0u;
    fail_read = 3u;
    assert(app_calibration_service_load(&service, &io, 32768u) == BSP_STATUS_ERROR);
    assert(!app_calibration_service_active_valid(&service));
    assert(app_io_workspace_owner(&workspace) == APP_IO_WORKSPACE_OWNER_FREE);
    assert(app_io_workspace_acquire(&workspace, APP_IO_WORKSPACE_OWNER_METROLOGY) == BSP_STATUS_OK);
    assert(app_calibration_service_load(&service, &io, 32768u) == BSP_STATUS_BUSY);
    return 0;
}
