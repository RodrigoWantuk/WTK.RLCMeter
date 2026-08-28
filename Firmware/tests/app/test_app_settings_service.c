#include "app/app_settings_service.h"

#include <stdio.h>
#include <string.h>

enum
{
    FAKE_FLASH_BYTES = STORAGE_LAYOUT_MUTABLE_RESERVED_BYTES,
    FAKE_SECTOR_BYTES = STORAGE_LAYOUT_W25Q_SECTOR_SIZE,
};

typedef struct
{
    uint8_t bytes[FAKE_FLASH_BYTES];
    uint32_t erase_calls;
    uint32_t program_calls;
    uint32_t poll_calls;
    uint32_t read_calls;
    uint32_t fail_erase_call;
    uint32_t fail_program_call;
    uint32_t fail_poll_call;
    uint32_t fail_read_call;
} fake_flash_t;

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int expect_u32(uint32_t actual, uint32_t expected, const char *message)
{
    if (actual != expected)
    {
        (void)fprintf(stderr, "FAIL: %s (got %lu expected %lu)\n",
                      message,
                      (unsigned long)actual,
                      (unsigned long)expected);
        return 1;
    }
    return 0;
}

static void fake_flash_init(fake_flash_t *fake)
{
    (void)memset(fake, 0, sizeof(*fake));
    (void)memset(fake->bytes, 0xFF, sizeof(fake->bytes));
}

static bsp_status_t fake_read(uint32_t address, void *dst, size_t size, void *user)
{
    fake_flash_t *fake = (fake_flash_t *)user;
    fake->read_calls++;
    if ((fake->fail_read_call != 0u) && (fake->read_calls == fake->fail_read_call))
    {
        return BSP_STATUS_ERROR;
    }
    if ((fake == NULL) || (dst == NULL) || (size > sizeof(fake->bytes)) ||
        (address > (sizeof(fake->bytes) - size)))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    (void)memcpy(dst, &fake->bytes[address], size);
    return BSP_STATUS_OK;
}

static bsp_status_t fake_erase(uint32_t address, uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_flash_t *fake = (fake_flash_t *)user;
    fake->erase_calls++;
    if ((fake->fail_erase_call != 0u) && (fake->erase_calls == fake->fail_erase_call))
    {
        return BSP_STATUS_ERROR;
    }
    if ((address % FAKE_SECTOR_BYTES) != 0u)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    (void)memset(&fake->bytes[address], 0xFF, FAKE_SECTOR_BYTES);
    return BSP_STATUS_OK;
}

static bsp_status_t fake_program(uint32_t address,
                                 const void *src,
                                 size_t size,
                                 uint32_t now_ms,
                                 void *user)
{
    (void)now_ms;
    fake_flash_t *fake = (fake_flash_t *)user;
    fake->program_calls++;
    if ((fake->fail_program_call != 0u) && (fake->program_calls == fake->fail_program_call))
    {
        return BSP_STATUS_ERROR;
    }
    if ((fake == NULL) || (src == NULL) || (size > sizeof(fake->bytes)) ||
        (address > (sizeof(fake->bytes) - size)))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    const uint8_t *in = (const uint8_t *)src;
    for (size_t i = 0u; i < size; i++)
    {
        fake->bytes[address + i] &= in[i];
    }
    return BSP_STATUS_OK;
}

static bsp_status_t fake_poll(uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_flash_t *fake = (fake_flash_t *)user;
    fake->poll_calls++;
    if ((fake->fail_poll_call != 0u) && (fake->poll_calls == fake->fail_poll_call))
    {
        return BSP_STATUS_ERROR;
    }
    return BSP_STATUS_OK;
}

static app_settings_store_io_t make_io(fake_flash_t *fake)
{
    return (app_settings_store_io_t){
        .read = fake_read,
        .erase_sector_start = fake_erase,
        .program_start = fake_program,
        .poll = fake_poll,
        .user = fake,
    };
}

