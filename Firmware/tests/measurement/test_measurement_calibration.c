#include "measurement/measurement_calibration.h"
#include "measurement/measurement_calibration_store.h"
#include "measurement/measurement_condition.h"
#include "measurement/measurement_engine.h"
#include "app/app_calibration_runtime.h"

#include <stdio.h>
#include <string.h>

#include "storage/storage_crc32.h"

#define TEST_CAPACITY_BYTES (2u * 1024u * 1024u)
#define TEST_MUTABLE_BASE (TEST_CAPACITY_BYTES - STORAGE_LAYOUT_MUTABLE_RESERVED_BYTES)
#define TEST_FLASH_BYTES STORAGE_LAYOUT_MUTABLE_RESERVED_BYTES
#define TEST_ADC_SCALE (3.3f / 4095.0f)

typedef struct
{
    uint8_t flash[TEST_FLASH_BYTES];
    uint8_t pending[256];
    uint32_t pending_address;
    size_t pending_size;
    uint8_t busy_polls;
    uint8_t erase_start_count;
    uint8_t program_start_count;
    enum
    {
        FAKE_NOR_IDLE = 0,
        FAKE_NOR_ERASE_BUSY,
        FAKE_NOR_PROGRAM_BUSY,
    } state;
    bool fail_program;
} fake_nor_t;

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static float abs_f(float value)
{
    return (value < 0.0f) ? -value : value;
}

static int expect_near(float actual, float expected, float tolerance, const char *message)
{
    if (abs_f(actual - expected) > tolerance)
    {
        (void)fprintf(stderr,
                      "FAIL: %s (got %.7g expected %.7g tol %.7g)\n",
                      message,
                      (double)actual,
                      (double)expected,
                      (double)tolerance);
        return 1;
    }
    return 0;
}

static void fake_nor_init(fake_nor_t *nor)
{
    (void)memset(nor->flash, 0xFF, sizeof(nor->flash));
    (void)memset(nor->pending, 0xFF, sizeof(nor->pending));
    nor->pending_address = 0u;
    nor->pending_size = 0u;
    nor->busy_polls = 0u;
    nor->erase_start_count = 0u;
    nor->program_start_count = 0u;
    nor->state = FAKE_NOR_IDLE;
    nor->fail_program = false;
}

static bool fake_map(uint32_t address, size_t size, size_t *offset)
{
    if ((address < TEST_MUTABLE_BASE) || (offset == NULL))
    {
        return false;
    }
    const uint32_t local = address - TEST_MUTABLE_BASE;
    if ((local >= TEST_FLASH_BYTES) || (size > ((size_t)TEST_FLASH_BYTES - (size_t)local)))
    {
        return false;
    }
    *offset = (size_t)local;
    return true;
}

static bsp_status_t fake_read(uint32_t address, void *dst, size_t size, void *user)
{
    fake_nor_t *nor = (fake_nor_t *)user;
    size_t offset = 0u;
    if ((nor == NULL) || (dst == NULL) || !fake_map(address, size, &offset))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    (void)memcpy(dst, &nor->flash[offset], size);
    return BSP_STATUS_OK;
}

