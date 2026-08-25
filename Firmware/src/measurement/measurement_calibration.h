#ifndef WTK_MEASUREMENT_CALIBRATION_H
#define WTK_MEASUREMENT_CALIBRATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hardware/hw_excitation.h"
#include "hardware/hw_range.h"
#include "measurement/measurement_condition.h"
#include "measurement/measurement_dsp.h"

enum
{
    MEASUREMENT_CAL_SCHEMA_VERSION = 2u,
    MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V1 = 1u,
    MEASUREMENT_CAL_MODEL_VERSION_DIRECT_V2 = 2u,
    MEASUREMENT_CAL_MODEL_VERSION_OSL_MOBIUS_V1 = 3u,
    MEASUREMENT_CAL_MODEL_VERSION_CURRENT = MEASUREMENT_CAL_MODEL_VERSION_OSL_MOBIUS_V1,
    MEASUREMENT_CAL_HARDWARE_REV1 = 0x00010001u,
    MEASUREMENT_CAL_FRAME_MAGIC = 0x434C4157u,
    MEASUREMENT_CAL_FRAME_HEADER_BYTES = 64u,
    MEASUREMENT_CAL_COMMIT_MARKER = 0x54494D43u,
    MEASUREMENT_CAL_MAX_RECORDS = MEASUREMENT_CONDITION_REV1_MAX_SUPPORTED,
    MEASUREMENT_CAL_MAX_REQUIRED_KEYS = MEASUREMENT_CONDITION_REV1_MAX_SUPPORTED,
    MEASUREMENT_CAL_MAX_FRAME_BYTES = 3072u,
};

typedef enum
{
    MEASUREMENT_CAL_RECORD_SET = 1,
    MEASUREMENT_CAL_RECORD_CONDITION = 2,
    MEASUREMENT_CAL_RECORD_OPEN_EVIDENCE = 3,
    MEASUREMENT_CAL_RECORD_SHORT_EVIDENCE = 4,
    MEASUREMENT_CAL_RECORD_LOAD_EVIDENCE = 5,
} measurement_cal_record_type_t;

typedef enum
{
    MEASUREMENT_CAL_FLAG_ADC = 1u << 0,
    MEASUREMENT_CAL_FLAG_RET_HG = 1u << 1,
    MEASUREMENT_CAL_FLAG_ZREF = 1u << 2,
    MEASUREMENT_CAL_FLAG_OUTPUT_CORRECTION_1X = 1u << 3,
    MEASUREMENT_CAL_FLAG_OUTPUT_CORRECTION_HG = 1u << 4,
    MEASUREMENT_CAL_FLAG_OSL_MOBIUS = 1u << 5,
    MEASUREMENT_CAL_FLAG_TEMPERATURE_VALID = 1u << 6,
    MEASUREMENT_CAL_FLAG_HG_OBSERVED = 1u << 7,
    MEASUREMENT_CAL_FLAG_QUALIFIED = 1u << 8,
} measurement_cal_flags_t;

typedef enum
{
    MEASUREMENT_CAL_SOURCE_NONE = 0,
    MEASUREMENT_CAL_SOURCE_IDEAL,
    MEASUREMENT_CAL_SOURCE_PERSISTED,
} measurement_cal_source_t;

typedef enum
{
    MEASUREMENT_CAL_RESOLVE_FOUND = 0,
    MEASUREMENT_CAL_RESOLVE_MISSING,
    MEASUREMENT_CAL_RESOLVE_INCOMPATIBLE,
    MEASUREMENT_CAL_RESOLVE_CORRUPT,
    MEASUREMENT_CAL_RESOLVE_UNQUALIFIED,
    MEASUREMENT_CAL_RESOLVE_INVALID_ARG,
} measurement_cal_resolve_status_t;

typedef enum
{
    MEASUREMENT_CAL_VALIDITY_VALID = 0,
    MEASUREMENT_CAL_VALIDITY_MISSING,
    MEASUREMENT_CAL_VALIDITY_CORRUPT,
    MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_SCHEMA,
    MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_HARDWARE,
    MEASUREMENT_CAL_VALIDITY_INCOMPATIBLE_MODEL,
    MEASUREMENT_CAL_VALIDITY_INCOMPLETE,
} measurement_cal_validity_status_t;