static bsp_status_t drain_save(app_settings_service_t *service)
{
    for (uint32_t now = 0u; now < 32u; now++)
    {
        const bsp_status_t status = app_settings_service_step(service, now);
        if ((status != BSP_STATUS_OK) && (status != BSP_STATUS_BUSY))
        {
            return status;
        }
        if (!app_settings_service_busy(service))
        {
            (void)app_settings_service_acknowledge(service);
            return BSP_STATUS_OK;
        }
    }
    return BSP_STATUS_TIMEOUT;
}

static int test_layout_rebalanced(void)
{
    int failures = 0;
    storage_partition_t cal_a;
    storage_partition_t cal_b;
    storage_partition_t set_a;
    storage_partition_t set_b;
    storage_partition_t diag;
    storage_partition_t bringup;
    failures += expect_true(storage_layout_partition(FAKE_FLASH_BYTES, STORAGE_PARTITION_CALIBRATION_A, &cal_a),
                            "cal A partition");
    failures += expect_true(storage_layout_partition(FAKE_FLASH_BYTES, STORAGE_PARTITION_CALIBRATION_B, &cal_b),
                            "cal B partition");
    failures += expect_true(storage_layout_partition(FAKE_FLASH_BYTES, STORAGE_PARTITION_SETTINGS_A, &set_a),
                            "settings A partition");
    failures += expect_true(storage_layout_partition(FAKE_FLASH_BYTES, STORAGE_PARTITION_SETTINGS_B, &set_b),
                            "settings B partition");
    failures += expect_true(storage_layout_partition(FAKE_FLASH_BYTES, STORAGE_PARTITION_DIAGNOSTICS, &diag),
                            "diagnostics partition");
    failures += expect_true(storage_layout_partition(FAKE_FLASH_BYTES, STORAGE_PARTITION_BRINGUP_TEST, &bringup),
                            "bringup partition");
    failures += expect_u32(cal_a.size, STORAGE_LAYOUT_W25Q_SECTOR_SIZE, "cal A one sector");
    failures += expect_u32(set_a.size, STORAGE_LAYOUT_W25Q_SECTOR_SIZE, "settings A one sector");
    failures += expect_u32(set_b.start, set_a.start + STORAGE_LAYOUT_W25Q_SECTOR_SIZE, "settings slots contiguous");
    failures += expect_u32(diag.size, 3u * STORAGE_LAYOUT_W25Q_SECTOR_SIZE, "diagnostics three sectors");
    failures += expect_u32(bringup.start, FAKE_FLASH_BYTES - STORAGE_LAYOUT_W25Q_SECTOR_SIZE, "bringup still last sector");
    failures += expect_u32(cal_b.start, cal_a.start + STORAGE_LAYOUT_W25Q_SECTOR_SIZE, "cal addresses unchanged");
    return failures;
}

static int test_defaults_validation_and_roundtrip(void)
{
    int failures = 0;
    fake_flash_t fake;
    fake_flash_init(&fake);
    app_settings_store_io_t io = make_io(&fake);
    app_settings_service_t service;
    failures += expect_true(app_settings_service_init(&service, &io, FAKE_FLASH_BYTES) == BSP_STATUS_OK,
                            "settings init");
    failures += expect_true(app_settings_service_load(&service, NULL) == BSP_STATUS_OK,
                            "empty load nonfatal");
    const app_settings_t *settings = app_settings_service_current(&service);
    failures += expect_u32(settings->brightness_percent, 25u, "default brightness");
    failures += expect_u32((uint32_t)settings->backlight_timeout, 60u, "default timeout");
    failures += expect_true(settings->sound_enabled, "default sound enabled");

    app_settings_t changed = {.brightness_percent = 55u,
                              .backlight_timeout = APP_BACKLIGHT_TIMEOUT_120S,
                              .sound_enabled = false};
    failures += expect_true(app_settings_service_set(&service, &changed) == BSP_STATUS_OK,
                            "set valid settings");
    failures += expect_true(app_settings_service_save_start(&service, 0u) == BSP_STATUS_OK,
                            "save start");
    failures += expect_true(drain_save(&service) == BSP_STATUS_OK, "save drains");
    failures += expect_true(!app_settings_service_dirty(&service), "save clears dirty");
    failures += expect_u32(app_settings_service_persisted_sequence(&service), 1u, "sequence one");

    app_settings_service_t loaded;
    failures += expect_true(app_settings_service_init(&loaded, &io, FAKE_FLASH_BYTES) == BSP_STATUS_OK,
                            "reload init");
    failures += expect_true(app_settings_service_load(&loaded, NULL) == BSP_STATUS_OK,
                            "reload succeeds");
    settings = app_settings_service_current(&loaded);
    failures += expect_u32(settings->brightness_percent, 55u, "reload brightness");
    failures += expect_u32((uint32_t)settings->backlight_timeout, 120u, "reload timeout");
    failures += expect_true(!settings->sound_enabled, "reload sound disabled");

    changed.brightness_percent = 4u;
    failures += expect_true(app_settings_service_set(&loaded, &changed) == BSP_STATUS_INVALID_ARG,
                            "brightness under 5 rejected");
    changed = *settings;
    changed.backlight_timeout = (app_backlight_timeout_t)45u;
    failures += expect_true(app_settings_service_set(&loaded, &changed) == BSP_STATUS_INVALID_ARG,
                            "unsupported timeout rejected");
    return failures;
}