static bsp_status_t fake_erase_start(uint32_t address, uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_nor_t *nor = (fake_nor_t *)user;
    size_t offset = 0u;
    if ((nor == NULL) || !fake_map(address, STORAGE_LAYOUT_W25Q_SECTOR_SIZE, &offset) ||
        ((address % STORAGE_LAYOUT_W25Q_SECTOR_SIZE) != 0u))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (nor->state != FAKE_NOR_IDLE)
    {
        return BSP_STATUS_BUSY;
    }
    nor->pending_address = address;
    nor->pending_size = STORAGE_LAYOUT_W25Q_SECTOR_SIZE;
    nor->busy_polls = 1u;
    nor->erase_start_count++;
    nor->state = FAKE_NOR_ERASE_BUSY;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_program_start(uint32_t address,
                                       const void *src,
                                       size_t size,
                                       uint32_t now_ms,
                                       void *user)
{
    (void)now_ms;
    fake_nor_t *nor = (fake_nor_t *)user;
    const uint8_t *bytes = (const uint8_t *)src;
    size_t offset = 0u;
    if ((nor == NULL) || (bytes == NULL) || nor->fail_program ||
        !fake_map(address, size, &offset))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if ((nor->state != FAKE_NOR_IDLE) || (size == 0u) ||
        (size > (256u - (address % 256u))) || (size > sizeof(nor->pending)))
    {
        return BSP_STATUS_BUSY;
    }
    for (size_t i = 0u; i < size; i++)
    {
        if ((uint8_t)(nor->flash[offset + i] & bytes[i]) != bytes[i])
        {
            return BSP_STATUS_ERROR;
        }
    }
    (void)memcpy(nor->pending, bytes, size);
    nor->pending_address = address;
    nor->pending_size = size;
    nor->busy_polls = 1u;
    nor->program_start_count++;
    nor->state = FAKE_NOR_PROGRAM_BUSY;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_poll(uint32_t now_ms, void *user)
{
    (void)now_ms;
    fake_nor_t *nor = (fake_nor_t *)user;
    if (nor == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (nor->state == FAKE_NOR_IDLE)
    {
        return BSP_STATUS_OK;
    }
    if (nor->busy_polls != 0u)
    {
        nor->busy_polls--;
        return BSP_STATUS_BUSY;
    }
    size_t offset = 0u;
    if (!fake_map(nor->pending_address, nor->pending_size, &offset))
    {
        nor->state = FAKE_NOR_IDLE;
        return BSP_STATUS_ERROR;
    }
    if (nor->state == FAKE_NOR_ERASE_BUSY)
    {
        (void)memset(&nor->flash[offset], 0xFF, nor->pending_size);
    }
    else if (nor->state == FAKE_NOR_PROGRAM_BUSY)
    {
        for (size_t i = 0u; i < nor->pending_size; i++)
        {
            nor->flash[offset + i] &= nor->pending[i];
        }
    }
    nor->state = FAKE_NOR_IDLE;
    return BSP_STATUS_OK;
}

static measurement_cal_store_io_t fake_io(fake_nor_t *nor)
{
    measurement_cal_store_io_t io = {
        .read = fake_read,
        .erase_sector_start = fake_erase_start,
        .program_start = fake_program_start,
        .poll = fake_poll,
        .user = nor,
    };
    return io;
}

static bool fake_write_slot_frame(fake_nor_t *nor,
                                  measurement_cal_store_slot_t slot,
                                  const measurement_cal_set_t *set)
{
    if ((nor == NULL) || (set == NULL))
    {
        return false;
    }
    uint8_t bytes[MEASUREMENT_CAL_MAX_FRAME_BYTES];
    size_t written = 0u;
    if (!measurement_cal_serialize_set(set, bytes, sizeof(bytes), &written))
    {
        return false;
    }
    storage_partition_t partition;
    if (!storage_layout_partition(TEST_CAPACITY_BYTES,
                                  (slot == MEASUREMENT_CAL_STORE_SLOT_A) ?
                                      STORAGE_PARTITION_CALIBRATION_A :
                                      STORAGE_PARTITION_CALIBRATION_B,
                                  &partition))
    {
        return false;
    }
    size_t offset = 0u;
    if (!fake_map(partition.start, partition.size, &offset) || (written > partition.size))
    {
        return false;
    }
    (void)memset(&nor->flash[offset], 0xFF, partition.size);
    (void)memcpy(&nor->flash[offset], bytes, written);
    return true;
}

static measurement_cal_key_t key_for(hw_range_id_t range,
                                     hw_excitation_freq_t frequency)
{
    return measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                               MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                               range,
                               frequency,
                               HW_EXCITATION_AMP_100MVRMS);
}

static measurement_cal_record_t record_for(hw_range_id_t range, uint32_t zref_ohms)
{
    const measurement_cal_key_t key = key_for(range, HW_EXCITATION_FREQ_1KHZ);
    measurement_cal_record_t record = measurement_cal_make_ideal_record(&key);
    record.correction.zref_ohms = measurement_complex((float)zref_ohms, 0.0f);
    record.correction.flags |= MEASUREMENT_CAL_FLAG_QUALIFIED;
    return record;
}

static measurement_cal_set_t set_with_records(uint32_t sequence, uint8_t count)
{
    measurement_cal_set_t set;
    measurement_cal_set_init(&set,
                             MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                             sequence);
    for (uint8_t i = 0u; i < count; i++)
    {
        const hw_range_id_t range = (hw_range_id_t)(HW_RANGE_ID_10R + i);
        measurement_cal_record_t record = record_for(range, 10u * (uint32_t)(i + 1u));
        (void)measurement_cal_set_add_record(&set, &record);
    }
    return set;
}

static measurement_cal_set_t full_supported_set(uint32_t sequence)
{
    measurement_cal_set_t set;
    measurement_cal_set_init(&set,
                             MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                             sequence);
    for (hw_range_id_t range = HW_RANGE_ID_10R; range <= HW_RANGE_ID_1M; range++)
    {
        for (hw_excitation_freq_t freq = HW_EXCITATION_FREQ_100HZ;
             freq <= HW_EXCITATION_FREQ_10KHZ;
             freq++)
        {
            for (hw_excitation_amp_t amp = HW_EXCITATION_AMP_100MVRMS;
                 amp <= HW_EXCITATION_AMP_500MVRMS;
                 amp++)
            {
                if (measurement_cal_condition_allowed(range, freq, amp))
                {
                    const measurement_cal_key_t key =
                        measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                                            MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                            range,
                                            freq,
                                            amp);
                    measurement_cal_record_t record = measurement_cal_make_ideal_record(&key);
                    record.correction.zref_ohms = measurement_dsp_config_ideal(range).zref_ohms;
                    record.correction.flags |= MEASUREMENT_CAL_FLAG_QUALIFIED;
                    (void)measurement_cal_set_add_record(&set, &record);
                }
            }
        }
    }
    return set;
}

static int test_crc_and_layout(void)
{
    int failures = 0;
    failures += expect_true(storage_crc32("123456789", 9u) == 0xCBF43926u, "crc32 check vector");

    storage_partition_t resource;
    storage_partition_t cal_a;
    storage_partition_t cal_b;
    storage_partition_t test;
    failures += expect_true(storage_layout_partition(TEST_CAPACITY_BYTES,
                                                     STORAGE_PARTITION_RESOURCE_PACK,
                                                     &resource),
                            "resource partition");
    failures += expect_true(storage_layout_partition(TEST_CAPACITY_BYTES,
                                                     STORAGE_PARTITION_CALIBRATION_A,
                                                     &cal_a),
                            "cal A partition");
    failures += expect_true(storage_layout_partition(TEST_CAPACITY_BYTES,
                                                     STORAGE_PARTITION_CALIBRATION_B,
                                                     &cal_b),
                            "cal B partition");
    failures += expect_true(storage_layout_partition(TEST_CAPACITY_BYTES,
                                                     STORAGE_PARTITION_BRINGUP_TEST,
                                                     &test),
                            "test partition");
    failures += expect_true(resource.start == 0u, "resource starts at zero");
    failures += expect_true(resource.size == TEST_MUTABLE_BASE, "resource leaves mutable tail");
    failures += expect_true(cal_b.start == (cal_a.start + STORAGE_LAYOUT_CAL_SLOT_BYTES), "slots adjacent");
    failures += expect_true(test.start == (TEST_CAPACITY_BYTES - STORAGE_LAYOUT_W25Q_SECTOR_SIZE),
                            "final sector remains bring-up test");
    return failures;
}

static int test_serialization_resolution_and_validity(void)
{
    int failures = 0;
    measurement_cal_set_t set = set_with_records(7u, 2u);
    uint8_t bytes[MEASUREMENT_CAL_MAX_FRAME_BYTES];
    size_t written = 0u;
    failures += expect_true(measurement_cal_serialize_set(&set, bytes, sizeof(bytes), &written),
                            "serialize set");
    failures += expect_true(written < sizeof(bytes), "serialized bounded");

    measurement_cal_set_t decoded;
    measurement_cal_frame_info_t info;
    failures += expect_true(measurement_cal_decode_set(bytes, written, &decoded, &info), "decode set");
    failures += expect_true(info.committed, "frame committed");
    failures += expect_true(decoded.sequence == 7u, "sequence roundtrip");
    failures += expect_true(decoded.record_count == 2u, "count roundtrip");

    measurement_cal_requirements_t req = measurement_cal_requirements_empty();
    const measurement_cal_key_t present = key_for(HW_RANGE_ID_10R, HW_EXCITATION_FREQ_1KHZ);
    const measurement_cal_key_t missing = key_for(HW_RANGE_ID_1M, HW_EXCITATION_FREQ_100HZ);
    (void)measurement_cal_requirements_add(&req, &present);
    measurement_cal_validity_t validity =
        measurement_cal_validate_set(&decoded, &req, MEASUREMENT_CAL_HARDWARE_REV1,
                                     MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
    failures += expect_true(validity.status == MEASUREMENT_CAL_VALIDITY_VALID, "valid required set");
    (void)measurement_cal_requirements_add(&req, &missing);
    validity = measurement_cal_validate_set(&decoded, &req, MEASUREMENT_CAL_HARDWARE_REV1,
                                            MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
    failures += expect_true(validity.status == MEASUREMENT_CAL_VALIDITY_INCOMPLETE, "incomplete set");
    validity = measurement_cal_validate_set(&decoded, NULL, 0x00020001u,
                                            MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
    failures += expect_true(validity.status == MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_HARDWARE,
                            "hardware mismatch");

    measurement_adc_calibration_t adc;
    measurement_dsp_config_t config;
    measurement_calibration_provenance_t provenance;
    failures += expect_true(measurement_cal_resolve(&decoded,
                                                    &present,
                                                    false,
                                                    &adc,
                                                    &config,
                                                    &provenance) == MEASUREMENT_CAL_RESOLVE_FOUND,
                            "resolve exact");
    failures += expect_near(config.zref_ohms.re, 10.0f, 0.01f, "resolved zref");
    failures += expect_true(measurement_cal_resolve(&decoded,
                                                    &missing,
                                                    true,
                                                    &adc,
                                                    &config,
                                                    &provenance) == MEASUREMENT_CAL_RESOLVE_MISSING,
                            "ideal fallback reported missing");
    failures += expect_true(provenance.source == MEASUREMENT_CAL_SOURCE_IDEAL &&
                                provenance.uncalibrated,
                            "fallback provenance");

    bytes[MEASUREMENT_CAL_FRAME_HEADER_BYTES + 6u] ^= 0x01u;
    failures += expect_true(!measurement_cal_decode_set(bytes, written, &decoded, NULL),
                            "crc catches payload corruption");
    return failures;
}

static int test_duplicate_and_replace_semantics(void)
{
    int failures = 0;
    measurement_cal_set_t set;
    measurement_cal_set_init(&set,
                             MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                             9u);
    measurement_cal_record_t rec_10r = record_for(HW_RANGE_ID_10R, 10u);
    measurement_cal_record_t rec_100r = record_for(HW_RANGE_ID_100R, 100u);
    failures += expect_true(measurement_cal_set_add_record(&set, &rec_10r), "unique add succeeds");
    failures += expect_true(!measurement_cal_set_add_record(&set, &rec_10r), "duplicate add fails");
    rec_10r.correction.zref_ohms = measurement_complex(11.0f, 0.0f);
    failures += expect_true(measurement_cal_set_replace_record(&set, &rec_10r), "replace existing succeeds");
    measurement_cal_resolved_t resolved;
    failures += expect_true(measurement_cal_resolve_condition(&set,
                                                              &rec_10r.key,
                                                              false,
                                                              &resolved) ==
                                MEASUREMENT_CAL_RESOLVE_FOUND,
                            "replacement resolves");
    failures += expect_near(resolved.config.zref_ohms.re, 11.0f, 0.001f, "replacement value visible");
    failures += expect_true(!measurement_cal_set_replace_record(&set, &rec_100r), "replace missing fails");
    failures += expect_true(measurement_cal_set_add_record(&set, &rec_100r), "different key add succeeds");

    measurement_cal_set_t corrupted = set;
    corrupted.records[1].key = corrupted.records[0].key;
    corrupted.records[1].condition_id = measurement_cal_condition_id(&corrupted.records[1].key);
    measurement_cal_validity_t validity =
        measurement_cal_validate_set(&corrupted,
                                     NULL,
                                     MEASUREMENT_CAL_HARDWARE_REV1,
                                     MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
    failures += expect_true(validity.status == MEASUREMENT_CAL_VALIDITY_CORRUPT,
                            "manual duplicate rejected");

    uint8_t bytes[MEASUREMENT_CAL_MAX_FRAME_BYTES];
    size_t written = 0u;
    failures += expect_true(!measurement_cal_serialize_set(&corrupted, bytes, sizeof(bytes), &written),
                            "duplicate serialization rejected");
    failures += expect_true(measurement_cal_key_equal(&set.records[0].key, &set.records[1].key) == false,
                            "complete key remains authoritative");
    failures += expect_true(set.records[0].condition_id != 0u && set.records[1].condition_id != 0u,
                            "condition id remains diagnostic metadata");
    return failures;
}

static int test_full_supported_matrix_capacity(void)
{
    int failures = 0;
    measurement_cal_set_t set = full_supported_set(11u);
    const measurement_cal_requirements_t req = measurement_cal_requirements_rev1_full();
    failures += expect_true(req.count == MEASUREMENT_CONDITION_REV1_MAX_SUPPORTED,
                            "Rev1 supported matrix count");
    failures += expect_true(req.count == set.record_count, "full matrix count matches requirements");
    failures += expect_true(set.record_count <= MEASUREMENT_CAL_MAX_RECORDS, "full matrix fits record limit");

    uint8_t bytes[MEASUREMENT_CAL_MAX_FRAME_BYTES];
    size_t written = 0u;
    failures += expect_true(measurement_cal_serialize_set(&set, bytes, sizeof(bytes), &written),
                            "full matrix serializes");
    failures += expect_true(written <= MEASUREMENT_CAL_MAX_FRAME_BYTES, "full matrix frame bounded");

    measurement_cal_set_t decoded;
    failures += expect_true(measurement_cal_decode_set(bytes, written, &decoded, NULL), "full matrix decodes");
    measurement_cal_validity_t validity =
        measurement_cal_validate_set(&decoded, &req, MEASUREMENT_CAL_HARDWARE_REV1,
                                     MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
    failures += expect_true(validity.status == MEASUREMENT_CAL_VALIDITY_VALID, "full matrix valid");

    for (uint8_t i = 0u; i < req.count; i++)
    {
        measurement_cal_resolved_t resolved;
        failures += expect_true(measurement_cal_resolve_condition(&decoded,
                                                                  &req.keys[i],
                                                                  false,
                                                                  &resolved) ==
                                    MEASUREMENT_CAL_RESOLVE_FOUND,
                                "full matrix condition resolves");
    }
    return failures;
}

static int test_condition_domain_consistency(void)
{
    int failures = 0;
    uint8_t supported_count = 0u;
    for (hw_range_id_t range = HW_RANGE_ID_10R; range <= HW_RANGE_ID_1M; range++)
    {
        for (hw_excitation_freq_t freq = HW_EXCITATION_FREQ_100HZ;
             freq <= HW_EXCITATION_FREQ_10KHZ;
             freq++)
        {
            for (hw_excitation_amp_t amp = HW_EXCITATION_AMP_100MVRMS;
                 amp <= HW_EXCITATION_AMP_500MVRMS;
                 amp++)
            {
                hw_excitation_freq_profile_t profile;
                const bool hardware =
                    (hw_excitation_freq_profile(freq, &profile) == BSP_STATUS_OK) &&
                    (hw_excitation_validate_amplitude(range, amp) == BSP_STATUS_OK);
                const bool condition = measurement_condition_supported(range, freq, amp);
                const bool automatic = measurement_auto_condition_allowed(range, freq, amp);
                const bool calibration = measurement_cal_condition_allowed(range, freq, amp);
                failures += expect_true(condition == hardware, "condition matches hardware capability");
                failures += expect_true(automatic == condition, "automatic matches condition domain");
                failures += expect_true(calibration == condition, "calibration matches condition domain");
                if (condition)
                {
                    supported_count++;
                }
            }
        }
    }
    failures += expect_true(supported_count == MEASUREMENT_CONDITION_REV1_MAX_SUPPORTED,
                            "condition domain count");
    failures += expect_true(!measurement_condition_supported(HW_RANGE_ID_10R,
                                                            HW_EXCITATION_FREQ_100HZ,
                                                            HW_EXCITATION_AMP_500MVRMS),
                            "10R 500m forbidden");
    failures += expect_true(measurement_condition_supported(HW_RANGE_ID_100K,
                                                           HW_EXCITATION_FREQ_10KHZ,
                                                           HW_EXCITATION_AMP_100MVRMS),
                            "100K 10k calibratable");
    failures += expect_true(measurement_condition_supported(HW_RANGE_ID_1M,
                                                           HW_EXCITATION_FREQ_1KHZ,
                                                           HW_EXCITATION_AMP_100MVRMS),
                            "1M 1k calibratable");
    failures += expect_true(measurement_condition_supported(HW_RANGE_ID_1M,
                                                           HW_EXCITATION_FREQ_10KHZ,
                                                           HW_EXCITATION_AMP_100MVRMS),
                            "1M 10k calibratable");
    return failures;
}

static int write_to_completion(measurement_cal_store_t *store, const measurement_cal_set_t *set)
{
    int failures = 0;
    failures += expect_true(measurement_cal_store_write_start(store, set, NULL) == BSP_STATUS_BUSY,
                            "write start busy");
    for (uint32_t i = 0u; i < 128u; i++)
    {
        const bsp_status_t status = measurement_cal_store_step(store, i);
        if (status == BSP_STATUS_OK)
        {
            return failures;
        }
        if (status != BSP_STATUS_BUSY)
        {
            (void)fprintf(stderr,
                          "FAIL: write status %d state %d step %lu\n",
                          (int)status,
                          (int)measurement_cal_store_state(store),
                          (unsigned long)i);
            failures++;
        }
    }
    failures += expect_true(false, "write completed within bounded steps");
    return failures;
}

static int expect_loaded_sequence(measurement_cal_store_t *store,
                                  uint32_t sequence,
                                  const char *message)
{
    measurement_cal_set_t loaded;
    const bsp_status_t status = measurement_cal_store_load_newest(store, &loaded, NULL);
    if (status != BSP_STATUS_OK)
    {
        (void)fprintf(stderr, "FAIL: %s (load status %d)\n", message, (int)status);
        return 1;
    }
    return expect_true(loaded.sequence == sequence, message);
}

static int test_store_power_loss(void)
{
    int failures = 0;
    for (uint8_t stop_after = 0u; stop_after < 9u; stop_after++)
    {
        fake_nor_t nor;
        fake_nor_init(&nor);
        measurement_cal_store_t store;
        measurement_cal_store_io_t io = fake_io(&nor);
        failures += expect_true(measurement_cal_store_init(&store, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                                "store init");
        measurement_cal_set_t old_set = set_with_records(0u, 1u);
        failures += write_to_completion(&store, &old_set);
        failures += expect_loaded_sequence(&store, 1u, "old sequence loaded");

        measurement_cal_store_t torn;
        failures += expect_true(measurement_cal_store_init(&torn, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                                "torn store init");
        measurement_cal_set_t candidate = set_with_records(0u, 5u);
        failures += expect_true(measurement_cal_store_write_start(&torn, &candidate, NULL) == BSP_STATUS_BUSY,
                                "candidate start");
        for (uint8_t step = 0u; step < stop_after; step++)
        {
            (void)measurement_cal_store_step(&torn, step);
        }
        measurement_cal_store_t rebooted;
        failures += expect_true(measurement_cal_store_init(&rebooted, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                                "rebooted init");
        const measurement_cal_store_state_t torn_state = measurement_cal_store_state(&torn);
        const uint32_t expected =
            ((torn_state == MEASUREMENT_CAL_STORE_VERIFY) ||
             (torn_state == MEASUREMENT_CAL_STORE_DONE)) ? 2u : 1u;
        failures += expect_loaded_sequence(&rebooted, expected, "power-loss recovery sequence");
    }

    failures += expect_true(measurement_cal_store_sequence_newer(1u, 0xFFFFFFFFu),
                            "sequence rollover newer");
    failures += expect_true(!measurement_cal_store_sequence_newer(0xFFFFFFFFu, 1u),
                            "old pre-rollover not newer");
    return failures;
}

static int test_store_write_header_override_does_not_mutate_candidate(void)
{
    int failures = 0;
    fake_nor_t nor;
    fake_nor_init(&nor);
    measurement_cal_store_t store;
    measurement_cal_store_io_t io = fake_io(&nor);
    failures += expect_true(measurement_cal_store_init(&store, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                            "store init for header override");

    measurement_cal_set_t candidate;
    measurement_cal_set_init(&candidate,
                             MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                             0u);
    const measurement_cal_key_t key = key_for(HW_RANGE_ID_1K, HW_EXCITATION_FREQ_1KHZ);
    const measurement_cal_record_t record = measurement_cal_make_ideal_record(&key);
    failures += expect_true(measurement_cal_set_add_record(&candidate, &record),
                            "header override candidate record");
    candidate.schema_version = 1u;
    candidate.model_version = MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1;
    failures += expect_true(write_to_completion(&store, &candidate) == 0, "header override write");
    failures += expect_true(candidate.sequence == 0u, "candidate sequence not mutated");
    failures += expect_true(candidate.schema_version == 1u, "candidate schema not mutated");
    failures += expect_true(candidate.model_version == MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1,
                            "candidate model not mutated");

    measurement_cal_set_t loaded;
    measurement_cal_store_slot_t slot;
    failures += expect_true(measurement_cal_store_load_newest(&store, &loaded, &slot) == BSP_STATUS_OK,
                            "header override load");
    failures += expect_true(loaded.sequence == 1u, "stored sequence assigned");
    failures += expect_true(loaded.schema_version == MEASUREMENT_CAL_SCHEMA_VERSION,
                            "stored schema current");
    failures += expect_true(loaded.model_version == MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                            "stored model current");
    return failures;
}

static int test_fake_nor_async_busy_and_page_rules(void)
{
    int failures = 0;
    fake_nor_t nor;
    fake_nor_init(&nor);
    uint8_t data[4] = {0xAAu, 0x55u, 0x00u, 0xFFu};
    const uint32_t page_tail = TEST_MUTABLE_BASE + 255u;

    failures += expect_true(fake_erase_start(TEST_MUTABLE_BASE, 0u, &nor) == BSP_STATUS_BUSY,
                            "erase enters busy");
    failures += expect_true(fake_program_start(TEST_MUTABLE_BASE, data, sizeof(data), 0u, &nor) == BSP_STATUS_BUSY,
                            "program while erase busy rejected");
    failures += expect_true(fake_poll(1u, &nor) == BSP_STATUS_BUSY, "erase first poll busy");
    failures += expect_true(fake_poll(2u, &nor) == BSP_STATUS_OK, "erase completes");
    failures += expect_true(fake_program_start(page_tail, data, sizeof(data), 3u, &nor) == BSP_STATUS_BUSY,
                            "page crossing program rejected");
    failures += expect_true(fake_program_start(TEST_MUTABLE_BASE, data, sizeof(data), 4u, &nor) == BSP_STATUS_BUSY,
                            "program enters busy");
    failures += expect_true(fake_erase_start(TEST_MUTABLE_BASE, 5u, &nor) == BSP_STATUS_BUSY,
                            "erase while program busy rejected");
    failures += expect_true(fake_poll(6u, &nor) == BSP_STATUS_BUSY, "program first poll busy");
    failures += expect_true(fake_poll(7u, &nor) == BSP_STATUS_OK, "program completes");
    return failures;
}

static int test_store_selects_newest_usable(void)
{
    int failures = 0;
    fake_nor_t nor;
    fake_nor_init(&nor);
    measurement_cal_store_t store;
    measurement_cal_store_io_t io = fake_io(&nor);
    const measurement_cal_requirements_t req = measurement_cal_requirements_rev1_full();
    failures += expect_true(measurement_cal_store_init(&store, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                            "usable store init");

    measurement_cal_set_t full = full_supported_set(0u);
    failures += write_to_completion(&store, &full);

    measurement_cal_set_t incomplete = set_with_records(0u, 1u);
    failures += expect_true(measurement_cal_store_write_start(&store, &incomplete, NULL) == BSP_STATUS_BUSY,
                            "incomplete structural write starts");
    for (uint32_t i = 0u; i < 96u; i++)
    {
        if (measurement_cal_store_step(&store, 100u + i) == BSP_STATUS_OK)
        {
            break;
        }
    }

    measurement_cal_set_t active;
    measurement_cal_store_slot_t slot = MEASUREMENT_CAL_STORE_SLOT_A;
    measurement_cal_store_slot_info_t info[2];
    failures += expect_true(measurement_cal_store_load_newest_usable(&store,
                                                                     &req,
                                                                     MEASUREMENT_CAL_HARDWARE_REV1,
                                                                     MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                                                     &active,
                                                                     &slot,
                                                                     info) == BSP_STATUS_OK,
                            "newest usable loads");
    failures += expect_true(active.record_count == req.count, "newer incomplete rejected for active");
    failures += expect_true((info[0].validity.status == MEASUREMENT_CAL_VALIDITY_VALID) ||
                                (info[1].validity.status == MEASUREMENT_CAL_VALIDITY_VALID),
                            "slot diagnostics include valid slot");
    failures += expect_true((info[0].validity.status == MEASUREMENT_CAL_VALIDITY_INCOMPLETE) ||
                                (info[1].validity.status == MEASUREMENT_CAL_VALIDITY_INCOMPLETE),
                            "slot diagnostics expose incomplete newer slot");
    return failures;
}

static int exercise_incompatible_newer_slot(measurement_cal_set_t newer,
                                            measurement_cal_validity_status_t expected_status,
                                            const char *message)
{
    int failures = 0;
    fake_nor_t nor;
    fake_nor_init(&nor);
    const measurement_cal_key_t required = key_for(HW_RANGE_ID_10R, HW_EXCITATION_FREQ_1KHZ);
    measurement_cal_requirements_t req = measurement_cal_requirements_empty();
    (void)measurement_cal_requirements_add(&req, &required);

    measurement_cal_set_t active = set_with_records(10u, 1u);
    newer.sequence = 11u;
    failures += expect_true(fake_write_slot_frame(&nor, MEASUREMENT_CAL_STORE_SLOT_A, &active),
                            "write compatible older slot");
    failures += expect_true(fake_write_slot_frame(&nor, MEASUREMENT_CAL_STORE_SLOT_B, &newer),
                            "write incompatible newer slot");

    measurement_cal_store_t store;
    measurement_cal_store_io_t io = fake_io(&nor);
    failures += expect_true(measurement_cal_store_init(&store, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                            "compat store init");
    measurement_cal_set_t loaded;
    measurement_cal_store_slot_t slot = MEASUREMENT_CAL_STORE_SLOT_B;
    measurement_cal_store_slot_info_t info[2];
    failures += expect_true(measurement_cal_store_load_newest_usable(&store,
                                                                     &req,
                                                                     MEASUREMENT_CAL_HARDWARE_REV1,
                                                                     MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                                                     &loaded,
                                                                     &slot,
                                                                     info) == BSP_STATUS_OK,
                            message);
    failures += expect_true(loaded.sequence == 10u, "older compatible active");
    failures += expect_true(slot == MEASUREMENT_CAL_STORE_SLOT_A, "older slot selected");
    failures += expect_true(info[1].frame_valid, "incompatible frame structurally valid");
    failures += expect_true(info[1].frame.sequence == 11u, "incompatible sequence preserved");
    failures += expect_true(info[1].validity.status == expected_status, message);
    return failures;
}

static int test_app_calibration_runtime_loads_active_set(void)
{
    int failures = 0;
    fake_nor_t nor;
    fake_nor_init(&nor);

    measurement_cal_set_t set;
    measurement_cal_set_init(&set,
                             MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                             9u);
    for (hw_range_id_t range = HW_RANGE_ID_10R; range <= HW_RANGE_ID_1M; range++)
    {
        for (hw_excitation_freq_t freq = HW_EXCITATION_FREQ_100HZ;
             freq <= HW_EXCITATION_FREQ_10KHZ;
             freq++)
        {
            for (hw_excitation_amp_t amp = HW_EXCITATION_AMP_100MVRMS;
                 amp <= HW_EXCITATION_AMP_500MVRMS;
                 amp++)
            {
                if (measurement_cal_condition_allowed(range, freq, amp))
                {
                    const measurement_cal_key_t key =
                        measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                                            MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                            range,
                                            freq,
                                            amp);
                    measurement_cal_record_t record = measurement_cal_make_ideal_record(&key);
                    record.correction.flags |= MEASUREMENT_CAL_FLAG_QUALIFIED;
                    failures += expect_true(measurement_cal_set_add_record(&set, &record),
                                            "runtime fixture record add");
                }
            }
        }
    }
    failures += expect_true(fake_write_slot_frame(&nor, MEASUREMENT_CAL_STORE_SLOT_B, &set),
                            "runtime fixture writes slot");

    measurement_cal_store_t store_scratch;
    app_calibration_runtime_t runtime;
    app_calibration_runtime_init(&runtime);
    measurement_cal_store_io_t io = fake_io(&nor);
    failures += expect_true(app_calibration_runtime_refresh(&runtime,
                                                            &store_scratch,
                                                            &io,
                                                            TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                            "runtime refresh loads active calibration");
    failures += expect_true(app_calibration_runtime_store_ready(&runtime), "runtime store ready");
    failures += expect_true(app_calibration_runtime_active_valid(&runtime), "runtime active valid");
    failures += expect_true(app_calibration_runtime_active_slot(&runtime) == MEASUREMENT_CAL_STORE_SLOT_B,
                            "runtime records active slot");
    const measurement_cal_set_t *active = app_calibration_runtime_active_set(&runtime);
    failures += expect_true((active != NULL) && (active->sequence == 9u), "runtime active sequence");
    failures += expect_true((active != NULL) &&
                                (active->record_count == MEASUREMENT_CONDITION_REV1_MAX_SUPPORTED),
                            "runtime active complete record count");
    return failures;
}

static int test_slot_compatibility_diagnostics(void)
{
    int failures = 0;
    measurement_cal_set_t newer = set_with_records(11u, 1u);
    newer.schema_version = (uint16_t)(MEASUREMENT_CAL_SCHEMA_VERSION - 1u);
    failures += exercise_incompatible_newer_slot(newer,
                                                 MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_SCHEMA,
                                                 "old schema diagnosed");

    newer = set_with_records(11u, 1u);
    newer.hardware_revision = 0x00020001u;
    failures += exercise_incompatible_newer_slot(newer,
                                                 MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_HARDWARE,
                                                 "wrong hardware diagnosed");

    newer = set_with_records(11u, 1u);
    newer.model_version = MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1;
    failures += exercise_incompatible_newer_slot(newer,
                                                 MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_MODEL,
                                                 "wrong model diagnosed");

    fake_nor_t nor;
    fake_nor_init(&nor);
    measurement_cal_set_t corrupt = set_with_records(12u, 1u);
    failures += expect_true(fake_write_slot_frame(&nor, MEASUREMENT_CAL_STORE_SLOT_A, &corrupt),
                            "write corrupt source");
    storage_partition_t cal_a;
    failures += expect_true(storage_layout_partition(TEST_CAPACITY_BYTES,
                                                     STORAGE_PARTITION_CALIBRATION_A,
                                                     &cal_a),
                            "cal A for corruption");
    size_t offset = 0u;
    failures += expect_true(fake_map(cal_a.start, cal_a.size, &offset), "map cal A for corruption");
    nor.flash[offset + MEASUREMENT_CAL_FRAME_HEADER_BYTES + 8u] ^= 0x01u;

    measurement_cal_store_t store;
    measurement_cal_store_io_t io = fake_io(&nor);
    failures += expect_true(measurement_cal_store_init(&store, &io, TEST_CAPACITY_BYTES) == BSP_STATUS_OK,
                            "corrupt store init");
    measurement_cal_set_t loaded;
    measurement_cal_store_slot_info_t info[2];
    failures += expect_true(measurement_cal_store_load_newest_usable(&store,
                                                                     NULL,
                                                                     MEASUREMENT_CAL_HARDWARE_REV1,
                                                                     MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                                                     &loaded,
                                                                     NULL,
                                                                     info) == BSP_STATUS_ERROR,
                            "crc corrupt rejected");
    failures += expect_true(info[0].validity.status == MEASUREMENT_CAL_VALIDITY_CORRUPT,
                            "crc corrupt diagnosed");
    failures += expect_true(!info[0].frame_valid, "crc corrupt frame invalid");
    return failures;
}

static uint16_t volts_to_raw(float volts)
{
    int raw = (int)((volts / TEST_ADC_SCALE) + 0.5f);
    if (raw < 0)
    {
        raw = 0;
    }
    if (raw > 4095)
    {
        raw = 4095;
    }
    return (uint16_t)raw;
}

static float waveform(float vmid, measurement_complex_t phasor, float cos_ref, float sin_ref)
{
    return vmid + (phasor.re * cos_ref) - (phasor.im * sin_ref);
}

static void step_ref(float *cos_ref, float *sin_ref)
{
    const float next_cos = (*cos_ref * 0.9951847267f) - (*sin_ref * 0.0980171403f);
    const float next_sin = (*sin_ref * 0.9951847267f) + (*cos_ref * 0.0980171403f);
    *cos_ref = next_cos;
    *sin_ref = next_sin;
}

static int test_dsp_uses_calibrated_hg_transfer(void)
{
    int failures = 0;
    measurement_cal_set_t set;
    measurement_cal_set_init(&set,
                             MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                             3u);
    const measurement_cal_key_t key = key_for(HW_RANGE_ID_1K, HW_EXCITATION_FREQ_1KHZ);
    measurement_cal_record_t record = measurement_cal_make_ideal_record(&key);
    record.correction.ret_hg_transfer = measurement_complex(10.0f, 1.0f);
    record.correction.flags |= MEASUREMENT_CAL_FLAG_QUALIFIED;
    (void)measurement_cal_set_add_record(&set, &record);

    measurement_adc_calibration_t adc;
    measurement_dsp_config_t config;
    measurement_calibration_provenance_t provenance;
    failures += expect_true(measurement_cal_resolve(&set,
                                                    &key,
                                                    false,
                                                    &adc,
                                                    &config,
                                                    &provenance) == MEASUREMENT_CAL_RESOLVE_FOUND,
                            "calibrated HG resolve");

    hw_metrology_block_t block = {0};
    uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    (void)memset(raw, 0, sizeof(raw));
    block.valid = true;
    block.excitation_frequency_hz = 1000u;
    block.range_id = HW_RANGE_ID_1K;
    block.sample_count = HW_METROLOGY_SAMPLES_PER_BLOCK;
    block.samples_per_cycle = 64u;
    block.cycles_per_block = 4u;
    block.words_per_sample = HW_METROLOGY_WORDS_PER_SAMPLE;
    block.raw_words = raw;
    block.dma_complete = true;

    const measurement_complex_t source = measurement_complex(0.05f, 0.0f);
    const measurement_complex_t ret = measurement_complex(0.005f, -0.002f);
    const measurement_complex_t ret_hg = measurement_complex_mul(ret, record.correction.ret_hg_transfer);
    float cos_ref = 1.0f;
    float sin_ref = 0.0f;
    for (uint32_t n = 0u; n < HW_METROLOGY_SAMPLES_PER_BLOCK; n++)
    {
        raw[(3u * n) + 0u] =
            hw_metrology_pack_word(volts_to_raw(waveform(1.65f, source, cos_ref, sin_ref)),
                                   volts_to_raw(waveform(1.65f, ret, cos_ref, sin_ref)));
        raw[(3u * n) + 1u] =
            hw_metrology_pack_word(volts_to_raw(waveform(1.65f, source, cos_ref, sin_ref)),
                                   volts_to_raw(waveform(1.65f, ret_hg, cos_ref, sin_ref)));
        raw[(3u * n) + 2u] = hw_metrology_pack_word(volts_to_raw(1.65f), volts_to_raw(1.65f));
        step_ref(&cos_ref, &sin_ref);
    }
    hw_metrology_analyze_block(raw, HW_METROLOGY_SAMPLES_PER_BLOCK, &block);

    measurement_phasor_set_t phasors = {0};
    failures += expect_true(measurement_extract_phasors(&block, &adc, &config, &phasors) == BSP_STATUS_OK,
                            "extract with calibrated transfer");
    const measurement_complex_t reconstructed =
        measurement_complex_sub(phasors.ret_hg_reconstructed, phasors.vmid);
    failures += expect_near(reconstructed.re, ret.re, 0.0015f, "HG reconstructed real");
    failures += expect_near(reconstructed.im, ret.im, 0.0015f, "HG reconstructed imag");
    return failures;
}

static int test_output_correction_updates_result_and_derivatives(void)
{
    int failures = 0;
    measurement_cal_set_t set;
    measurement_cal_set_init(&set,
                             MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                             4u);
    const measurement_cal_key_t key = key_for(HW_RANGE_ID_1K, HW_EXCITATION_FREQ_1KHZ);
    measurement_cal_record_t record = measurement_cal_make_ideal_record(&key);
    record.correction.ret_1x_output.scale = measurement_complex(2.0f, 0.0f);
    record.correction.ret_1x_output.offset_ohms = measurement_complex(3.0f, 4.0f);
    record.correction.ret_hg_output.scale = measurement_complex(2.0f, 0.0f);
    record.correction.ret_hg_output.offset_ohms = measurement_complex(3.0f, 4.0f);
    record.correction.flags |= MEASUREMENT_CAL_FLAG_QUALIFIED;
    (void)measurement_cal_set_add_record(&set, &record);

    hw_metrology_block_t block = {0};
    uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
    (void)memset(raw, 0, sizeof(raw));
    block.valid = true;
    block.excitation_frequency_hz = 1000u;
    block.requested_amplitude_mvrms = 100u;
    block.range_id = HW_RANGE_ID_1K;
    block.sample_count = HW_METROLOGY_SAMPLES_PER_BLOCK;
    block.samples_per_cycle = 64u;
    block.cycles_per_block = 4u;
    block.words_per_sample = HW_METROLOGY_WORDS_PER_SAMPLE;
    block.raw_words = raw;
    block.dma_complete = true;

    const measurement_complex_t source = measurement_complex(0.05f, 0.0f);
    const measurement_complex_t ret = measurement_complex(0.005f, 0.0f);
    const measurement_complex_t ret_hg = measurement_complex_mul(ret, record.correction.ret_hg_transfer);
    float cos_ref = 1.0f;
    float sin_ref = 0.0f;
    for (uint32_t n = 0u; n < HW_METROLOGY_SAMPLES_PER_BLOCK; n++)
    {
        raw[(3u * n) + 0u] =
            hw_metrology_pack_word(volts_to_raw(waveform(1.65f, source, cos_ref, sin_ref)),
                                   volts_to_raw(waveform(1.65f, ret, cos_ref, sin_ref)));
        raw[(3u * n) + 1u] =
            hw_metrology_pack_word(volts_to_raw(waveform(1.65f, source, cos_ref, sin_ref)),
                                   volts_to_raw(waveform(1.65f, ret_hg, cos_ref, sin_ref)));
        raw[(3u * n) + 2u] = hw_metrology_pack_word(volts_to_raw(1.65f), volts_to_raw(1.65f));
        step_ref(&cos_ref, &sin_ref);
    }
    hw_metrology_analyze_block(raw, HW_METROLOGY_SAMPLES_PER_BLOCK, &block);

    measurement_calibrated_result_t processed;
    failures += expect_true(measurement_cal_process_block(&block, &set, &key, false, &processed) == BSP_STATUS_OK,
                            "calibrated block processes");
    failures += expect_true(processed.output_corrected, "output correction applied");
    failures += expect_near(processed.result.impedance.z_ohms.re,
                            (processed.raw_z_ohms.re * 2.0f) + 3.0f,
                            0.02f,
                            "corrected Z real");
    failures += expect_near(processed.result.impedance.z_ohms.im,
                            (processed.raw_z_ohms.im * 2.0f) + 4.0f,
                            0.02f,
                            "corrected Z imag");
    failures += expect_near(processed.result.derived.resistance_ohms,
                            processed.result.impedance.z_ohms.re,
                            0.001f,
                            "derived resistance follows corrected Z");
    failures += expect_true(processed.provenance.source == MEASUREMENT_CAL_SOURCE_PERSISTED,
                            "persisted provenance used");
    return failures;
}

int main(int argc, char **argv)
{
    if ((argc == 2) && (strcmp(argv[1], "--sizes") == 0))
    {
        (void)printf("measurement_cal_record_t=%lu\n",
                     (unsigned long)measurement_cal_record_size_bytes());
        (void)printf("measurement_cal_key_t=%lu\n",
                     (unsigned long)measurement_cal_key_size_bytes());
        (void)printf("measurement_cal_set_t=%lu\n",
                     (unsigned long)measurement_cal_set_size_bytes());
        (void)printf("measurement_cal_requirements_t=%lu\n",
                     (unsigned long)measurement_cal_requirements_size_bytes());
        (void)printf("measurement_cal_store_t=%lu\n",
                     (unsigned long)measurement_cal_store_context_size_bytes());
        (void)printf("app_calibration_runtime_t=%lu\n",
                     (unsigned long)app_calibration_runtime_context_size_bytes());
        return 0;
    }

    int failures = 0;
    failures += test_crc_and_layout();
    failures += test_serialization_resolution_and_validity();
    failures += test_duplicate_and_replace_semantics();
    failures += test_full_supported_matrix_capacity();
    failures += test_condition_domain_consistency();
    failures += test_fake_nor_async_busy_and_page_rules();
    failures += test_store_power_loss();
    failures += test_store_write_header_override_does_not_mutate_candidate();
    failures += test_store_selects_newest_usable();
    failures += test_app_calibration_runtime_loads_active_set();
    failures += test_slot_compatibility_diagnostics();
    failures += test_dsp_uses_calibrated_hg_transfer();
    failures += test_output_correction_updates_result_and_derivatives();
    return (failures == 0) ? 0 : 1;
}
