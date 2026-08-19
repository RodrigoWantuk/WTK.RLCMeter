#ifndef WTK_HW_METROLOGY_MEASURE_H
#define WTK_HW_METROLOGY_MEASURE_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_clock.h"
#include "bsp/bsp_status.h"
#include "hardware/hw_excitation.h"
#include "hardware/hw_k1.h"
#include "hardware/hw_measure_permit.h"
#include "hardware/hw_metrology_clock.h"
#include "hardware/hw_metrology_raw.h"
#include "hardware/hw_range.h"
#include "hardware/hw_safety.h"

enum
{
    HW_METROLOGY_MEASURE_K1_OPERATE_GUARD_MS = 10u,
    HW_METROLOGY_MEASURE_K1_RELEASE_GUARD_MS = 8u,
    HW_METROLOGY_MEASURE_RANGE_TIMEOUT_MS = 50u,
    HW_METROLOGY_MEASURE_DMA_TIMEOUT_MARGIN_MS = 50u,
};

typedef enum
{
    HW_METROLOGY_MEASURE_IDLE = 0,
    HW_METROLOGY_MEASURE_FORCE_K1_SAFE,
    HW_METROLOGY_MEASURE_RANGE_REQUEST,
    HW_METROLOGY_MEASURE_RANGE_WAIT,
    HW_METROLOGY_MEASURE_EXC_NEUTRAL,
    HW_METROLOGY_MEASURE_EXC_NEUTRAL_WAIT,
    HW_METROLOGY_MEASURE_QUIET_ENTER,
    HW_METROLOGY_MEASURE_PERMIT_ISSUE,
    HW_METROLOGY_MEASURE_PERMIT_VALIDATE,
    HW_METROLOGY_MEASURE_AUX_PAUSE,
    HW_METROLOGY_MEASURE_K1_REQUEST,
    HW_METROLOGY_MEASURE_K1_OPERATE_GUARD,
    HW_METROLOGY_MEASURE_ADC_ACQUIRE,
    HW_METROLOGY_MEASURE_EXC_SINE_START,
    HW_METROLOGY_MEASURE_EXC_SETTLE,
    HW_METROLOGY_MEASURE_ADC_DMA_START,
    HW_METROLOGY_MEASURE_ADC_DMA_WAIT,
    HW_METROLOGY_MEASURE_ADC_DMA_COMPLETE,
    HW_METROLOGY_MEASURE_EXC_NEUTRAL_POST,
    HW_METROLOGY_MEASURE_EXC_NEUTRAL_POST_WAIT,
    HW_METROLOGY_MEASURE_K1_SAFE,
    HW_METROLOGY_MEASURE_K1_RELEASE_GUARD,
    HW_METROLOGY_MEASURE_EXC_OFF,
    HW_METROLOGY_MEASURE_RANGE_DISABLE,
    HW_METROLOGY_MEASURE_ADC_RELEASE,
    HW_METROLOGY_MEASURE_ADC_RESTORE,
    HW_METROLOGY_MEASURE_AUX_RESUME,
    HW_METROLOGY_MEASURE_QUIET_EXIT,
    HW_METROLOGY_MEASURE_ANALYZE,
    HW_METROLOGY_MEASURE_DONE,
    HW_METROLOGY_MEASURE_ABORT,
} hw_metrology_measure_state_t;

typedef enum
{
    HW_METROLOGY_MEASURE_OK = 0,
    HW_METROLOGY_MEASURE_ERR_CLOCK,
    HW_METROLOGY_MEASURE_ERR_BUSY,
    HW_METROLOGY_MEASURE_ERR_INVALID,
    HW_METROLOGY_MEASURE_ERR_FORBIDDEN_AMPLITUDE,
    HW_METROLOGY_MEASURE_ERR_RANGE,
    HW_METROLOGY_MEASURE_ERR_PERMIT,
    HW_METROLOGY_MEASURE_ERR_K1,
    HW_METROLOGY_MEASURE_ERR_ADC_ACQUIRE,
    HW_METROLOGY_MEASURE_ERR_EXCITATION,
    HW_METROLOGY_MEASURE_ERR_DMA,
    HW_METROLOGY_MEASURE_ERR_TIMEOUT,
    HW_METROLOGY_MEASURE_ERR_AUX_RESTORE,
    HW_METROLOGY_MEASURE_ERR_ABORT,
} hw_metrology_measure_error_t;