static int test_failed_replacement_preserves_previous_slot(void)
{
    int failures = 0;
    fake_flash_t fake;
    fake_flash_init(&fake);
    app_settings_store_io_t io = make_io(&fake);
    app_settings_service_t service;
    app_settings_t first = {.brightness_percent = 35u,
                            .backlight_timeout = APP_BACKLIGHT_TIMEOUT_30S,
                            .sound_enabled = true};
    app_settings_t second = {.brightness_percent = 80u,
                             .backlight_timeout = APP_BACKLIGHT_TIMEOUT_300S,
                             .sound_enabled = false};
    failures += expect_true(app_settings_service_init(&service, &io, FAKE_FLASH_BYTES) == BSP_STATUS_OK,
                            "initial service");
    failures += expect_true(app_settings_service_set(&service, &first) == BSP_STATUS_OK, "first set");
    failures += expect_true(app_settings_service_save_start(&service, 0u) == BSP_STATUS_OK, "first save start");
    failures += expect_true(drain_save(&service) == BSP_STATUS_OK, "first save");
    failures += expect_u32((uint32_t)app_settings_service_active_slot(&service),
                           (uint32_t)APP_SETTINGS_SLOT_A,
                           "first slot A");

    fake.fail_program_call = fake.program_calls + 3u;
    failures += expect_true(app_settings_service_set(&service, &second) == BSP_STATUS_OK, "second set");
    failures += expect_true(app_settings_service_save_start(&service, 10u) == BSP_STATUS_OK, "second save start");
    failures += expect_true(drain_save(&service) == BSP_STATUS_ERROR, "marker failure propagates");
    failures += expect_true(app_settings_service_dirty(&service), "failed write leaves RAM dirty");
    failures += expect_true(app_settings_service_save_failed(&service), "failed write visible");
    const app_settings_t *settings = app_settings_service_current(&service);
    failures += expect_u32(settings->brightness_percent, 80u, "RAM setting remains applied");

    fake.fail_program_call = 0u;
    app_settings_service_t loaded;
    failures += expect_true(app_settings_service_init(&loaded, &io, FAKE_FLASH_BYTES) == BSP_STATUS_OK,
                            "load after failed replacement init");
    failures += expect_true(app_settings_service_load(&loaded, NULL) == BSP_STATUS_OK,
                            "load after failed replacement");
    settings = app_settings_service_current(&loaded);
    failures += expect_u32(settings->brightness_percent, 35u, "previous valid slot remains authoritative");
    failures += expect_true(settings->sound_enabled, "previous sound remains");
    return failures;
}

