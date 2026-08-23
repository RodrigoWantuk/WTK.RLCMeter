#ifndef WTK_MEASUREMENT_ENGINE_H
#define WTK_MEASUREMENT_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/hw_excitation.h"
#include "hardware/hw_range.h"
#include "measurement/measurement_calibration.h"
#include "measurement/measurement_dsp.h"

enum
{
    MEASUREMENT_AUTO_MAX_ATTEMPTS = 6u,
    MEASUREMENT_AUTO_MAX_RANGE_TRANSITIONS = 4u,
    MEASUREMENT_AUTO_MAX_FREQUENCY_REFINEMENTS = 1u,
    MEASUREMENT_AUTO_MAX_REPEATED_CONDITIONS = 1u,
    MEASUREMENT_AUTO_INDEX_NONE = 255u,
};

typedef enum
{
    MEASUREMENT_AUTO_MODE_CLICK = 0,
    MEASUREMENT_AUTO_MODE_LIVE,
} measurement_auto_mode_t;

typedef enum
{
    MEASUREMENT_ATTEMPT_INITIAL_PROBE = 0,
    MEASUREMENT_ATTEMPT_PREVIOUS_HINT,
    MEASUREMENT_ATTEMPT_RERANGE,
    MEASUREMENT_ATTEMPT_IMPROVE_SNR,
    MEASUREMENT_ATTEMPT_AVOID_CLIPPING,
    MEASUREMENT_ATTEMPT_CHECK_SECOND_FREQUENCY,
    MEASUREMENT_ATTEMPT_VERIFY_CLASSIFICATION,
    MEASUREMENT_ATTEMPT_CHECK_REACTIVE_PARAMETER,
} measurement_attempt_reason_t;

typedef enum
{
    /* Phase 05 captures both RET paths; Phase 06 chooses the mathematically usable channel. */
    MEASUREMENT_RET_STRATEGY_DSP_AUTO = 0,
    MEASUREMENT_RET_STRATEGY_1X,
    MEASUREMENT_RET_STRATEGY_HG,
    MEASUREMENT_RET_STRATEGY_AUTO = MEASUREMENT_RET_STRATEGY_DSP_AUTO,
} measurement_ret_strategy_t;

typedef enum
{
    MEASUREMENT_AUTO_EVENT_NONE = 0,
    MEASUREMENT_AUTO_EVENT_ATTEMPT_READY,
    MEASUREMENT_AUTO_EVENT_PARTIAL_RESULT,
    MEASUREMENT_AUTO_EVENT_FINAL_RESULT,
} measurement_auto_event_t;

typedef enum
{
    MEASUREMENT_AUTO_STATUS_IDLE = 0,
    MEASUREMENT_AUTO_STATUS_RUNNING,
    MEASUREMENT_AUTO_STATUS_FINAL_OK,
    MEASUREMENT_AUTO_STATUS_OPEN_LIKE,
    MEASUREMENT_AUTO_STATUS_SHORT_LIKE,
    MEASUREMENT_AUTO_STATUS_NO_VALID_CONDITION,
    MEASUREMENT_AUTO_STATUS_SAFETY_ABORT,
    MEASUREMENT_AUTO_STATUS_CANCELED,
    MEASUREMENT_AUTO_STATUS_FAILED,
} measurement_auto_status_t;

typedef enum
{
    MEASUREMENT_QUALITY_GOOD = 0,
    MEASUREMENT_QUALITY_DEGRADED,
    MEASUREMENT_QUALITY_INVALID,
} measurement_quality_class_t;

typedef enum
{
    MEASUREMENT_CONFIDENCE_NOMINAL = 0,
    MEASUREMENT_CONFIDENCE_EXTENDED,
    MEASUREMENT_CONFIDENCE_LOW_CONFIDENCE,
    MEASUREMENT_CONFIDENCE_REJECTED,
} measurement_confidence_class_t;