typedef enum
{
    MEASUREMENT_CAL_VALID_FLAG_NONE = 0u,
    MEASUREMENT_CAL_VALID_FLAG_MISSING = 1u << 0,
    MEASUREMENT_CAL_VALID_FLAG_CORRUPT = 1u << 1,
    MEASUREMENT_CAL_VALID_FLAG_SCHEMA = 1u << 2,
    MEASUREMENT_CAL_VALID_FLAG_HARDWARE = 1u << 3,
    MEASUREMENT_CAL_VALID_FLAG_MODEL = 1u << 4,
    MEASUREMENT_CAL_VALID_FLAG_INCOMPLETE = 1u << 5,
    MEASUREMENT_CAL_VALID_FLAG_UNQUALIFIED = 1u << 6,
} measurement_cal_validity_flags_t;

typedef struct
{
    uint32_t hardware_revision;
    uint16_t model_version;
    hw_range_id_t range_id;
    hw_excitation_freq_t frequency;
    hw_excitation_amp_t amplitude;
} measurement_cal_key_t;

typedef struct
{
    measurement_complex_t scale;
    measurement_complex_t offset_ohms;
} measurement_cal_output_correction_t;

typedef struct
{
    measurement_complex_t ret_hg_transfer;
    measurement_complex_t zref_ohms;
    measurement_cal_output_correction_t ret_1x_output;
    measurement_cal_output_correction_t ret_hg_output;
    uint32_t flags;
} measurement_cal_correction_t;

typedef struct
{
    measurement_complex_t ret_hg_transfer;
    measurement_complex_t load_z_ohms;
    measurement_complex_t t_short;
    measurement_complex_t t_open;
    measurement_complex_t k;
} measurement_cal_osl_coefficients_t;

typedef struct
{
    measurement_cal_key_t key;
    measurement_cal_correction_t correction;
    measurement_cal_record_type_t record_type;
    int32_t temperature_mC;
    uint32_t condition_id;
} measurement_cal_record_t;

typedef struct
{
    uint32_t sequence;
    uint32_t hardware_revision;
    uint16_t schema_version;
    uint16_t model_version;
    bool adc_valid;
    measurement_adc_calibration_t adc;
    uint8_t record_count;
    measurement_cal_record_t records[MEASUREMENT_CAL_MAX_RECORDS];
} measurement_cal_set_t;

typedef struct
{
    uint8_t count;
    measurement_cal_key_t keys[MEASUREMENT_CAL_MAX_REQUIRED_KEYS];
} measurement_cal_requirements_t;

typedef struct
{
    measurement_cal_validity_status_t status;
    uint32_t flags;
    uint8_t missing_required_count;
    uint8_t unqualified_count;
} measurement_cal_validity_t;

typedef struct
{
    measurement_cal_source_t source;
    measurement_cal_resolve_status_t status;
    uint32_t set_sequence;
    uint16_t model_version;
    uint32_t condition_id;
    bool uncalibrated;
} measurement_calibration_provenance_t;

typedef struct
{
    measurement_adc_calibration_t adc;
    measurement_dsp_config_t config;
    measurement_cal_correction_t correction;
    measurement_calibration_provenance_t provenance;
} measurement_cal_resolved_t;

typedef struct
{
    measurement_result_t result;
    measurement_complex_t raw_z_ohms;
    measurement_calibration_provenance_t provenance;
    bool output_corrected;
} measurement_calibrated_result_t;

typedef struct
{
    uint32_t magic;
    uint16_t record_type;
    uint16_t schema_version;
    uint16_t header_size;
    uint16_t payload_length;
    uint32_t sequence;
    uint32_t hardware_revision;
    uint16_t model_version;
    uint32_t crc32;
    bool committed;
} measurement_cal_frame_info_t;

measurement_cal_key_t measurement_cal_key(uint32_t hardware_revision,
                                          uint16_t model_version,
                                          hw_range_id_t range_id,
                                          hw_excitation_freq_t frequency,
                                          hw_excitation_amp_t amplitude);