typedef struct
{
    bsp_status_t (*k1_force_safe)(void *user);
    bsp_status_t (*k1_request_measure)(const hw_safety_result_t *permission, void *user);
    hw_k1_state_t (*k1_commanded_state)(void *user);
    bsp_status_t (*range_request)(hw_range_id_t id, uint32_t now_ms, void *user);
    bsp_status_t (*range_step)(uint32_t now_ms, void *user);
    bool (*range_is_ready)(void *user);
    hw_range_id_t (*range_current_id)(void *user);
    hw_safety_range_state_t (*range_safety_state)(void *user);
    bsp_status_t (*range_force_disabled)(void *user);
    void (*quiet_request)(bool requested, void *user);
    void (*aux_pause)(void *user);
    void (*aux_resume)(uint32_t now_ms, void *user);
    bsp_status_t (*adc_acquire)(uint32_t now_ms, void *user);
    bsp_status_t (*adc_start_capture)(uint32_t *raw_words,
                                      uint32_t word_count,
                                      const hw_metrology_adc_profile_t *profile,
                                      void *user);
    void (*adc_stop)(void *user);
    bsp_status_t (*adc_restore)(uint32_t now_ms, void *user);
    bool (*adc_dma_complete)(void *user);
    bool (*adc_dma_error)(void *user);
    bsp_status_t (*excitation_off)(void *user);
    bsp_status_t (*excitation_neutral)(void *user);
    bsp_status_t (*excitation_sine)(hw_excitation_freq_t frequency,
                                    hw_excitation_amp_t amplitude,
                                    void *user);
    hw_excitation_mode_t (*excitation_mode)(void *user);
    bool (*excitation_dma_error)(void *user);
    hw_charger_state_t (*charger_state)(void *user);
    uint32_t (*safety_fault_mask)(void *user);
    bsp_status_t (*permit_issue_input)(hw_measure_permit_issue_input_t *input, void *user);
    bsp_status_t (*permit_validate_input)(hw_measure_permit_validate_input_t *input, void *user);
    void (*latch_k1_io_fault)(void *user);
    void (*latch_range_io_fault)(void *user);
    void (*latch_adc_runtime_fault)(void *user);
    void (*latch_metrology_runtime_fault)(void *user);
    void *user;
} hw_metrology_measure_io_t;

typedef struct
{
    const bsp_clock_summary_t *clock_summary;
    bsp_status_t clock_init_status;
    hw_excitation_freq_t frequency;
    hw_excitation_amp_t amplitude;
    hw_range_id_t range_id;
} hw_metrology_measure_request_t;

typedef struct
{
    hw_metrology_measure_io_t io;
    hw_metrology_measure_state_t state;
    hw_metrology_measure_error_t error;
    hw_metrology_measure_request_t request;
    hw_measure_permit_t permit;
    hw_excitation_freq_profile_t exc_profile;
    hw_metrology_adc_profile_t adc_profile;
    hw_metrology_block_t block;
    uint32_t *raw_words;
    uint32_t raw_word_count;
    uint32_t wait_deadline_ms;
    uint32_t sequence;
    bool quiet_held;
    bool aux_paused;
    bool adc_owned;
    bool dumpable;
    bool adc_restore_failed;
    bool permit_validated;
    bool k1_owned;
    bool k1_reached_measure;
    bool abort_release_guard_pending;
    uint8_t abort_phase;
} hw_metrology_measure_t;

bsp_status_t hw_metrology_measure_init(hw_metrology_measure_t *measure,
                                       const hw_metrology_measure_io_t *io,
                                       uint32_t *raw_words,
                                       uint32_t raw_word_count);
bsp_status_t hw_metrology_measure_start(hw_metrology_measure_t *measure,
                                        const hw_metrology_measure_request_t *request,
                                        uint32_t now_ms);
bsp_status_t hw_metrology_measure_step(hw_metrology_measure_t *measure, uint32_t now_ms);
hw_metrology_measure_state_t hw_metrology_measure_state(const hw_metrology_measure_t *measure);
hw_metrology_measure_error_t hw_metrology_measure_error(const hw_metrology_measure_t *measure);
bool hw_metrology_measure_active(const hw_metrology_measure_t *measure);
bool hw_metrology_measure_dumpable(const hw_metrology_measure_t *measure);
bool hw_metrology_measure_k1_owned(void);
bool hw_metrology_measure_k1_measure_active(void);
const hw_metrology_block_t *hw_metrology_measure_block(const hw_metrology_measure_t *measure);
void hw_metrology_measure_acknowledge(hw_metrology_measure_t *measure);
const char *hw_metrology_measure_state_string(hw_metrology_measure_state_t state);
const char *hw_metrology_measure_error_string(hw_metrology_measure_error_t error);

#endif