typedef enum
{
    MEASUREMENT_QUALIFICATION_UNQUALIFIED = 0,
    MEASUREMENT_QUALIFICATION_NOMINAL,
    MEASUREMENT_QUALIFICATION_EXTENDED,
    MEASUREMENT_QUALIFICATION_DISABLED,
} measurement_qualification_t;

typedef enum
{
    MEASUREMENT_AUTO_REASON_NONE = 0u,
    MEASUREMENT_AUTO_REASON_UNQUALIFIED = 1u << 0,
    MEASUREMENT_AUTO_REASON_CALIBRATION_PROVISIONAL = 1u << 1,
    MEASUREMENT_AUTO_REASON_DSP_ERROR = 1u << 2,
    MEASUREMENT_AUTO_REASON_CLIPPED = 1u << 3,
    MEASUREMENT_AUTO_REASON_SOURCE_LOW = 1u << 4,
    MEASUREMENT_AUTO_REASON_RETURN_LOW = 1u << 5,
    MEASUREMENT_AUTO_REASON_DENOMINATOR_LOW = 1u << 6,
    MEASUREMENT_AUTO_REASON_OPEN_LIKE = 1u << 7,
    MEASUREMENT_AUTO_REASON_SHORT_LIKE = 1u << 8,
    MEASUREMENT_AUTO_REASON_HG_PROVISIONAL = 1u << 9,
    MEASUREMENT_AUTO_REASON_AMBIGUOUS = 1u << 10,
    MEASUREMENT_AUTO_REASON_INCONSISTENT = 1u << 11,
    MEASUREMENT_AUTO_REASON_ATTEMPT_LIMIT = 1u << 12,
    MEASUREMENT_AUTO_REASON_SAFETY_ABORT = 1u << 13,
    MEASUREMENT_AUTO_REASON_CANCELED = 1u << 14,
    MEASUREMENT_AUTO_REASON_HARDWARE_ERROR = 1u << 15,
    MEASUREMENT_AUTO_REASON_PRIMARY_PROVISIONAL = 1u << 16,
    MEASUREMENT_AUTO_REASON_FREQUENCY_TREND_CAPACITIVE = 1u << 17,
    MEASUREMENT_AUTO_REASON_FREQUENCY_TREND_INDUCTIVE = 1u << 18,
    MEASUREMENT_AUTO_REASON_FREQUENCY_TREND_INCONSISTENT = 1u << 19,
    MEASUREMENT_AUTO_REASON_X_DOMINANT = 1u << 20,
    MEASUREMENT_AUTO_REASON_R_DOMINANT = 1u << 21,
    MEASUREMENT_AUTO_REASON_RET_OVERLAP = 1u << 22,
} measurement_auto_reason_flags_t;

typedef struct
{
    hw_range_id_t range_id;
    hw_excitation_freq_t frequency;
    hw_excitation_amp_t amplitude;
    measurement_ret_strategy_t ret_strategy;
    uint8_t attempt_number;
    measurement_attempt_reason_t reason;
} measurement_attempt_config_t;

typedef struct
{
    bool valid;
    hw_range_id_t range_id;
    hw_excitation_freq_t frequency;
    hw_excitation_amp_t amplitude;
    measurement_return_channel_t ret_channel;
} measurement_auto_hint_t;

typedef struct
{
    bool ret_1x_valid;
    bool ret_hg_valid;
    bool both_usable;
    bool agreement_available;
    measurement_return_channel_t selected;
    float ret_1x_peak_v;
    float ret_hg_peak_v;
    uint32_t reason_flags;
} measurement_ret_evidence_t;

typedef struct
{
    measurement_attempt_config_t config;
    measurement_status_t dsp_status;
    measurement_complex_t z_ohms;
    measurement_derived_result_t derived;
    measurement_channel_quality_t ret_1x_quality;
    measurement_channel_quality_t ret_hg_quality;
    measurement_return_channel_t selected_channel;
    measurement_ret_evidence_t ret_evidence;
    float source_peak_v;
    float return_peak_v;
    float denominator_peak_v;
    float dut_zref_ratio;
    bool open_like;
    bool short_like;
    bool clipped;
    bool phase05_failed;
    bool safety_abort;
    bool canceled;
    measurement_calibration_provenance_t calibration;
} measurement_attempt_result_t;

