#include "storage/app_settings_w25q_adapter.h"

#include "storage/measurement_cal_w25q_adapter.h"

static bsp_status_t adapter_read(uint32_t address, void *dst, size_t size, void *user)
{
    return measurement_cal_w25q_status_to_bsp(
        w25q_device_read((const w25q_device_t *)user, address, dst, size));
}

static bsp_status_t adapter_erase(uint32_t address, uint32_t now_ms, void *user)
{
    return measurement_cal_w25q_status_to_bsp(
        w25q_device_sector_erase_start((w25q_device_t *)user, address, now_ms));
}

static bsp_status_t adapter_program(uint32_t address,
                                    const void *src,
                                    size_t size,
                                    uint32_t now_ms,
                                    void *user)
{
    return measurement_cal_w25q_status_to_bsp(
        w25q_device_page_program_start((w25q_device_t *)user, address, src, size, now_ms));
}

static bsp_status_t adapter_poll(uint32_t now_ms, void *user)
{
    return measurement_cal_w25q_status_to_bsp(w25q_device_poll((w25q_device_t *)user, now_ms));
}

app_settings_store_io_t app_settings_w25q_store_io(w25q_device_t *flash)
{
    const app_settings_store_io_t io = {
        .read = adapter_read,
        .erase_sector_start = adapter_erase,
        .program_start = adapter_program,
        .poll = adapter_poll,
        .user = flash,
    };
    return io;
}