static int test_failed_writes_stay_dirty(void)
{
    int failures = 0;
    for (uint8_t scenario = 0u; scenario < 5u; scenario++)
    {
        fake_flash_t fake;
        fake_flash_init(&fake);
        if (scenario == 0u)
        {
            fake.fail_erase_call = 1u;
        }
        else if (scenario == 1u)
        {
            fake.fail_program_call = 1u;
        }
        else if (scenario == 2u)
        {
            fake.fail_poll_call = 1u;
        }
        else if (scenario == 3u)
        {
            fake.fail_program_call = 2u;
        }
        else
        {
            fake.fail_read_call = 1u;
        }
        app_settings_store_io_t io = make_io(&fake);
        app_settings_service_t service;
        app_settings_t settings = {.brightness_percent = 60u,
                                   .backlight_timeout = APP_BACKLIGHT_TIMEOUT_15S,
                                   .sound_enabled = false};
        failures += expect_true(app_settings_service_init(&service, &io, FAKE_FLASH_BYTES) == BSP_STATUS_OK,
                                "failure scenario init");
        failures += expect_true(app_settings_service_set(&service, &settings) == BSP_STATUS_OK,
                                "failure scenario set");
        failures += expect_true(app_settings_service_save_start(&service, 0u) == BSP_STATUS_OK,
                                "failure scenario start");
        failures += expect_true(drain_save(&service) != BSP_STATUS_OK,
                                "failure scenario reports error");
        failures += expect_true(app_settings_service_dirty(&service),
                                "failure scenario leaves dirty");
        failures += expect_true(app_settings_service_save_failed(&service),
                                "failure scenario exposes save failure");
        failures += expect_u32(app_settings_service_status(&service),
                               APP_SETTINGS_STATUS_SAVE_FAILED,
                               "failure scenario status");
    }
    return failures;
}

static int test_newest_wrap_safe_slot_selected(void)
{
    int failures = 0;
    fake_flash_t fake;
    fake_flash_init(&fake);
    app_settings_store_io_t io = make_io(&fake);
    app_settings_service_t service;
    app_settings_t a = {.brightness_percent = 10u,
                        .backlight_timeout = APP_BACKLIGHT_TIMEOUT_15S,
                        .sound_enabled = true};
    app_settings_t b = {.brightness_percent = 95u,
                        .backlight_timeout = APP_BACKLIGHT_TIMEOUT_OFF,
                        .sound_enabled = false};
    failures += expect_true(app_settings_service_init(&service, &io, FAKE_FLASH_BYTES) == BSP_STATUS_OK,
                            "wrap init");
    service.persisted_valid = true;
    service.persisted_sequence = UINT32_MAX;
    service.active_slot = APP_SETTINGS_SLOT_B;
    failures += expect_true(app_settings_service_set(&service, &a) == BSP_STATUS_OK, "wrap set A");
    failures += expect_true(app_settings_service_save_start(&service, 0u) == BSP_STATUS_OK, "wrap save A");
    failures += expect_true(drain_save(&service) == BSP_STATUS_OK, "wrap save A done");
    failures += expect_u32(app_settings_service_persisted_sequence(&service), 0u, "sequence wrapped");
    failures += expect_true(app_settings_service_set(&service, &b) == BSP_STATUS_OK, "wrap set B");
    failures += expect_true(app_settings_service_save_start(&service, 10u) == BSP_STATUS_OK, "wrap save B");
    failures += expect_true(drain_save(&service) == BSP_STATUS_OK, "wrap save B done");

    app_settings_service_t loaded;
    failures += expect_true(app_settings_service_init(&loaded, &io, FAKE_FLASH_BYTES) == BSP_STATUS_OK,
                            "wrap load init");
    failures += expect_true(app_settings_service_load(&loaded, NULL) == BSP_STATUS_OK,
                            "wrap load");
    const app_settings_t *settings = app_settings_service_current(&loaded);
    failures += expect_u32(settings->brightness_percent, 95u, "newest wrap-safe slot selected");
    failures += expect_true(!settings->sound_enabled, "newest sound selected");
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_layout_rebalanced();
    failures += test_defaults_validation_and_roundtrip();
    failures += test_failed_replacement_preserves_previous_slot();
    failures += test_failed_writes_stay_dirty();
    failures += test_newest_wrap_safe_slot_selected();
    failures += expect_true(app_settings_service_context_size_bytes() <= 256u,
                            "settings context budget");
    return failures;
}