typedef struct
{
    bool usable;
    measurement_return_channel_t selected;
    uint32_t reason_flags;
} measurement_ret_policy_result_t;

typedef struct
{
    measurement_quality_class_t measurement_quality;
    measurement_qualification_t qualification;
    measurement_confidence_class_t publication_confidence;
    uint32_t reason_flags;
    bool mathematically_valid;
} measurement_confidence_result_t;

typedef struct
{
    measurement_interpretation_t interpretation;
    uint32_t reason_flags;
    uint8_t supporting_attempts;
} measurement_session_classification_t;

typedef struct
{
    measurement_auto_status_t status;
    measurement_confidence_result_t confidence;
    measurement_session_classification_t classification;
    measurement_attempt_result_t primary_attempt;
    measurement_attempt_reason_t continuation_reason;
    uint8_t attempt_count;
    uint8_t primary_attempt_index;
    uint32_t session_sequence;
    measurement_auto_mode_t mode;
    bool partial;
    bool final;
} measurement_session_result_t;

typedef struct
{
    measurement_auto_mode_t mode;
    measurement_qualification_t qualification;
    uint32_t session_sequence;
    measurement_auto_hint_t hint;
} measurement_auto_start_t;

typedef struct
{
    measurement_auto_status_t status;
    measurement_auto_mode_t mode;
    measurement_qualification_t qualification;
    uint32_t session_sequence;
    measurement_auto_hint_t hint;
    measurement_attempt_config_t next_attempt;
    measurement_attempt_result_t history[MEASUREMENT_AUTO_MAX_ATTEMPTS];
    measurement_session_result_t last_result;
    uint64_t attempted_conditions;
    uint8_t attempt_count;
    uint8_t primary_attempt_index;
    uint8_t range_transition_count;
    uint8_t frequency_refinement_count;
    bool has_next_attempt;
    bool has_partial;
    bool has_final;
} measurement_auto_session_t;

void measurement_auto_session_init(measurement_auto_session_t *session);
bool measurement_auto_start_session(measurement_auto_session_t *session,
                                    const measurement_auto_start_t *start);
bool measurement_auto_next_attempt(const measurement_auto_session_t *session,
                                   measurement_attempt_config_t *attempt);
measurement_auto_event_t measurement_auto_submit_result(measurement_auto_session_t *session,
                                                        const measurement_attempt_result_t *result);
measurement_auto_event_t measurement_auto_cancel(measurement_auto_session_t *session);
measurement_auto_hint_t measurement_auto_make_hint(const measurement_session_result_t *result);

bool measurement_auto_condition_allowed(hw_range_id_t range_id,
                                        hw_excitation_freq_t frequency,
                                        hw_excitation_amp_t amplitude);
measurement_ret_evidence_t measurement_auto_evaluate_ret_evidence(
    const measurement_attempt_result_t *result);
measurement_ret_policy_result_t measurement_auto_select_ret_channel(
    const measurement_attempt_result_t *result);
measurement_confidence_result_t measurement_auto_evaluate_confidence(
    const measurement_attempt_result_t *result,
    measurement_qualification_t qualification);
measurement_session_classification_t measurement_auto_classify_session(
    const measurement_attempt_result_t *history,
    uint8_t count);

const measurement_session_result_t *measurement_auto_last_result(
    const measurement_auto_session_t *session);
const char *measurement_auto_status_string(measurement_auto_status_t status);
const char *measurement_confidence_string(measurement_confidence_class_t confidence);
const char *measurement_quality_string(measurement_quality_class_t quality);
const char *measurement_qualification_string(measurement_qualification_t qualification);
const char *measurement_attempt_reason_string(measurement_attempt_reason_t reason);
const char *measurement_ret_strategy_string(measurement_ret_strategy_t strategy);
uint32_t measurement_auto_session_size_bytes(void);

#endif
