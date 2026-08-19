#ifndef WTK_HW_METROLOGY_SESSION_H
#define WTK_HW_METROLOGY_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_clock.h"
#include "bsp/bsp_status.h"
#include "hardware/hw_excitation.h"
#include "hardware/hw_k1.h"
#include "hardware/hw_metrology_clock.h"
#include "hardware/hw_metrology_raw.h"
#include "hardware/hw_range.h"
#include "hardware/hw_safety.h"

enum
{
    HW_METROLOGY_SESSION_RANGE_TIMEOUT_MS = 50u,
    HW_METROLOGY_SESSION_DMA_TIMEOUT_MARGIN_MS = 50u,
};

typedef enum
{
    HW_METROLOGY_SESSION_IDLE = 0,
    HW_METROLOGY_SESSION_FORCE_K1_SAFE,
    HW_METROLOGY_SESSION_RANGE_REQUEST,
    HW_METROLOGY_SESSION_RANGE_WAIT,
    HW_METROLOGY_SESSION_QUIET_ENTER,
    HW_METROLOGY_SESSION_AUX_PAUSE,
    HW_METROLOGY_SESSION_ADC_ACQUIRE,
    HW_METROLOGY_SESSION_EXC_NEUTRAL,
    HW_METROLOGY_SESSION_EXC_NEUTRAL_WAIT,
    HW_METROLOGY_SESSION_EXC_SINE_START,
    HW_METROLOGY_SESSION_EXC_SETTLE,
    HW_METROLOGY_SESSION_ADC_DMA_START,
    HW_METROLOGY_SESSION_ADC_DMA_WAIT,
    HW_METROLOGY_SESSION_ADC_DMA_COMPLETE,
    HW_METROLOGY_SESSION_EXC_OFF,
    HW_METROLOGY_SESSION_ADC_RELEASE,
    HW_METROLOGY_SESSION_AUX_RESTORE,
    HW_METROLOGY_SESSION_AUX_RESUME,
    HW_METROLOGY_SESSION_RANGE_DISABLE,
    HW_METROLOGY_SESSION_QUIET_EXIT,
    HW_METROLOGY_SESSION_ANALYZE,
    HW_METROLOGY_SESSION_DONE,
    HW_METROLOGY_SESSION_ABORT,
} hw_metrology_session_state_t;

typedef enum
{
    HW_METROLOGY_SESSION_OK = 0,
    HW_METROLOGY_SESSION_ERR_CLOCK,
    HW_METROLOGY_SESSION_ERR_BUSY,
    HW_METROLOGY_SESSION_ERR_INVALID,
    HW_METROLOGY_SESSION_ERR_FORBIDDEN_AMPLITUDE,
    HW_METROLOGY_SESSION_ERR_RANGE,
    HW_METROLOGY_SESSION_ERR_ADC_ACQUIRE,
    HW_METROLOGY_SESSION_ERR_EXCITATION,
    HW_METROLOGY_SESSION_ERR_DMA,
    HW_METROLOGY_SESSION_ERR_TIMEOUT,
    HW_METROLOGY_SESSION_ERR_AUX_RESTORE,
} hw_metrology_session_error_t;

typedef struct
{
    bsp_status_t (*k1_force_safe)(void *user);
    hw_k1_state_t (*k1_commanded_state)(void *user);
    bsp_status_t (*range_request)(hw_range_id_t id, uint32_t now_ms, void *user);
    bsp_status_t (*range_step)(uint32_t now_ms, void *user);
    bool (*range_is_ready)(void *user);
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
    void (*latch_k1_io_fault)(void *user);
    void (*latch_range_io_fault)(void *user);
    void (*latch_adc_runtime_fault)(void *user);
    void (*latch_metrology_runtime_fault)(void *user);
    void *user;
} hw_metrology_session_io_t;

typedef struct
{
    const bsp_clock_summary_t *clock_summary;
    bsp_status_t clock_init_status;
    hw_excitation_freq_t frequency;
    hw_excitation_amp_t amplitude;
    hw_range_id_t range_id;
} hw_metrology_session_request_t;

typedef struct
{
    hw_metrology_session_io_t io;
    hw_metrology_session_state_t state;
    hw_metrology_session_error_t error;
    hw_metrology_session_request_t request;
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
    bool k1_left_safe;
    bool dumpable;
    bool adc_restore_failed;
} hw_metrology_session_t;

bsp_status_t hw_metrology_session_init(hw_metrology_session_t *session,
                                       const hw_metrology_session_io_t *io,
                                       uint32_t *raw_words,
                                       uint32_t raw_word_count);
bsp_status_t hw_metrology_session_start(hw_metrology_session_t *session,
                                        const hw_metrology_session_request_t *request,
                                        uint32_t now_ms);
bsp_status_t hw_metrology_session_step(hw_metrology_session_t *session, uint32_t now_ms);
hw_metrology_session_state_t hw_metrology_session_state(const hw_metrology_session_t *session);
hw_metrology_session_error_t hw_metrology_session_error(const hw_metrology_session_t *session);
bool hw_metrology_session_active(const hw_metrology_session_t *session);
bool hw_metrology_session_dumpable(const hw_metrology_session_t *session);
bool hw_metrology_session_k1_left_safe(const hw_metrology_session_t *session);
const hw_metrology_block_t *hw_metrology_session_block(const hw_metrology_session_t *session);
void hw_metrology_session_acknowledge(hw_metrology_session_t *session);
const char *hw_metrology_session_state_string(hw_metrology_session_state_t state);
const char *hw_metrology_session_error_string(hw_metrology_session_error_t error);

#endif
