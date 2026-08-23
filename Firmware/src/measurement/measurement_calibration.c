#include "measurement/measurement_calibration.h"

#include <math.h>
#include <string.h>

#include "storage/storage_crc32.h"

enum
{
    FRAME_OFF_MAGIC = 0u,
    FRAME_OFF_RECORD_TYPE = 4u,
    FRAME_OFF_SCHEMA = 6u,
    FRAME_OFF_HEADER_SIZE = 8u,
    FRAME_OFF_PAYLOAD_LENGTH = 10u,
    FRAME_OFF_SEQUENCE = 12u,
    FRAME_OFF_HARDWARE = 16u,
    FRAME_OFF_MODEL = 20u,
    FRAME_OFF_FLAGS = 22u,
    FRAME_OFF_RESERVED = 24u,
    FRAME_OFF_CRC32 = 56u,
    FRAME_OFF_COMMIT = 60u,
    SET_PAYLOAD_HEADER_BYTES = 4u,
    CAL_RECORD_BYTES = 112u,
};

static bool finite_f(float value)
{
    return isfinite(value) != 0;
}

static void write_u16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8u) & 0xFFu);
}

static void write_u32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8u) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16u) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24u) & 0xFFu);
}

static uint16_t read_u16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8u));
}

static uint32_t read_u32(const uint8_t *src)
{
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8u) |
           ((uint32_t)src[2] << 16u) |
           ((uint32_t)src[3] << 24u);
}

static void write_i32(uint8_t *dst, int32_t value)
{
    write_u32(dst, (uint32_t)value);
}

static int32_t read_i32(const uint8_t *src)
{
    return (int32_t)read_u32(src);
}

static void write_f32(uint8_t *dst, float value)
{
    uint32_t bits = 0u;
    (void)memcpy(&bits, &value, sizeof(bits));
    write_u32(dst, bits);
}

