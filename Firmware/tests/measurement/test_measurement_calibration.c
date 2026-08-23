#include "measurement/measurement_calibration.h"
#include "measurement/measurement_calibration_store.h"

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

static bsp_status_t fake_erase(uint32_t address, void *user)
{
    fake_nor_t *nor = (fake_nor_t *)user;
    size_t offset = 0u;
    if ((nor == NULL) || !fake_map(address, STORAGE_LAYOUT_W25Q_SECTOR_SIZE, &offset) ||
        ((address % STORAGE_LAYOUT_W25Q_SECTOR_SIZE) != 0u))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    (void)memset(&nor->flash[offset], 0xFF, STORAGE_LAYOUT_W25Q_SECTOR_SIZE);
    return BSP_STATUS_OK;
}

static bsp_status_t fake_program(uint32_t address, const void *src, size_t size, void *user)
{
    fake_nor_t *nor = (fake_nor_t *)user;
    const uint8_t *bytes = (const uint8_t *)src;
    size_t offset = 0u;
    if ((nor == NULL) || (bytes == NULL) || nor->fail_program ||
        !fake_map(address, size, &offset))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    for (size_t i = 0u; i < size; i++)
    {
        if ((uint8_t)(nor->flash[offset + i] & bytes[i]) != bytes[i])
        {
            return BSP_STATUS_ERROR;
        }
    }
    for (size_t i = 0u; i < size; i++)
    {
        nor->flash[offset + i] &= bytes[i];
    }
    return BSP_STATUS_OK;
}

static bsp_status_t fake_poll(void *user)
{
    (void)user;
    return BSP_STATUS_OK;
}

static measurement_cal_store_io_t fake_io(fake_nor_t *nor)
{
    measurement_cal_store_io_t io = {
        .read = fake_read,
        .erase_sector = fake_erase,
        .program = fake_program,
        .poll = fake_poll,
        .user = nor,
    };
    return io;
}

static measurement_cal_key_t key_for(hw_range_id_t range,
                                     hw_excitation_freq_t frequency,
                                     measurement_return_channel_t channel)
{
    return measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                               MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1,
                               range,
                               frequency,
                               HW_EXCITATION_AMP_100MVRMS,
                               channel,
                               0u);
}

static measurement_cal_record_t record_for(hw_range_id_t range, uint32_t zref_ohms)
{
    const measurement_cal_key_t key = key_for(range, HW_EXCITATION_FREQ_1KHZ, MEASUREMENT_RETURN_1X);
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
                             MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1,
                             sequence);
    for (uint8_t i = 0u; i < count; i++)
    {
        const hw_range_id_t range = (hw_range_id_t)(HW_RANGE_ID_10R + i);
        measurement_cal_record_t record = record_for(range, 10u * (uint32_t)(i + 1u));
        (void)measurement_cal_set_add_record(&set, &record);
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
    const measurement_cal_key_t present = key_for(HW_RANGE_ID_10R,
                                                  HW_EXCITATION_FREQ_1KHZ,
                                                  MEASUREMENT_RETURN_1X);
    const measurement_cal_key_t missing = key_for(HW_RANGE_ID_1M,
                                                  HW_EXCITATION_FREQ_1KHZ,
                                                  MEASUREMENT_RETURN_1X);
    (void)measurement_cal_requirements_add(&req, &present);
    measurement_cal_validity_t validity =
        measurement_cal_validate_set(&decoded, &req, MEASUREMENT_CAL_HARDWARE_REV1,
                                     MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1);
    failures += expect_true(validity.status == MEASUREMENT_CAL_VALIDITY_VALID, "valid required set");
    (void)measurement_cal_requirements_add(&req, &missing);
    validity = measurement_cal_validate_set(&decoded, &req, MEASUREMENT_CAL_HARDWARE_REV1,
                                            MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1);
    failures += expect_true(validity.status == MEASUREMENT_CAL_VALIDITY_INCOMPLETE, "incomplete set");
    validity = measurement_cal_validate_set(&decoded, NULL, 0x00020001u,
                                            MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1);
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

static int write_to_completion(measurement_cal_store_t *store, const measurement_cal_set_t *set)
{
    int failures = 0;
    failures += expect_true(measurement_cal_store_write_start(store, set) == BSP_STATUS_BUSY,
                            "write start busy");
    for (uint32_t i = 0u; i < 32u; i++)
    {
        const bsp_status_t status = measurement_cal_store_step(store);
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
        failures += expect_true(measurement_cal_store_write_start(&torn, &candidate) == BSP_STATUS_BUSY,
                                "candidate start");
        for (uint8_t step = 0u; step < stop_after; step++)
        {
            (void)measurement_cal_store_step(&torn);
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
                             MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1,
                             3u);
    const measurement_cal_key_t key = key_for(HW_RANGE_ID_1K,
                                              HW_EXCITATION_FREQ_1KHZ,
                                              MEASUREMENT_RETURN_HG);
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

int main(void)
{
    int failures = 0;
    failures += test_crc_and_layout();
    failures += test_serialization_resolution_and_validity();
    failures += test_store_power_loss();
    failures += test_dsp_uses_calibrated_hg_transfer();
    return (failures == 0) ? 0 : 1;
}