bool measurement_cal_key_equal(const measurement_cal_key_t *a, const measurement_cal_key_t *b);
uint32_t measurement_cal_condition_id(const measurement_cal_key_t *key);
bool measurement_cal_condition_allowed(hw_range_id_t range_id,
                                       hw_excitation_freq_t frequency,
                                       hw_excitation_amp_t amplitude);

void measurement_cal_set_init(measurement_cal_set_t *set,
                              uint32_t hardware_revision,
                              uint16_t model_version,
                              uint32_t sequence);
measurement_cal_record_t measurement_cal_make_ideal_record(const measurement_cal_key_t *key);
measurement_cal_correction_t measurement_cal_make_osl_correction(
    const measurement_cal_osl_coefficients_t *coefficients,
    bool temperature_valid,
    bool hg_observed);
bool measurement_cal_get_osl_coefficients(const measurement_cal_correction_t *correction,
                                          measurement_cal_osl_coefficients_t *coefficients);
bool measurement_cal_set_add_record(measurement_cal_set_t *set, const measurement_cal_record_t *record);
bool measurement_cal_set_replace_record(measurement_cal_set_t *set, const measurement_cal_record_t *record);
measurement_cal_requirements_t measurement_cal_requirements_empty(void);
measurement_cal_requirements_t measurement_cal_requirements_rev1_full(void);
bool measurement_cal_requirements_add(measurement_cal_requirements_t *requirements,
                                      const measurement_cal_key_t *key);
measurement_cal_validity_t measurement_cal_validate_set(
    const measurement_cal_set_t *set,
    const measurement_cal_requirements_t *requirements,
    uint32_t hardware_revision,
    uint16_t model_version);

measurement_cal_resolve_status_t measurement_cal_resolve_condition(
    const measurement_cal_set_t *set,
    const measurement_cal_key_t *key,
    bool allow_ideal_fallback,
    measurement_cal_resolved_t *resolved);
measurement_cal_resolve_status_t measurement_cal_resolve(
    const measurement_cal_set_t *set,
    const measurement_cal_key_t *key,
    bool allow_ideal_fallback,
    measurement_adc_calibration_t *adc,
    measurement_dsp_config_t *config,
    measurement_calibration_provenance_t *provenance);

measurement_complex_t measurement_cal_apply_output_correction(
    measurement_complex_t z_ohms,
    const measurement_cal_correction_t *correction,
    measurement_return_channel_t selected_channel,
    bool *applied);
bsp_status_t measurement_cal_process_block(const hw_metrology_block_t *block,
                                           const measurement_cal_set_t *set,
                                           const measurement_cal_key_t *key,
                                           bool allow_ideal_fallback,
                                           measurement_calibrated_result_t *result);

bool measurement_cal_serialize_set(const measurement_cal_set_t *set,
                                   uint8_t *dst,
                                   size_t capacity,
                                   size_t *written);
bool measurement_cal_serialize_set_with_header(const measurement_cal_set_t *set,
                                               uint16_t schema_version,
                                               uint16_t model_version,
                                               uint32_t sequence,
                                               uint8_t *dst,
                                               size_t capacity,
                                               size_t *written);
bool measurement_cal_decode_set(const uint8_t *src,
                                size_t size,
                                measurement_cal_set_t *set,
                                measurement_cal_frame_info_t *info);
measurement_cal_validity_t measurement_cal_inspect_frame(const uint8_t *src,
                                                         size_t size,
                                                         uint32_t hardware_revision,
                                                         uint16_t model_version,
                                                         measurement_cal_frame_info_t *info);

uint32_t measurement_cal_record_size_bytes(void);
uint32_t measurement_cal_key_size_bytes(void);
uint32_t measurement_cal_set_size_bytes(void);
uint32_t measurement_cal_requirements_size_bytes(void);
const char *measurement_cal_resolve_status_string(measurement_cal_resolve_status_t status);
const char *measurement_cal_validity_status_string(measurement_cal_validity_status_t status);

#endif