static float read_f32(const uint8_t *src)
{
    const uint32_t bits = read_u32(src);
    float value = 0.0f;
    (void)memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t crc_frame(const uint8_t *frame, size_t payload_length)
{
    uint32_t crc = storage_crc32_update((uint32_t)STORAGE_CRC32_INIT, frame, FRAME_OFF_CRC32);
    crc = storage_crc32_update(crc,
                               &frame[FRAME_OFF_CRC32 + sizeof(uint32_t)],
                               FRAME_OFF_COMMIT - (FRAME_OFF_CRC32 + (uint32_t)sizeof(uint32_t)));
    crc = storage_crc32_update(crc,
                               &frame[MEASUREMENT_CAL_FRAME_HEADER_BYTES],
                               payload_length);
    return crc ^ (uint32_t)STORAGE_CRC32_XOR_OUT;
}

static bool valid_range(hw_range_id_t range_id)
{
    return range_id <= HW_RANGE_ID_1M;
}

static bool valid_frequency(hw_excitation_freq_t frequency)
{
    return (frequency == HW_EXCITATION_FREQ_100HZ) ||
           (frequency == HW_EXCITATION_FREQ_1KHZ) ||
           (frequency == HW_EXCITATION_FREQ_10KHZ);
}

static bool valid_amplitude(hw_excitation_amp_t amplitude)
{
    return (amplitude == HW_EXCITATION_AMP_100MVRMS) ||
           (amplitude == HW_EXCITATION_AMP_500MVRMS);
}

static bool valid_ret(measurement_return_channel_t ret_channel)
{
    return (ret_channel == MEASUREMENT_RETURN_1X) ||
           (ret_channel == MEASUREMENT_RETURN_HG);
}

measurement_cal_key_t measurement_cal_key(uint32_t hardware_revision,
                                          uint16_t model_version,
                                          hw_range_id_t range_id,
                                          hw_excitation_freq_t frequency,
                                          hw_excitation_amp_t amplitude,
                                          measurement_return_channel_t ret_channel,
                                          uint8_t ret_strategy)
{
    measurement_cal_key_t key = {
        .hardware_revision = hardware_revision,
        .model_version = model_version,
        .range_id = range_id,
        .frequency = frequency,
        .amplitude = amplitude,
        .ret_channel = ret_channel,
        .ret_strategy = ret_strategy,
    };
    return key;
}

bool measurement_cal_key_equal(const measurement_cal_key_t *a, const measurement_cal_key_t *b)
{
    return (a != NULL) && (b != NULL) &&
           (a->hardware_revision == b->hardware_revision) &&
           (a->model_version == b->model_version) &&
           (a->range_id == b->range_id) &&
           (a->frequency == b->frequency) &&
           (a->amplitude == b->amplitude) &&
           (a->ret_channel == b->ret_channel) &&
           (a->ret_strategy == b->ret_strategy);
}

uint32_t measurement_cal_condition_id(const measurement_cal_key_t *key)
{
    if (key == NULL)
    {
        return 0u;
    }
    uint8_t bytes[16] = {0};
    write_u32(&bytes[0], key->hardware_revision);
    write_u16(&bytes[4], key->model_version);
    bytes[6] = (uint8_t)key->range_id;
    bytes[7] = (uint8_t)key->frequency;
    bytes[8] = (uint8_t)key->amplitude;
    bytes[9] = (uint8_t)key->ret_channel;
    bytes[10] = key->ret_strategy;
    return storage_crc32(bytes, sizeof(bytes));
}

void measurement_cal_set_init(measurement_cal_set_t *set,
                              uint32_t hardware_revision,
                              uint16_t model_version,
                              uint32_t sequence)
{
    if (set == NULL)
    {
        return;
    }
    *set = (measurement_cal_set_t){0};
    set->sequence = sequence;
    set->hardware_revision = hardware_revision;
    set->schema_version = MEASUREMENT_CAL_SCHEMA_VERSION;
    set->model_version = model_version;
}

measurement_cal_record_t measurement_cal_make_ideal_record(const measurement_cal_key_t *key)
{
    measurement_cal_record_t record = {0};
    if (key != NULL)
    {
        record.key = *key;
    }
    measurement_dsp_config_t config = measurement_dsp_config_ideal(record.key.range_id);
    record.correction.adc = measurement_adc_calibration_ideal();
    record.correction.ret_hg_transfer = config.ret_hg_transfer;
    record.correction.zref_ohms = config.zref_ohms;
    record.correction.output_scale = measurement_complex(1.0f, 0.0f);
    record.correction.output_offset_ohms = measurement_complex(0.0f, 0.0f);
    record.correction.flags = MEASUREMENT_CAL_FLAG_ADC |
                              MEASUREMENT_CAL_FLAG_RET_HG |
                              MEASUREMENT_CAL_FLAG_ZREF |
                              MEASUREMENT_CAL_FLAG_OUTPUT_CORRECTION;
    record.record_type = MEASUREMENT_CAL_RECORD_CONDITION;
    record.temperature_mC = 25000;
    record.condition_id = measurement_cal_condition_id(&record.key);
    return record;
}

bool measurement_cal_set_add_record(measurement_cal_set_t *set, const measurement_cal_record_t *record)
{
    if ((set == NULL) || (record == NULL) || (set->record_count >= MEASUREMENT_CAL_MAX_RECORDS))
    {
        return false;
    }
    set->records[set->record_count] = *record;
    set->record_count++;
    return true;
}

measurement_cal_requirements_t measurement_cal_requirements_empty(void)
{
    measurement_cal_requirements_t requirements = {0};
    return requirements;
}

bool measurement_cal_requirements_add(measurement_cal_requirements_t *requirements,
                                      const measurement_cal_key_t *key)
{
    if ((requirements == NULL) || (key == NULL) ||
        (requirements->count >= MEASUREMENT_CAL_MAX_REQUIRED_KEYS))
    {
        return false;
    }
    requirements->keys[requirements->count] = *key;
    requirements->count++;
    return true;
}

static bool correction_finite(const measurement_cal_correction_t *correction)
{
    return (correction != NULL) &&
           finite_f(correction->adc.vexc_1.code_to_volts) &&
           finite_f(correction->adc.vexc_1.offset_volts) &&
           finite_f(correction->adc.ret_1x.code_to_volts) &&
           finite_f(correction->adc.ret_1x.offset_volts) &&
           finite_f(correction->adc.vexc_2.code_to_volts) &&
           finite_f(correction->adc.vexc_2.offset_volts) &&
           finite_f(correction->adc.ret_hg.code_to_volts) &&
           finite_f(correction->adc.ret_hg.offset_volts) &&
           finite_f(correction->adc.vmid_adc1.code_to_volts) &&
           finite_f(correction->adc.vmid_adc1.offset_volts) &&
           finite_f(correction->adc.vmid_adc2.code_to_volts) &&
           finite_f(correction->adc.vmid_adc2.offset_volts) &&
           measurement_complex_is_finite(correction->ret_hg_transfer) &&
           measurement_complex_is_finite(correction->zref_ohms) &&
           measurement_complex_is_finite(correction->output_scale) &&
           measurement_complex_is_finite(correction->output_offset_ohms);
}

static bool key_valid(const measurement_cal_key_t *key)
{
    return (key != NULL) &&
           (key->hardware_revision == MEASUREMENT_CAL_HARDWARE_REV1) &&
           (key->model_version == MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1) &&
           valid_range(key->range_id) &&
           valid_frequency(key->frequency) &&
           valid_amplitude(key->amplitude) &&
           valid_ret(key->ret_channel);
}

static const measurement_cal_record_t *find_record(const measurement_cal_set_t *set,
                                                   const measurement_cal_key_t *key)
{
    if ((set == NULL) || (key == NULL))
    {
        return NULL;
    }
    for (uint8_t i = 0u; i < set->record_count; i++)
    {
        if (measurement_cal_key_equal(&set->records[i].key, key))
        {
            return &set->records[i];
        }
    }
    return NULL;
}

measurement_cal_validity_t measurement_cal_validate_set(
    const measurement_cal_set_t *set,
    const measurement_cal_requirements_t *requirements,
    uint32_t hardware_revision,
    uint16_t model_version)
{
    measurement_cal_validity_t validity = {
        .status = MEASUREMENT_CAL_VALIDITY_VALID,
        .flags = MEASUREMENT_CAL_VALID_FLAG_NONE,
        .missing_required_count = 0u,
        .unqualified_count = 0u,
    };
    if (set == NULL)
    {
        validity.status = MEASUREMENT_CAL_VALIDITY_MISSING;
        validity.flags = MEASUREMENT_CAL_VALID_FLAG_MISSING;
        return validity;
    }
    if (set->schema_version != MEASUREMENT_CAL_SCHEMA_VERSION)
    {
        validity.status = MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_SCHEMA;
        validity.flags |= MEASUREMENT_CAL_VALID_FLAG_SCHEMA;
        return validity;
    }
    if (set->hardware_revision != hardware_revision)
    {
        validity.status = MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_HARDWARE;
        validity.flags |= MEASUREMENT_CAL_VALID_FLAG_HARDWARE;
        return validity;
    }
    if (set->model_version != model_version)
    {
        validity.status = MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_MODEL;
        validity.flags |= MEASUREMENT_CAL_VALID_FLAG_MODEL;
        return validity;
    }
    for (uint8_t i = 0u; i < set->record_count; i++)
    {
        const measurement_cal_record_t *record = &set->records[i];
        if (!key_valid(&record->key) || !correction_finite(&record->correction) ||
            (record->condition_id != measurement_cal_condition_id(&record->key)))
        {
            validity.status = MEASUREMENT_CAL_VALIDITY_CORRUPT;
            validity.flags |= MEASUREMENT_CAL_VALID_FLAG_CORRUPT;
            return validity;
        }
        if ((record->correction.flags & MEASUREMENT_CAL_FLAG_QUALIFIED) == 0u)
        {
            validity.flags |= MEASUREMENT_CAL_VALID_FLAG_UNQUALIFIED;
            validity.unqualified_count++;
        }
    }
    if (requirements != NULL)
    {
        for (uint8_t i = 0u; i < requirements->count; i++)
        {
            if (find_record(set, &requirements->keys[i]) == NULL)
            {
                validity.missing_required_count++;
            }
        }
    }
    if (validity.missing_required_count != 0u)
    {
        validity.status = MEASUREMENT_CAL_VALIDITY_INCOMPLETE;
        validity.flags |= MEASUREMENT_CAL_VALID_FLAG_INCOMPLETE;
    }
    return validity;
}

static void fill_from_record(const measurement_cal_record_t *record,
                             measurement_adc_calibration_t *adc,
                             measurement_dsp_config_t *config)
{
    *adc = record->correction.adc;
    *config = measurement_dsp_config_ideal(record->key.range_id);
    config->ret_hg_transfer = record->correction.ret_hg_transfer;
    config->zref_ohms = record->correction.zref_ohms;
}

measurement_cal_resolve_status_t measurement_cal_resolve(
    const measurement_cal_set_t *set,
    const measurement_cal_key_t *key,
    bool allow_ideal_fallback,
    measurement_adc_calibration_t *adc,
    measurement_dsp_config_t *config,
    measurement_calibration_provenance_t *provenance)
{
    if ((key == NULL) || (adc == NULL) || (config == NULL) || (provenance == NULL))
    {
        return MEASUREMENT_CAL_RESOLVE_INVALID_ARG;
    }

    *provenance = (measurement_calibration_provenance_t){
        .source = MEASUREMENT_CAL_SOURCE_NONE,
        .status = MEASUREMENT_CAL_RESOLVE_MISSING,
        .set_sequence = 0u,
        .model_version = key->model_version,
        .condition_id = measurement_cal_condition_id(key),
        .uncalibrated = true,
    };

    if (!key_valid(key))
    {
        provenance->status = MEASUREMENT_CAL_RESOLVE_INCOMPATIBLE;
        return MEASUREMENT_CAL_RESOLVE_INCOMPATIBLE;
    }

    const measurement_cal_record_t *record = find_record(set, key);
    if (record != NULL)
    {
        if (!correction_finite(&record->correction))
        {
            provenance->status = MEASUREMENT_CAL_RESOLVE_CORRUPT;
            return MEASUREMENT_CAL_RESOLVE_CORRUPT;
        }
        fill_from_record(record, adc, config);
        provenance->source = MEASUREMENT_CAL_SOURCE_PERSISTED;
        provenance->set_sequence = (set != NULL) ? set->sequence : 0u;
        provenance->model_version = record->key.model_version;
        provenance->condition_id = record->condition_id;
        provenance->uncalibrated = (record->correction.flags & MEASUREMENT_CAL_FLAG_QUALIFIED) == 0u;
        provenance->status = provenance->uncalibrated ? MEASUREMENT_CAL_RESOLVE_UNQUALIFIED :
                                                        MEASUREMENT_CAL_RESOLVE_FOUND;
        return provenance->status;
    }

    if (allow_ideal_fallback)
    {
        const measurement_cal_record_t ideal = measurement_cal_make_ideal_record(key);
        fill_from_record(&ideal, adc, config);
        provenance->source = MEASUREMENT_CAL_SOURCE_IDEAL;
        provenance->status = MEASUREMENT_CAL_RESOLVE_MISSING;
        provenance->uncalibrated = true;
    }
    return provenance->status;
}

measurement_complex_t measurement_cal_apply_output_correction(
    measurement_complex_t z_ohms,
    const measurement_cal_correction_t *correction)
{
    if ((correction == NULL) ||
        ((correction->flags & MEASUREMENT_CAL_FLAG_OUTPUT_CORRECTION) == 0u))
    {
        return z_ohms;
    }
    return measurement_complex_add(measurement_complex_mul(z_ohms, correction->output_scale),
                                   correction->output_offset_ohms);
}

static void encode_scale(uint8_t **cursor, measurement_adc_scale_t scale)
{
    write_f32(*cursor, scale.code_to_volts);
    *cursor += sizeof(uint32_t);
    write_f32(*cursor, scale.offset_volts);
    *cursor += sizeof(uint32_t);
}

static measurement_adc_scale_t decode_scale(const uint8_t **cursor)
{
    measurement_adc_scale_t scale = {
        .code_to_volts = read_f32(*cursor),
        .offset_volts = read_f32(*cursor + sizeof(uint32_t)),
    };
    *cursor += 2u * sizeof(uint32_t);
    return scale;
}

static void encode_complex(uint8_t **cursor, measurement_complex_t value)
{
    write_f32(*cursor, value.re);
    *cursor += sizeof(uint32_t);
    write_f32(*cursor, value.im);
    *cursor += sizeof(uint32_t);
}

static measurement_complex_t decode_complex(const uint8_t **cursor)
{
    measurement_complex_t value = {
        .re = read_f32(*cursor),
        .im = read_f32(*cursor + sizeof(uint32_t)),
    };
    *cursor += 2u * sizeof(uint32_t);
    return value;
}

static void encode_record(uint8_t *dst, const measurement_cal_record_t *record)
{
    uint8_t *cursor = dst;
    write_u32(cursor, record->key.hardware_revision);
    cursor += sizeof(uint32_t);
    write_u16(cursor, record->key.model_version);
    cursor += sizeof(uint16_t);
    *cursor++ = (uint8_t)record->key.range_id;
    *cursor++ = (uint8_t)record->key.frequency;
    *cursor++ = (uint8_t)record->key.amplitude;
    *cursor++ = (uint8_t)record->key.ret_channel;
    *cursor++ = record->key.ret_strategy;
    *cursor++ = (uint8_t)record->record_type;
    write_i32(cursor, record->temperature_mC);
    cursor += sizeof(uint32_t);
    write_u32(cursor, record->condition_id);
    cursor += sizeof(uint32_t);
    write_u32(cursor, record->correction.flags);
    cursor += sizeof(uint32_t);
    encode_scale(&cursor, record->correction.adc.vexc_1);
    encode_scale(&cursor, record->correction.adc.ret_1x);
    encode_scale(&cursor, record->correction.adc.vexc_2);
    encode_scale(&cursor, record->correction.adc.ret_hg);
    encode_scale(&cursor, record->correction.adc.vmid_adc1);
    encode_scale(&cursor, record->correction.adc.vmid_adc2);
    encode_complex(&cursor, record->correction.ret_hg_transfer);
    encode_complex(&cursor, record->correction.zref_ohms);
    encode_complex(&cursor, record->correction.output_scale);
    encode_complex(&cursor, record->correction.output_offset_ohms);
    while ((uint32_t)(cursor - dst) < CAL_RECORD_BYTES)
    {
        *cursor++ = 0u;
    }
}

static bool decode_record(const uint8_t *src, measurement_cal_record_t *record)
{
    const uint8_t *cursor = src;
    record->key.hardware_revision = read_u32(cursor);
    cursor += sizeof(uint32_t);
    record->key.model_version = read_u16(cursor);
    cursor += sizeof(uint16_t);
    record->key.range_id = (hw_range_id_t)*cursor++;
    record->key.frequency = (hw_excitation_freq_t)*cursor++;
    record->key.amplitude = (hw_excitation_amp_t)*cursor++;
    record->key.ret_channel = (measurement_return_channel_t)*cursor++;
    record->key.ret_strategy = *cursor++;
    record->record_type = (measurement_cal_record_type_t)*cursor++;
    record->temperature_mC = read_i32(cursor);
    cursor += sizeof(uint32_t);
    record->condition_id = read_u32(cursor);
    cursor += sizeof(uint32_t);
    record->correction.flags = read_u32(cursor);
    cursor += sizeof(uint32_t);
    record->correction.adc.vexc_1 = decode_scale(&cursor);
    record->correction.adc.ret_1x = decode_scale(&cursor);
    record->correction.adc.vexc_2 = decode_scale(&cursor);
    record->correction.adc.ret_hg = decode_scale(&cursor);
    record->correction.adc.vmid_adc1 = decode_scale(&cursor);
    record->correction.adc.vmid_adc2 = decode_scale(&cursor);
    record->correction.ret_hg_transfer = decode_complex(&cursor);
    record->correction.zref_ohms = decode_complex(&cursor);
    record->correction.output_scale = decode_complex(&cursor);
    record->correction.output_offset_ohms = decode_complex(&cursor);
    return key_valid(&record->key) &&
           (record->condition_id == measurement_cal_condition_id(&record->key)) &&
           correction_finite(&record->correction);
}

bool measurement_cal_serialize_set(const measurement_cal_set_t *set,
                                   uint8_t *dst,
                                   size_t capacity,
                                   size_t *written)
{
    if ((set == NULL) || (dst == NULL) || (written == NULL) ||
        (set->record_count > MEASUREMENT_CAL_MAX_RECORDS))
    {
        return false;
    }

    const size_t payload_length = SET_PAYLOAD_HEADER_BYTES +
                                  ((size_t)set->record_count * (size_t)CAL_RECORD_BYTES);
    const size_t total = MEASUREMENT_CAL_FRAME_HEADER_BYTES + payload_length;
    if ((capacity < total) || (total > MEASUREMENT_CAL_MAX_FRAME_BYTES))
    {
        return false;
    }

    (void)memset(dst, 0x00, total);
    (void)memset(&dst[FRAME_OFF_RESERVED], 0x00, FRAME_OFF_CRC32 - FRAME_OFF_RESERVED);
    write_u32(&dst[FRAME_OFF_MAGIC], MEASUREMENT_CAL_FRAME_MAGIC);
    write_u16(&dst[FRAME_OFF_RECORD_TYPE], (uint16_t)MEASUREMENT_CAL_RECORD_SET);
    write_u16(&dst[FRAME_OFF_SCHEMA], set->schema_version);
    write_u16(&dst[FRAME_OFF_HEADER_SIZE], MEASUREMENT_CAL_FRAME_HEADER_BYTES);
    write_u16(&dst[FRAME_OFF_PAYLOAD_LENGTH], (uint16_t)payload_length);
    write_u32(&dst[FRAME_OFF_SEQUENCE], set->sequence);
    write_u32(&dst[FRAME_OFF_HARDWARE], set->hardware_revision);
    write_u16(&dst[FRAME_OFF_MODEL], set->model_version);
    write_u16(&dst[FRAME_OFF_FLAGS], 0u);
    write_u32(&dst[FRAME_OFF_CRC32], 0u);
    write_u32(&dst[FRAME_OFF_COMMIT], 0xFFFFFFFFu);

    uint8_t *payload = &dst[MEASUREMENT_CAL_FRAME_HEADER_BYTES];
    write_u16(&payload[0], set->record_count);
    write_u16(&payload[2], 0u);
    for (uint8_t i = 0u; i < set->record_count; i++)
    {
        encode_record(&payload[SET_PAYLOAD_HEADER_BYTES + ((size_t)i * (size_t)CAL_RECORD_BYTES)],
                      &set->records[i]);
    }

    write_u32(&dst[FRAME_OFF_CRC32], crc_frame(dst, payload_length));
    write_u32(&dst[FRAME_OFF_COMMIT], MEASUREMENT_CAL_COMMIT_MARKER);
    *written = total;
    return true;
}

bool measurement_cal_decode_set(const uint8_t *src,
                                size_t size,
                                measurement_cal_set_t *set,
                                measurement_cal_frame_info_t *info)
{
    if ((src == NULL) || (set == NULL) || (size < MEASUREMENT_CAL_FRAME_HEADER_BYTES))
    {
        return false;
    }
    measurement_cal_frame_info_t frame = {
        .magic = read_u32(&src[FRAME_OFF_MAGIC]),
        .record_type = read_u16(&src[FRAME_OFF_RECORD_TYPE]),
        .schema_version = read_u16(&src[FRAME_OFF_SCHEMA]),
        .header_size = read_u16(&src[FRAME_OFF_HEADER_SIZE]),
        .payload_length = read_u16(&src[FRAME_OFF_PAYLOAD_LENGTH]),
        .sequence = read_u32(&src[FRAME_OFF_SEQUENCE]),
        .hardware_revision = read_u32(&src[FRAME_OFF_HARDWARE]),
        .model_version = read_u16(&src[FRAME_OFF_MODEL]),
        .crc32 = read_u32(&src[FRAME_OFF_CRC32]),
        .committed = read_u32(&src[FRAME_OFF_COMMIT]) == MEASUREMENT_CAL_COMMIT_MARKER,
    };
    if (info != NULL)
    {
        *info = frame;
    }
    if ((frame.magic != MEASUREMENT_CAL_FRAME_MAGIC) ||
        (frame.record_type != (uint16_t)MEASUREMENT_CAL_RECORD_SET) ||
        (frame.schema_version != MEASUREMENT_CAL_SCHEMA_VERSION) ||
        (frame.header_size != MEASUREMENT_CAL_FRAME_HEADER_BYTES) ||
        !frame.committed)
    {
        return false;
    }
    const size_t total = (size_t)frame.header_size + (size_t)frame.payload_length;
    if ((total > size) || (total > MEASUREMENT_CAL_MAX_FRAME_BYTES) ||
        (frame.payload_length < SET_PAYLOAD_HEADER_BYTES))
    {
        return false;
    }
    if (crc_frame(src, frame.payload_length) != frame.crc32)
    {
        return false;
    }

    const uint8_t *payload = &src[MEASUREMENT_CAL_FRAME_HEADER_BYTES];
    const uint16_t count = read_u16(&payload[0]);
    if ((count > MEASUREMENT_CAL_MAX_RECORDS) ||
        ((size_t)frame.payload_length !=
         (SET_PAYLOAD_HEADER_BYTES + ((size_t)count * (size_t)CAL_RECORD_BYTES))))
    {
        return false;
    }

    measurement_cal_set_init(set, frame.hardware_revision, frame.model_version, frame.sequence);
    for (uint16_t i = 0u; i < count; i++)
    {
        measurement_cal_record_t record = {0};
        if (!decode_record(&payload[SET_PAYLOAD_HEADER_BYTES + ((size_t)i * (size_t)CAL_RECORD_BYTES)],
                           &record) ||
            !measurement_cal_set_add_record(set, &record))
        {
            return false;
        }
    }
    return true;
}

const char *measurement_cal_resolve_status_string(measurement_cal_resolve_status_t status)
{
    switch (status)
    {
    case MEASUREMENT_CAL_RESOLVE_FOUND:
        return "FOUND";
    case MEASUREMENT_CAL_RESOLVE_MISSING:
        return "MISSING";
    case MEASUREMENT_CAL_RESOLVE_INCOMPATIBLE:
        return "INCOMPATIBLE";
    case MEASUREMENT_CAL_RESOLVE_CORRUPT:
        return "CORRUPT";
    case MEASUREMENT_CAL_RESOLVE_UNQUALIFIED:
        return "UNQUALIFIED";
    case MEASUREMENT_CAL_RESOLVE_INVALID_ARG:
    default:
        return "INVALID_ARG";
    }
}

const char *measurement_cal_validity_status_string(measurement_cal_validity_status_t status)
{
    switch (status)
    {
    case MEASUREMENT_CAL_VALIDITY_VALID:
        return "VALID";
    case MEASUREMENT_CAL_VALIDITY_MISSING:
        return "MISSING";
    case MEASUREMENT_CAL_VALIDITY_CORRUPT:
        return "CORRUPT";
    case MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_SCHEMA:
        return "INCOMPATIBLE_SCHEMA";
    case MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_HARDWARE:
        return "INCOMPATIBLE_HARDWARE";
    case MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_MODEL:
        return "INCOMPATIBLE_MODEL";
    case MEASUREMENT_CAL_VALIDITY_INCOMPLETE:
    default:
        return "INCOMPLETE";
    }
}
