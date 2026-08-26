#include "measurement/measurement_calibration.h"

#include <math.h>
#include <string.h>

#include "measurement/measurement_condition.h"
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
    SET_PAYLOAD_HEADER_BYTES = 56u,
    CAL_RECORD_BYTES = 80u,
};

static bool finite_f(float value)
{
    return isfinite(value) != 0;
}

static measurement_complex_t complex_neg(measurement_complex_t value)
{
    return measurement_complex(-value.re, -value.im);
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

static uint32_t frequency_hz(hw_excitation_freq_t frequency)
{
    switch (frequency)
    {
    case HW_EXCITATION_FREQ_100HZ:
        return 100u;
    case HW_EXCITATION_FREQ_1KHZ:
        return 1000u;
    case HW_EXCITATION_FREQ_10KHZ:
        return 10000u;
    case HW_EXCITATION_FREQ_INVALID:
    default:
        return 0u;
    }
}

bool measurement_cal_condition_allowed(hw_range_id_t range_id,
                                       hw_excitation_freq_t frequency,
                                       hw_excitation_amp_t amplitude)
{
    return measurement_condition_calibratable(range_id, frequency, amplitude);
}

measurement_cal_key_t measurement_cal_key(uint32_t hardware_revision,
                                          uint16_t model_version,
                                          hw_range_id_t range_id,
                                          hw_excitation_freq_t frequency,
                                          hw_excitation_amp_t amplitude)
{
    const measurement_cal_key_t key = {
        .hardware_revision = hardware_revision,
        .model_version = model_version,
        .range_id = range_id,
        .frequency = frequency,
        .amplitude = amplitude,
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
           (a->amplitude == b->amplitude);
}

static int16_t find_record_index(const measurement_cal_set_t *set, const measurement_cal_key_t *key)
{
    if ((set == NULL) || (key == NULL))
    {
        return -1;
    }
    for (uint8_t i = 0u; i < set->record_count; i++)
    {
        if (measurement_cal_key_equal(&set->records[i].key, key))
        {
            return (int16_t)i;
        }
    }
    return -1;
}

uint32_t measurement_cal_condition_id(const measurement_cal_key_t *key)
{
    if (key == NULL)
    {
        return 0u;
    }
    uint8_t bytes[12] = {0};
    write_u32(&bytes[0], key->hardware_revision);
    write_u16(&bytes[4], key->model_version);
    bytes[6] = (uint8_t)key->range_id;
    bytes[7] = (uint8_t)key->frequency;
    bytes[8] = (uint8_t)key->amplitude;
    return storage_crc32(bytes, sizeof(bytes));
}

static bool key_valid(const measurement_cal_key_t *key)
{
    return (key != NULL) &&
           (key->hardware_revision == MEASUREMENT_CAL_HARDWARE_REV1) &&
           (key->model_version == MEASUREMENT_CAL_MODEL_VERSION_CURRENT) &&
           measurement_cal_condition_allowed(key->range_id, key->frequency, key->amplitude);
}

static bool model_is_direct(uint16_t model_version)
{
    return (model_version == MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1) ||
           (model_version == MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V2);
}

static bool model_is_osl(uint16_t model_version)
{
    return (model_version == MEASUREMENT_CAL_MODEL_VERSION_OSL_MOBIUS_V1) ||
           (model_version == MEASUREMENT_CAL_MODEL_VERSION_OSL_MOBIUS_EFFECTIVE_HG_V1);
}

static bool scale_finite(measurement_adc_scale_t scale)
{
    return finite_f(scale.code_to_volts) && finite_f(scale.offset_volts);
}

static bool adc_finite(const measurement_adc_calibration_t *adc)
{
    return (adc != NULL) &&
           scale_finite(adc->vexc_1) &&
           scale_finite(adc->ret_1x) &&
           scale_finite(adc->vexc_2) &&
           scale_finite(adc->ret_hg) &&
           scale_finite(adc->vmid_adc1) &&
           scale_finite(adc->vmid_adc2);
}

static bool output_finite(const measurement_cal_output_correction_t *correction)
{
    return (correction != NULL) &&
           measurement_complex_is_finite(correction->scale) &&
           measurement_complex_is_finite(correction->offset_ohms);
}

static bool correction_finite(const measurement_cal_correction_t *correction)
{
    return (correction != NULL) &&
           measurement_complex_is_finite(correction->ret_hg_transfer) &&
           measurement_complex_is_finite(correction->zref_ohms) &&
           output_finite(&correction->ret_1x_output) &&
           output_finite(&correction->ret_hg_output);
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
    set->adc = measurement_adc_calibration_ideal();
    set->adc_valid = true;
}

measurement_cal_record_t measurement_cal_make_ideal_record(const measurement_cal_key_t *key)
{
    measurement_cal_record_t record = {0};
    if (key != NULL)
    {
        record.key = *key;
    }
    measurement_dsp_config_t config = measurement_dsp_config_ideal(record.key.range_id);
    if ((key != NULL) && model_is_osl(key->model_version))
    {
        const measurement_cal_osl_coefficients_t ideal = {
            .ret_hg_transfer = config.ret_hg_transfer,
            .load_z_ohms = config.zref_ohms,
            .t_short = measurement_complex(0.0f, 0.0f),
            .t_open = measurement_complex(1.0f, 0.0f),
            .k = complex_neg(config.zref_ohms),
        };
        record.correction = measurement_cal_make_osl_correction(&ideal, false, false);
    }
    else
    {
        record.correction.ret_hg_transfer = config.ret_hg_transfer;
        record.correction.zref_ohms = config.zref_ohms;
        record.correction.ret_1x_output.scale = measurement_complex(1.0f, 0.0f);
        record.correction.ret_1x_output.offset_ohms = measurement_complex(0.0f, 0.0f);
        record.correction.ret_hg_output.scale = measurement_complex(1.0f, 0.0f);
        record.correction.ret_hg_output.offset_ohms = measurement_complex(0.0f, 0.0f);
        record.correction.flags = MEASUREMENT_CAL_FLAG_RET_HG |
                                  MEASUREMENT_CAL_FLAG_ZREF |
                                  MEASUREMENT_CAL_FLAG_OUTPUT_CORRECTION_1X |
                                  MEASUREMENT_CAL_FLAG_OUTPUT_CORRECTION_HG;
    }
    record.record_type = MEASUREMENT_CAL_RECORD_CONDITION;
    record.temperature_mC = 0;
    record.condition_id = measurement_cal_condition_id(&record.key);
    return record;
}

measurement_cal_correction_t measurement_cal_make_osl_correction(
    const measurement_cal_osl_coefficients_t *coefficients,
    bool temperature_valid,
    bool hg_observed)
{
    measurement_cal_correction_t correction = {0};
    if (coefficients == NULL)
    {
        return correction;
    }
    correction.ret_hg_transfer = coefficients->ret_hg_transfer;
    correction.zref_ohms = coefficients->load_z_ohms;
    correction.ret_1x_output.scale = coefficients->t_short;
    correction.ret_1x_output.offset_ohms = coefficients->t_open;
    correction.ret_hg_output.scale = coefficients->k;
    correction.ret_hg_output.offset_ohms = measurement_complex(0.0f, 0.0f);
    correction.flags = MEASUREMENT_CAL_FLAG_OSL_MOBIUS |
                       MEASUREMENT_CAL_FLAG_LOAD_REFERENCE;
    if (temperature_valid)
    {
        correction.flags |= MEASUREMENT_CAL_FLAG_TEMPERATURE_VALID;
    }
    if (hg_observed)
    {
        correction.flags |= MEASUREMENT_CAL_FLAG_HG_OBSERVED;
    }
    return correction;
}

bool measurement_cal_get_osl_coefficients(const measurement_cal_correction_t *correction,
                                          measurement_cal_osl_coefficients_t *coefficients)
{
    if ((correction == NULL) || (coefficients == NULL) ||
        ((correction->flags & MEASUREMENT_CAL_FLAG_OSL_MOBIUS) == 0u) ||
        !correction_finite(correction))
    {
        return false;
    }
    *coefficients = (measurement_cal_osl_coefficients_t){
        .ret_hg_transfer = correction->ret_hg_transfer,
        .load_z_ohms = correction->zref_ohms,
        .t_short = correction->ret_1x_output.scale,
        .t_open = correction->ret_1x_output.offset_ohms,
        .k = correction->ret_hg_output.scale,
    };
    return measurement_complex_is_finite(coefficients->ret_hg_transfer) &&
           measurement_complex_is_finite(coefficients->load_z_ohms) &&
           measurement_complex_is_finite(coefficients->t_short) &&
           measurement_complex_is_finite(coefficients->t_open) &&
           measurement_complex_is_finite(coefficients->k);
}

bool measurement_cal_validate_osl_correction(const measurement_cal_correction_t *correction)
{
    measurement_cal_osl_coefficients_t coefficients;
    if (!measurement_cal_get_osl_coefficients(correction, &coefficients))
    {
        return false;
    }
    const uint32_t legacy_direct_flags = MEASUREMENT_CAL_FLAG_ZREF |
                                         MEASUREMENT_CAL_FLAG_OUTPUT_CORRECTION_1X |
                                         MEASUREMENT_CAL_FLAG_OUTPUT_CORRECTION_HG;
    if ((correction->flags & MEASUREMENT_CAL_FLAG_LOAD_REFERENCE) == 0u)
    {
        return false;
    }
    if ((correction->flags & legacy_direct_flags) != 0u)
    {
        return false;
    }
    if (measurement_complex_near_zero(coefficients.ret_hg_transfer, 1.0e-6f) ||
        measurement_complex_near_zero(coefficients.load_z_ohms, 1.0e-6f) ||
        measurement_complex_near_zero(coefficients.k, 1.0e-6f) ||
        measurement_complex_near_zero(measurement_complex_sub(coefficients.t_open,
                                                              coefficients.t_short),
                                      1.0e-5f))
    {
        return false;
    }
    return true;
}

bool measurement_cal_set_add_record(measurement_cal_set_t *set, const measurement_cal_record_t *record)
{
    if ((set == NULL) || (record == NULL) || (set->record_count >= MEASUREMENT_CAL_MAX_RECORDS) ||
        (record->record_type != MEASUREMENT_CAL_RECORD_CONDITION) || !key_valid(&record->key) ||
        !correction_finite(&record->correction) ||
        (model_is_osl(record->key.model_version) &&
         !measurement_cal_validate_osl_correction(&record->correction)) ||
        (record->condition_id != measurement_cal_condition_id(&record->key)) ||
        (find_record_index(set, &record->key) >= 0))
    {
        return false;
    }
    set->records[set->record_count] = *record;
    set->record_count++;
    return true;
}

bool measurement_cal_set_replace_record(measurement_cal_set_t *set, const measurement_cal_record_t *record)
{
    if ((set == NULL) || (record == NULL) ||
        (record->record_type != MEASUREMENT_CAL_RECORD_CONDITION) || !key_valid(&record->key) ||
        !correction_finite(&record->correction) ||
        (model_is_osl(record->key.model_version) &&
         !measurement_cal_validate_osl_correction(&record->correction)) ||
        (record->condition_id != measurement_cal_condition_id(&record->key)))
    {
        return false;
    }
    const int16_t index = find_record_index(set, &record->key);
    if (index < 0)
    {
        return false;
    }
    set->records[(uint8_t)index] = *record;
    return true;
}

measurement_cal_requirements_t measurement_cal_requirements_empty(void)
{
    const measurement_cal_requirements_t requirements = {0};
    return requirements;
}

bool measurement_cal_requirements_add(measurement_cal_requirements_t *requirements,
                                      const measurement_cal_key_t *key)
{
    if ((requirements == NULL) || (key == NULL) ||
        (requirements->count >= MEASUREMENT_CAL_MAX_REQUIRED_KEYS) || !key_valid(key))
    {
        return false;
    }
    for (uint8_t i = 0u; i < requirements->count; i++)
    {
        if (measurement_cal_key_equal(&requirements->keys[i], key))
        {
            return false;
        }
    }
    requirements->keys[requirements->count] = *key;
    requirements->count++;
    return true;
}

measurement_cal_requirements_t measurement_cal_requirements_rev1_full(void)
{
    measurement_cal_requirements_t requirements = measurement_cal_requirements_empty();
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
                    (void)measurement_cal_requirements_add(&requirements, &key);
                }
            }
        }
    }
    return requirements;
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
    if (!set->adc_valid || !adc_finite(&set->adc))
    {
        validity.status = MEASUREMENT_CAL_VALIDITY_INCOMPLETE;
        validity.flags |= MEASUREMENT_CAL_VALID_FLAG_INCOMPLETE;
        return validity;
    }
    for (uint8_t i = 0u; i < set->record_count; i++)
    {
        const measurement_cal_record_t *record = &set->records[i];
        if ((record->record_type != MEASUREMENT_CAL_RECORD_CONDITION) ||
            !key_valid(&record->key) ||
            !correction_finite(&record->correction) ||
            (model_is_osl(record->key.model_version) &&
             !measurement_cal_validate_osl_correction(&record->correction)) ||
            (record->condition_id != measurement_cal_condition_id(&record->key)))
        {
            validity.status = MEASUREMENT_CAL_VALIDITY_CORRUPT;
            validity.flags |= MEASUREMENT_CAL_VALID_FLAG_CORRUPT;
            return validity;
        }
        for (uint8_t j = (uint8_t)(i + 1u); j < set->record_count; j++)
        {
            if (measurement_cal_key_equal(&record->key, &set->records[j].key))
            {
                validity.status = MEASUREMENT_CAL_VALIDITY_CORRUPT;
                validity.flags |= MEASUREMENT_CAL_VALID_FLAG_CORRUPT;
                return validity;
            }
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

static void fill_from_record(const measurement_cal_set_t *set,
                             const measurement_cal_record_t *record,
                             measurement_cal_resolved_t *resolved)
{
    resolved->adc = ((set != NULL) && set->adc_valid) ? set->adc : measurement_adc_calibration_ideal();
    resolved->config = measurement_dsp_config_ideal(record->key.range_id);
    resolved->config.ret_hg_transfer = record->correction.ret_hg_transfer;
    if (model_is_direct(record->key.model_version))
    {
        resolved->config.zref_ohms = record->correction.zref_ohms;
    }
    resolved->correction = record->correction;
}

measurement_cal_resolve_status_t measurement_cal_resolve_condition(
    const measurement_cal_set_t *set,
    const measurement_cal_key_t *key,
    bool allow_ideal_fallback,
    measurement_cal_resolved_t *resolved)
{
    if ((key == NULL) || (resolved == NULL))
    {
        return MEASUREMENT_CAL_RESOLVE_INVALID_ARG;
    }

    *resolved = (measurement_cal_resolved_t){0};
    resolved->provenance = (measurement_calibration_provenance_t){
        .source = MEASUREMENT_CAL_SOURCE_NONE,
        .status = MEASUREMENT_CAL_RESOLVE_MISSING,
        .set_sequence = 0u,
        .model_version = key->model_version,
        .condition_id = measurement_cal_condition_id(key),
        .uncalibrated = true,
    };

    if (!key_valid(key))
    {
        resolved->provenance.status = MEASUREMENT_CAL_RESOLVE_INCOMPATIBLE;
        return MEASUREMENT_CAL_RESOLVE_INCOMPATIBLE;
    }

    const measurement_cal_record_t *record = find_record(set, key);
    if (record != NULL)
    {
        if (((set != NULL) && (!set->adc_valid || !adc_finite(&set->adc))) ||
            !correction_finite(&record->correction) ||
            (model_is_osl(record->key.model_version) &&
             !measurement_cal_validate_osl_correction(&record->correction)))
        {
            resolved->provenance.status = MEASUREMENT_CAL_RESOLVE_CORRUPT;
            return MEASUREMENT_CAL_RESOLVE_CORRUPT;
        }
        fill_from_record(set, record, resolved);
        resolved->provenance.source = MEASUREMENT_CAL_SOURCE_PERSISTED;
        resolved->provenance.set_sequence = (set != NULL) ? set->sequence : 0u;
        resolved->provenance.model_version = record->key.model_version;
        resolved->provenance.condition_id = record->condition_id;
        resolved->provenance.uncalibrated =
            (record->correction.flags & MEASUREMENT_CAL_FLAG_QUALIFIED) == 0u;
        resolved->provenance.status = resolved->provenance.uncalibrated ?
                                          MEASUREMENT_CAL_RESOLVE_UNQUALIFIED :
                                          MEASUREMENT_CAL_RESOLVE_FOUND;
        return resolved->provenance.status;
    }

    if (allow_ideal_fallback)
    {
        const measurement_cal_record_t ideal = measurement_cal_make_ideal_record(key);
        fill_from_record(NULL, &ideal, resolved);
        resolved->provenance.source = MEASUREMENT_CAL_SOURCE_IDEAL;
        resolved->provenance.status = MEASUREMENT_CAL_RESOLVE_MISSING;
        resolved->provenance.uncalibrated = true;
    }
    return resolved->provenance.status;
}

measurement_cal_resolve_status_t measurement_cal_resolve(
    const measurement_cal_set_t *set,
    const measurement_cal_key_t *key,
    bool allow_ideal_fallback,
    measurement_adc_calibration_t *adc,
    measurement_dsp_config_t *config,
    measurement_calibration_provenance_t *provenance)
{
    if ((adc == NULL) || (config == NULL) || (provenance == NULL))
    {
        return MEASUREMENT_CAL_RESOLVE_INVALID_ARG;
    }
    measurement_cal_resolved_t resolved;
    const measurement_cal_resolve_status_t status =
        measurement_cal_resolve_condition(set, key, allow_ideal_fallback, &resolved);
    if ((status == MEASUREMENT_CAL_RESOLVE_FOUND) ||
        (status == MEASUREMENT_CAL_RESOLVE_UNQUALIFIED) ||
        (status == MEASUREMENT_CAL_RESOLVE_MISSING))
    {
        *adc = resolved.adc;
        *config = resolved.config;
    }
    *provenance = resolved.provenance;
    return status;
}

measurement_complex_t measurement_cal_apply_output_correction(
    measurement_complex_t z_ohms,
    const measurement_cal_correction_t *correction,
    measurement_return_channel_t selected_channel,
    bool *applied)
{
    if (applied != NULL)
    {
        *applied = false;
    }
    if (correction == NULL)
    {
        return z_ohms;
    }
    const bool use_hg = selected_channel == MEASUREMENT_RETURN_HG;
    const uint32_t flag = use_hg ? MEASUREMENT_CAL_FLAG_OUTPUT_CORRECTION_HG :
                                   MEASUREMENT_CAL_FLAG_OUTPUT_CORRECTION_1X;
    const measurement_cal_output_correction_t *selected =
        use_hg ? &correction->ret_hg_output : &correction->ret_1x_output;
    if ((correction->flags & flag) == 0u)
    {
        return z_ohms;
    }
    if (applied != NULL)
    {
        *applied = true;
    }
    return measurement_complex_add(measurement_complex_mul(z_ohms, selected->scale),
                                   selected->offset_ohms);
}

static bool stream_clipped(const hw_metrology_block_t *block, hw_metrology_stream_t stream)
{
    return (block != NULL) && (stream < HW_METROLOGY_STREAM_COUNT) &&
           block->streams[stream].hard_clipped;
}

static measurement_status_t compute_t(measurement_complex_t vx,
                                      measurement_complex_t vs,
                                      float source_min,
                                      measurement_complex_t *t)
{
    if (t == NULL)
    {
        return MEASUREMENT_STATUS_INVALID_ARG;
    }
    if (measurement_complex_near_zero(vs, source_min))
    {
        *t = measurement_complex(0.0f, 0.0f);
        return MEASUREMENT_STATUS_SOURCE_TOO_SMALL;
    }
    return measurement_complex_div(vx, vs, t);
}

static measurement_impedance_result_t compute_osl_impedance(measurement_complex_t vs,
                                                            measurement_complex_t vx,
                                                            const measurement_cal_osl_coefficients_t *coefficients,
                                                            const measurement_dsp_config_t *config,
                                                            measurement_return_channel_t channel)
{
    measurement_impedance_result_t result = {
        .status = MEASUREMENT_STATUS_OK,
        .channel = channel,
        .vs_v = vs,
        .vx_v = vx,
        .z_ohms = measurement_complex(0.0f, 0.0f),
        .open_like = false,
        .short_like = false,
    };
    if ((coefficients == NULL) || (config == NULL))
    {
        result.status = MEASUREMENT_STATUS_INVALID_ARG;
        return result;
    }
    if (!measurement_complex_is_finite(vs) || !measurement_complex_is_finite(vx) ||
        !measurement_complex_is_finite(coefficients->t_short) ||
        !measurement_complex_is_finite(coefficients->t_open) ||
        !measurement_complex_is_finite(coefficients->k))
    {
        result.status = MEASUREMENT_STATUS_NONFINITE;
        return result;
    }

    measurement_complex_t t = measurement_complex(0.0f, 0.0f);
    result.status = compute_t(vx, vs, config->source_min_v_peak, &t);
    if (result.status != MEASUREMENT_STATUS_OK)
    {
        return result;
    }
    const measurement_complex_t numerator = measurement_complex_sub(t, coefficients->t_short);
    const measurement_complex_t denominator = measurement_complex_sub(t, coefficients->t_open);
    if (measurement_complex_near_zero(denominator, 1.0e-5f))
    {
        result.status = MEASUREMENT_STATUS_DENOMINATOR_TOO_SMALL;
        result.open_like = true;
        return result;
    }
    if (measurement_complex_near_zero(numerator, 1.0e-5f) ||
        measurement_complex_near_zero(vx, config->return_min_v_peak))
    {
        result.short_like = true;
    }

    measurement_complex_t ratio = measurement_complex(0.0f, 0.0f);
    result.status = measurement_complex_div(numerator, denominator, &ratio);
    if (result.status != MEASUREMENT_STATUS_OK)
    {
        return result;
    }
    result.z_ohms = measurement_complex_mul(coefficients->k, ratio);
    if (!measurement_complex_is_finite(result.z_ohms))
    {
        result.status = MEASUREMENT_STATUS_NONFINITE;
    }
    return result;
}

static bsp_status_t process_osl_block(const hw_metrology_block_t *block,
                                      const measurement_cal_resolved_t *resolved,
                                      const measurement_cal_key_t *key,
                                      measurement_calibrated_result_t *result)
{
    measurement_cal_osl_coefficients_t coefficients;
    if (!measurement_cal_get_osl_coefficients(&resolved->correction, &coefficients))
    {
        return BSP_STATUS_ERROR;
    }
    if (measurement_extract_phasors(block, &resolved->adc, &resolved->config, &result->result.phasors) !=
        BSP_STATUS_OK)
    {
        result->result.status = block->clipped ? MEASUREMENT_STATUS_CLIPPED : MEASUREMENT_STATUS_INVALID_ARG;
        return BSP_STATUS_ERROR;
    }

    const measurement_complex_t ret_1x_signal =
        measurement_complex_sub(result->result.phasors.ret_1x, result->result.phasors.vmid);
    const measurement_complex_t ret_hg_signal =
        measurement_complex_sub(result->result.phasors.ret_hg_reconstructed, result->result.phasors.vmid);
    const bool vmid_clipped = stream_clipped(block, HW_METROLOGY_STREAM_VMID_ADC1) ||
                              stream_clipped(block, HW_METROLOGY_STREAM_VMID_ADC2);
    const bool ret_1x_clipped = stream_clipped(block, HW_METROLOGY_STREAM_RET_1X) ||
                                stream_clipped(block, HW_METROLOGY_STREAM_VEXC_1) ||
                                vmid_clipped;
    const bool ret_hg_clipped = stream_clipped(block, HW_METROLOGY_STREAM_RET_HG) ||
                                stream_clipped(block, HW_METROLOGY_STREAM_VEXC_2) ||
                                vmid_clipped;
    result->result.ret_1x_quality = measurement_channel_quality(&ret_1x_signal,
                                                                ret_1x_clipped,
                                                                true,
                                                                resolved->config.return_min_v_peak);
    result->result.ret_hg_quality =
        measurement_channel_quality(&ret_hg_signal,
                                    ret_hg_clipped,
                                    !measurement_complex_near_zero(resolved->config.ret_hg_transfer, 1.0e-6f) &&
                                        ((resolved->provenance.source == MEASUREMENT_CAL_SOURCE_IDEAL) ||
                                         ((resolved->correction.flags & MEASUREMENT_CAL_FLAG_HG_OBSERVED) != 0u)),
                                    resolved->config.return_min_v_peak);
    result->result.selected_channel = measurement_select_return_channel(&result->result.ret_1x_quality,
                                                                        &result->result.ret_hg_quality);
    if ((result->result.selected_channel == MEASUREMENT_RETURN_HG) && result->result.ret_hg_quality.usable)
    {
        result->result.impedance =
            compute_osl_impedance(measurement_complex_sub(result->result.phasors.vexc_2,
                                                          result->result.phasors.vmid),
                                  ret_hg_signal,
                                  &coefficients,
                                  &resolved->config,
                                  MEASUREMENT_RETURN_HG);
    }
    else if (result->result.ret_1x_quality.usable)
    {
        result->result.selected_channel = MEASUREMENT_RETURN_1X;
        result->result.impedance =
            compute_osl_impedance(measurement_complex_sub(result->result.phasors.vexc_1,
                                                          result->result.phasors.vmid),
                                  ret_1x_signal,
                                  &coefficients,
                                  &resolved->config,
                                  MEASUREMENT_RETURN_1X);
    }
    else
    {
        result->result.status = block->clipped ? MEASUREMENT_STATUS_CLIPPED :
                                                 MEASUREMENT_STATUS_CHANNEL_UNUSABLE;
        return BSP_STATUS_ERROR;
    }

    result->result.status = result->result.impedance.status;
    result->raw_z_ohms = result->result.impedance.z_ohms;
    result->output_corrected = true;
    result->result.derived = measurement_derive_quantities(result->result.impedance.z_ohms,
                                                           frequency_hz(key->frequency),
                                                           &resolved->config,
                                                           result->result.impedance.status);
    return (result->result.status == MEASUREMENT_STATUS_OK) ? BSP_STATUS_OK : BSP_STATUS_ERROR;
}

bsp_status_t measurement_cal_process_block(const hw_metrology_block_t *block,
                                           const measurement_cal_set_t *set,
                                           const measurement_cal_key_t *key,
                                           bool allow_ideal_fallback,
                                           measurement_calibrated_result_t *result)
{
    if ((block == NULL) || (key == NULL) || (result == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *result = (measurement_calibrated_result_t){0};
    measurement_cal_resolved_t resolved;
    const measurement_cal_resolve_status_t resolve_status =
        measurement_cal_resolve_condition(set, key, allow_ideal_fallback, &resolved);
    if ((resolve_status != MEASUREMENT_CAL_RESOLVE_FOUND) &&
        (resolve_status != MEASUREMENT_CAL_RESOLVE_UNQUALIFIED) &&
        (resolve_status != MEASUREMENT_CAL_RESOLVE_MISSING))
    {
        result->provenance = resolved.provenance;
        return BSP_STATUS_ERROR;
    }

    result->provenance = resolved.provenance;
    if (model_is_osl(key->model_version))
    {
        if (!measurement_cal_validate_osl_correction(&resolved.correction))
        {
            result->provenance.status = MEASUREMENT_CAL_RESOLVE_CORRUPT;
            return BSP_STATUS_ERROR;
        }
        return process_osl_block(block, &resolved, key, result);
    }

    const bsp_status_t status =
        measurement_process_block(block, &resolved.adc, &resolved.config, &result->result);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    result->raw_z_ohms = result->result.impedance.z_ohms;
    const measurement_complex_t corrected =
        measurement_cal_apply_output_correction(result->raw_z_ohms,
                                                &resolved.correction,
                                                result->result.selected_channel,
                                                &result->output_corrected);
    if (result->output_corrected)
    {
        result->result.impedance.z_ohms = corrected;
        result->result.derived =
            measurement_derive_quantities(corrected,
                                          frequency_hz(key->frequency),
                                          &resolved.config,
                                          result->result.impedance.status);
    }
    return BSP_STATUS_OK;
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
    const measurement_adc_scale_t scale = {
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
    const measurement_complex_t value = {
        .re = read_f32(*cursor),
        .im = read_f32(*cursor + sizeof(uint32_t)),
    };
    *cursor += 2u * sizeof(uint32_t);
    return value;
}

static void encode_adc(uint8_t **cursor, const measurement_adc_calibration_t *adc)
{
    encode_scale(cursor, adc->vexc_1);
    encode_scale(cursor, adc->ret_1x);
    encode_scale(cursor, adc->vexc_2);
    encode_scale(cursor, adc->ret_hg);
    encode_scale(cursor, adc->vmid_adc1);
    encode_scale(cursor, adc->vmid_adc2);
}

static void decode_adc(const uint8_t **cursor, measurement_adc_calibration_t *adc)
{
    adc->vexc_1 = decode_scale(cursor);
    adc->ret_1x = decode_scale(cursor);
    adc->vexc_2 = decode_scale(cursor);
    adc->ret_hg = decode_scale(cursor);
    adc->vmid_adc1 = decode_scale(cursor);
    adc->vmid_adc2 = decode_scale(cursor);
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
    *cursor++ = (uint8_t)record->record_type;
    write_i32(cursor, record->temperature_mC);
    cursor += sizeof(uint32_t);
    write_u32(cursor, record->condition_id);
    cursor += sizeof(uint32_t);
    write_u32(cursor, record->correction.flags);
    cursor += sizeof(uint32_t);
    encode_complex(&cursor, record->correction.ret_hg_transfer);
    encode_complex(&cursor, record->correction.zref_ohms);
    encode_complex(&cursor, record->correction.ret_1x_output.scale);
    encode_complex(&cursor, record->correction.ret_1x_output.offset_ohms);
    encode_complex(&cursor, record->correction.ret_hg_output.scale);
    encode_complex(&cursor, record->correction.ret_hg_output.offset_ohms);
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
    record->record_type = (measurement_cal_record_type_t)*cursor++;
    record->temperature_mC = read_i32(cursor);
    cursor += sizeof(uint32_t);
    record->condition_id = read_u32(cursor);
    cursor += sizeof(uint32_t);
    record->correction.flags = read_u32(cursor);
    cursor += sizeof(uint32_t);
    record->correction.ret_hg_transfer = decode_complex(&cursor);
    record->correction.zref_ohms = decode_complex(&cursor);
    record->correction.ret_1x_output.scale = decode_complex(&cursor);
    record->correction.ret_1x_output.offset_ohms = decode_complex(&cursor);
    record->correction.ret_hg_output.scale = decode_complex(&cursor);
    record->correction.ret_hg_output.offset_ohms = decode_complex(&cursor);
    return (record->record_type == MEASUREMENT_CAL_RECORD_CONDITION) &&
           key_valid(&record->key) &&
           (record->condition_id == measurement_cal_condition_id(&record->key)) &&
           correction_finite(&record->correction) &&
           (!model_is_osl(record->key.model_version) ||
            measurement_cal_validate_osl_correction(&record->correction));
}

static bool erased_header(const uint8_t *src, size_t size)
{
    if (src == NULL)
    {
        return false;
    }
    const size_t inspect = (size < MEASUREMENT_CAL_FRAME_HEADER_BYTES) ?
                               size :
                               (size_t)MEASUREMENT_CAL_FRAME_HEADER_BYTES;
    for (size_t i = 0u; i < inspect; i++)
    {
        if (src[i] != 0xFFu)
        {
            return false;
        }
    }
    return inspect != 0u;
}

measurement_cal_validity_t measurement_cal_inspect_frame(const uint8_t *src,
                                                         size_t size,
                                                         uint32_t hardware_revision,
                                                         uint16_t model_version,
                                                         measurement_cal_frame_info_t *info)
{
    measurement_cal_validity_t validity = {
        .status = MEASUREMENT_CAL_VALIDITY_CORRUPT,
        .flags = MEASUREMENT_CAL_VALID_FLAG_CORRUPT,
        .missing_required_count = 0u,
        .unqualified_count = 0u,
    };
    if ((src == NULL) || (size < MEASUREMENT_CAL_FRAME_HEADER_BYTES))
    {
        if (erased_header(src, size))
        {
            validity.status = MEASUREMENT_CAL_VALIDITY_MISSING;
            validity.flags = MEASUREMENT_CAL_VALID_FLAG_MISSING;
        }
        if (info != NULL)
        {
            *info = (measurement_cal_frame_info_t){0};
        }
        return validity;
    }
    if (erased_header(src, size))
    {
        validity.status = MEASUREMENT_CAL_VALIDITY_MISSING;
        validity.flags = MEASUREMENT_CAL_VALID_FLAG_MISSING;
        if (info != NULL)
        {
            *info = (measurement_cal_frame_info_t){0};
        }
        return validity;
    }

    const measurement_cal_frame_info_t frame = {
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
        (frame.header_size != MEASUREMENT_CAL_FRAME_HEADER_BYTES) ||
        !frame.committed)
    {
        return validity;
    }
    const size_t total = (size_t)frame.header_size + (size_t)frame.payload_length;
    if ((total > size) || (total > MEASUREMENT_CAL_MAX_FRAME_BYTES) ||
        (frame.payload_length < SET_PAYLOAD_HEADER_BYTES))
    {
        return validity;
    }
    if (crc_frame(src, frame.payload_length) != frame.crc32)
    {
        return validity;
    }
    if (frame.schema_version != MEASUREMENT_CAL_SCHEMA_VERSION)
    {
        validity.status = MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_SCHEMA;
        validity.flags = MEASUREMENT_CAL_VALID_FLAG_SCHEMA;
        return validity;
    }
    if (frame.hardware_revision != hardware_revision)
    {
        validity.status = MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_HARDWARE;
        validity.flags = MEASUREMENT_CAL_VALID_FLAG_HARDWARE;
        return validity;
    }
    if (frame.model_version != model_version)
    {
        validity.status = MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_MODEL;
        validity.flags = MEASUREMENT_CAL_VALID_FLAG_MODEL;
        return validity;
    }
    validity.status = MEASUREMENT_CAL_VALIDITY_VALID;
    validity.flags = MEASUREMENT_CAL_VALID_FLAG_NONE;
    return validity;
}

bool measurement_cal_serialize_set_with_header(const measurement_cal_set_t *set,
                                               uint16_t schema_version,
                                               uint16_t model_version,
                                               uint32_t sequence,
                                               uint8_t *dst,
                                               size_t capacity,
                                               size_t *written)
{
    if ((set == NULL) || (dst == NULL) || (written == NULL) ||
        (set->record_count > MEASUREMENT_CAL_MAX_RECORDS) ||
        !adc_finite(&set->adc))
    {
        return false;
    }

    const size_t payload_length = SET_PAYLOAD_HEADER_BYTES +
                                  ((size_t)set->record_count * (size_t)CAL_RECORD_BYTES);
    const size_t total = MEASUREMENT_CAL_FRAME_HEADER_BYTES + payload_length;
    if ((capacity < total) || (total > MEASUREMENT_CAL_MAX_FRAME_BYTES) ||
        (payload_length > 0xFFFFu))
    {
        return false;
    }

    (void)memset(dst, 0x00, total);
    (void)memset(&dst[FRAME_OFF_RESERVED], 0x00, FRAME_OFF_CRC32 - FRAME_OFF_RESERVED);
    write_u32(&dst[FRAME_OFF_MAGIC], MEASUREMENT_CAL_FRAME_MAGIC);
    write_u16(&dst[FRAME_OFF_RECORD_TYPE], (uint16_t)MEASUREMENT_CAL_RECORD_SET);
    write_u16(&dst[FRAME_OFF_SCHEMA], schema_version);
    write_u16(&dst[FRAME_OFF_HEADER_SIZE], MEASUREMENT_CAL_FRAME_HEADER_BYTES);
    write_u16(&dst[FRAME_OFF_PAYLOAD_LENGTH], (uint16_t)payload_length);
    write_u32(&dst[FRAME_OFF_SEQUENCE], sequence);
    write_u32(&dst[FRAME_OFF_HARDWARE], set->hardware_revision);
    write_u16(&dst[FRAME_OFF_MODEL], model_version);
    write_u16(&dst[FRAME_OFF_FLAGS], set->adc_valid ? (uint16_t)MEASUREMENT_CAL_FLAG_ADC : 0u);
    write_u32(&dst[FRAME_OFF_CRC32], 0u);
    write_u32(&dst[FRAME_OFF_COMMIT], 0xFFFFFFFFu);

    uint8_t *payload = &dst[MEASUREMENT_CAL_FRAME_HEADER_BYTES];
    write_u16(&payload[0], set->record_count);
    write_u16(&payload[2], 0u);
    write_u32(&payload[4], set->adc_valid ? (uint32_t)MEASUREMENT_CAL_FLAG_ADC : 0u);
    uint8_t *cursor = &payload[8];
    encode_adc(&cursor, &set->adc);
    for (uint8_t i = 0u; i < set->record_count; i++)
    {
        if ((set->records[i].condition_id != measurement_cal_condition_id(&set->records[i].key)) ||
            !key_valid(&set->records[i].key) ||
            !correction_finite(&set->records[i].correction) ||
            (model_is_osl(set->records[i].key.model_version) &&
             !measurement_cal_validate_osl_correction(&set->records[i].correction)))
        {
            return false;
        }
        for (uint8_t j = (uint8_t)(i + 1u); j < set->record_count; j++)
        {
            if (measurement_cal_key_equal(&set->records[i].key, &set->records[j].key))
            {
                return false;
            }
        }
        encode_record(&payload[SET_PAYLOAD_HEADER_BYTES + ((size_t)i * (size_t)CAL_RECORD_BYTES)],
                      &set->records[i]);
    }

    write_u32(&dst[FRAME_OFF_CRC32], crc_frame(dst, payload_length));
    write_u32(&dst[FRAME_OFF_COMMIT], MEASUREMENT_CAL_COMMIT_MARKER);
    *written = total;
    return true;
}

bool measurement_cal_serialize_set(const measurement_cal_set_t *set,
                                   uint8_t *dst,
                                   size_t capacity,
                                   size_t *written)
{
    if (set == NULL)
    {
        return false;
    }
    return measurement_cal_serialize_set_with_header(set,
                                                     set->schema_version,
                                                     set->model_version,
                                                     set->sequence,
                                                     dst,
                                                     capacity,
                                                     written);
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
    const measurement_cal_frame_info_t frame = {
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
    const uint32_t adc_flags = read_u32(&payload[4]);
    if ((count > MEASUREMENT_CAL_MAX_RECORDS) ||
        ((size_t)frame.payload_length !=
         (SET_PAYLOAD_HEADER_BYTES + ((size_t)count * (size_t)CAL_RECORD_BYTES))) ||
        ((adc_flags & MEASUREMENT_CAL_FLAG_ADC) == 0u))
    {
        return false;
    }

    measurement_cal_set_init(set, frame.hardware_revision, frame.model_version, frame.sequence);
    set->adc_valid = true;
    const uint8_t *cursor = &payload[8];
    decode_adc(&cursor, &set->adc);
    if (!adc_finite(&set->adc))
    {
        return false;
    }

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

uint32_t measurement_cal_record_size_bytes(void)
{
    return (uint32_t)sizeof(measurement_cal_record_t);
}

uint32_t measurement_cal_key_size_bytes(void)
{
    return (uint32_t)sizeof(measurement_cal_key_t);
}

uint32_t measurement_cal_set_size_bytes(void)
{
    return (uint32_t)sizeof(measurement_cal_set_t);
}

uint32_t measurement_cal_requirements_size_bytes(void)
{
    return (uint32_t)sizeof(measurement_cal_requirements_t);
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
