#ifndef WTK_MEASUREMENT_LEAD_COMPENSATION_H
#define WTK_MEASUREMENT_LEAD_COMPENSATION_H
#include "measurement/measurement_calibration.h"

enum { MEASUREMENT_LEAD_FREQUENCIES = 3u, MEASUREMENT_LEAD_COVERAGE_ALL = 7u };
typedef struct
{
    measurement_complex_t series_ohms;
    measurement_complex_t shunt_siemens;
} measurement_lead_frequency_t;
typedef struct
{
    measurement_lead_frequency_t frequencies[MEASUREMENT_LEAD_FREQUENCIES];
    uint8_t uid[12];
    uint32_t sequence, factory_sequence, factory_crc32, hardware_revision;
    uint16_t model_version;
    uint8_t coverage;
    bool enabled, cleared;
} measurement_lead_profile_t;

/* All values describe a delta relative to the existing factory OSL plane.
 * Shorter leads can legitimately produce negative series/shunt deltas. */
measurement_status_t measurement_lead_series(const measurement_cal_osl_coefficients_t *factory,
    measurement_complex_t short_ratio, measurement_complex_t *series_ohms);
measurement_status_t measurement_lead_shunt(const measurement_cal_osl_coefficients_t *factory,
    measurement_complex_t open_ratio, measurement_complex_t series_ohms,
    measurement_complex_t *shunt_siemens);
measurement_status_t measurement_lead_apply_ratio(const measurement_cal_osl_coefficients_t *factory,
    const measurement_lead_frequency_t *lead, measurement_complex_t ratio, measurement_complex_t *z_ohms);
bool measurement_lead_frequency_valid(const measurement_lead_frequency_t *lead, uint32_t frequency_hz);
bool measurement_lead_profile_valid(const measurement_lead_profile_t *profile);
bool measurement_lead_profile_bound(const measurement_lead_profile_t *profile,
    const uint8_t uid[12], uint32_t factory_sequence, uint32_t factory_crc32);
/* A supplied profile must already be bound to this MCU and factory frame CRC. */
bsp_status_t measurement_lead_process_block(const hw_metrology_block_t *block,
    const measurement_cal_set_t *factory, const measurement_cal_key_t *key,
    const measurement_lead_profile_t *lead, measurement_calibrated_result_t *result);
/* Endpoint capture accepts a quiet SHORT signal below the DUT return threshold.
 * Channel is fixed for all seven samples; source and per-stream clipping remain checked. */
bsp_status_t measurement_lead_endpoint_ratio(const hw_metrology_block_t *block,
    const measurement_cal_set_t *factory, const measurement_cal_key_t *key,
    measurement_return_channel_t channel, measurement_complex_t *ratio);
#endif
