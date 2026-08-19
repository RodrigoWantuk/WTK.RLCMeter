#include "hardware/hw_metrology_measure.h"

#include <stddef.h>

typedef enum
{
    HW_METROLOGY_MEASURE_ABORT_NONE = 0,
    HW_METROLOGY_MEASURE_ABORT_STOP_ADC,
    HW_METROLOGY_MEASURE_ABORT_EXC_OFF,
    HW_METROLOGY_MEASURE_ABORT_K1_SAFE,
    HW_METROLOGY_MEASURE_ABORT_RANGE_DISABLE,
    HW_METROLOGY_MEASURE_ABORT_RELEASE_GUARD,
    HW_METROLOGY_MEASURE_ABORT_ADC_RESTORE,
    HW_METROLOGY_MEASURE_ABORT_AUX_RESUME,
    HW_METROLOGY_MEASURE_ABORT_QUIET_OFF,
} hw_metrology_measure_abort_phase_t;

static bool g_k1_owned = false;
static bool g_k1_measure_active = false;

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (now_ms - deadline_ms) < 0x80000000u;
}

static uint32_t dma_timeout_ms(const hw_metrology_adc_profile_t *profile)
{
    const uint32_t capture_ms = (profile->capture_us + 999u) / 1000u;
    return capture_ms + HW_METROLOGY_MEASURE_DMA_TIMEOUT_MARGIN_MS;
}

static bsp_status_t call_status(bsp_status_t (*fn)(void *user), void *user)
{
    return (fn == NULL) ? BSP_STATUS_INVALID_ARG : fn(user);
}

static void sync_k1_globals(const hw_metrology_measure_t *measure)
{
    if ((measure != NULL) && hw_metrology_measure_active(measure))
    {
        g_k1_owned = measure->k1_owned;
        if (measure->io.k1_commanded_state != NULL)
        {
            g_k1_measure_active =
                measure->io.k1_commanded_state(measure->io.user) == HW_K1_STATE_MEASURE;
        }
        else
        {
            g_k1_measure_active = false;
        }
        return;
    }

    g_k1_owned = false;
    g_k1_measure_active = false;
}

static void latch_adc_restore_fault(hw_metrology_measure_t *measure)
{
    if (!measure->adc_restore_failed)
    {
        measure->adc_restore_failed = true;
        if (measure->io.latch_adc_runtime_fault != NULL)
        {
            measure->io.latch_adc_runtime_fault(measure->io.user);
        }
    }
}

static void note_adc_restore_result(hw_metrology_measure_t *measure, bsp_status_t restore_status)
{
    if (restore_status != BSP_STATUS_OK)
    {
        latch_adc_restore_fault(measure);
        if (measure->error == HW_METROLOGY_MEASURE_OK)
        {
            measure->error = HW_METROLOGY_MEASURE_ERR_AUX_RESTORE;
        }
    }
}

static void enter_abort(hw_metrology_measure_t *measure, hw_metrology_measure_error_t error)
{
    if (measure->error == HW_METROLOGY_MEASURE_OK)
    {
        measure->error = error;
    }
    measure->state = HW_METROLOGY_MEASURE_ABORT;
    measure->dumpable = false;
    measure->block.valid = false;
    measure->abort_release_guard_pending = measure->k1_reached_measure;
    measure->abort_phase = (uint8_t)HW_METROLOGY_MEASURE_ABORT_STOP_ADC;
}

static void finalize_block_validity(hw_metrology_measure_t *measure)
{
    measure->block.valid = measure->block.dma_complete && !measure->block.dma_error &&
                           !measure->block.timeout && !measure->adc_restore_failed;
    measure->dumpable = measure->block.valid;
}

static void prepare_block_metadata(hw_metrology_measure_t *measure)
{
    hw_metrology_block_t *block = &measure->block;
    block->valid = false;
    block->mode = HW_METROLOGY_MODE_DUT_MEASURE;
    block->dut_measure = true;
    block->sequence = measure->sequence;
    block->permit_issue_ms = 0u;
    block->permit_validate_ms = 0u;
    block->k1_operate_guard_ms = HW_METROLOGY_MEASURE_K1_OPERATE_GUARD_MS;
    block->k1_release_guard_ms = HW_METROLOGY_MEASURE_K1_RELEASE_GUARD_MS;
    block->excitation_frequency_hz = measure->exc_profile.frequency_hz;
    block->requested_amplitude_mvrms = hw_excitation_amplitude_mvrms(measure->request.amplitude);
    block->range_id = measure->request.range_id;
    block->charger = HW_CHARGER_UNKNOWN;
    if (measure->io.charger_state != NULL)
    {
        block->charger = measure->io.charger_state(measure->io.user);
    }
    block->adc_clock_hz = HW_METROLOGY_ADC_HZ;
    block->sample_time_cycles_x2 = HW_METROLOGY_ADC_SAMPLE_TIME_CYCLES_X2;
    block->sample_rate_hz = measure->adc_profile.sample_rate_hz;
    block->samples_per_cycle = measure->adc_profile.samples_per_cycle;
    block->cycles_per_block = measure->adc_profile.cycles_per_block;
    block->sample_count = (uint16_t)HW_METROLOGY_SAMPLES_PER_BLOCK;
    block->words_per_sample = (uint16_t)HW_METROLOGY_WORDS_PER_SAMPLE;
    block->raw_words = measure->raw_words;
    block->dma_complete = false;
    block->dma_error = false;
    block->timeout = false;
    block->clipped = false;
}

static bool io_complete(const hw_metrology_measure_io_t *io)
{
    return (io != NULL) &&
           (io->k1_force_safe != NULL) &&
           (io->k1_request_measure != NULL) &&
           (io->k1_commanded_state != NULL) &&
           (io->range_request != NULL) &&
           (io->range_step != NULL) &&
           (io->range_is_ready != NULL) &&
           (io->range_current_id != NULL) &&
           (io->range_safety_state != NULL) &&
           (io->range_force_disabled != NULL) &&
           (io->quiet_request != NULL) &&
           (io->aux_pause != NULL) &&
           (io->aux_resume != NULL) &&
           (io->adc_acquire != NULL) &&
           (io->adc_start_capture != NULL) &&
           (io->adc_stop != NULL) &&
           (io->adc_restore != NULL) &&
           (io->adc_dma_complete != NULL) &&
           (io->adc_dma_error != NULL) &&
           (io->excitation_off != NULL) &&
           (io->excitation_neutral != NULL) &&
           (io->excitation_sine != NULL) &&
           (io->excitation_mode != NULL) &&
           (io->excitation_dma_error != NULL) &&
           (io->safety_fault_mask != NULL) &&
           (io->permit_issue_input != NULL) &&
           (io->permit_validate_input != NULL);
}

static bool measure_dynamic_abort(hw_metrology_measure_t *measure)
{
    if ((measure->io.k1_commanded_state == NULL) ||
        (measure->io.k1_commanded_state(measure->io.user) != HW_K1_STATE_MEASURE))
    {
        return false;
    }

    if ((measure->io.safety_fault_mask != NULL) && (measure->io.safety_fault_mask(measure->io.user) != 0u))
    {
        return true;
    }

    if ((measure->io.charger_state != NULL) &&
        (measure->io.charger_state(measure->io.user) != HW_CHARGER_ABSENT))
    {
        return true;
    }

    if ((measure->io.range_safety_state == NULL) ||
        (measure->io.range_current_id == NULL) ||
        (measure->io.range_safety_state(measure->io.user) != HW_RANGE_READY) ||
        (measure->io.range_current_id(measure->io.user) != measure->request.range_id))
    {
        return true;
    }

    if ((measure->io.excitation_dma_error != NULL) && measure->io.excitation_dma_error(measure->io.user))
    {
        return true;
    }

    if ((measure->io.adc_dma_error != NULL) && measure->io.adc_dma_error(measure->io.user))
    {
        return true;
    }

    return false;
}

static hw_safety_result_t build_measure_permission(const hw_measure_permit_issue_input_t *issue_input)
{
    const hw_safety_input_t safety_input = {
        .charger = issue_input->charger,
        .residual = issue_input->residual,
        .battery = issue_input->battery,
        .range = issue_input->range,
        .application_fault = issue_input->safety_fault_mask != 0u,
    };
    return hw_safety_evaluate(&safety_input);
}

static void abort_cleanup_step(hw_metrology_measure_t *measure,
                               hw_metrology_measure_abort_phase_t *phase,
                               uint32_t now_ms)
{
    switch (*phase)
    {
    case HW_METROLOGY_MEASURE_ABORT_STOP_ADC:
        if (measure->io.adc_stop != NULL)
        {
            measure->io.adc_stop(measure->io.user);
        }
        *phase = HW_METROLOGY_MEASURE_ABORT_EXC_OFF;
        break;

    case HW_METROLOGY_MEASURE_ABORT_EXC_OFF:
    {
        const bsp_status_t exc_status = call_status(measure->io.excitation_off, measure->io.user);
        if (exc_status != BSP_STATUS_OK)
        {
            if (measure->io.latch_metrology_runtime_fault != NULL)
            {
                measure->io.latch_metrology_runtime_fault(measure->io.user);
            }
        }
        *phase = HW_METROLOGY_MEASURE_ABORT_K1_SAFE;
        break;
    }

    case HW_METROLOGY_MEASURE_ABORT_K1_SAFE:
    {
        const bsp_status_t k1_status = call_status(measure->io.k1_force_safe, measure->io.user);
        if (k1_status != BSP_STATUS_OK)
        {
            if (measure->io.latch_k1_io_fault != NULL)
            {
                measure->io.latch_k1_io_fault(measure->io.user);
            }
        }
        measure->k1_owned = measure->abort_release_guard_pending;
        *phase = HW_METROLOGY_MEASURE_ABORT_RANGE_DISABLE;
        break;
    }

    case HW_METROLOGY_MEASURE_ABORT_RANGE_DISABLE:
    {
        const bsp_status_t range_status =
            call_status(measure->io.range_force_disabled, measure->io.user);
        if (range_status != BSP_STATUS_OK)
        {
            if (measure->io.latch_range_io_fault != NULL)
            {
                measure->io.latch_range_io_fault(measure->io.user);
            }
        }
        if (measure->abort_release_guard_pending)
        {
            measure->wait_deadline_ms = now_ms + HW_METROLOGY_MEASURE_K1_RELEASE_GUARD_MS;
            *phase = HW_METROLOGY_MEASURE_ABORT_RELEASE_GUARD;
        }
        else
        {
            *phase = HW_METROLOGY_MEASURE_ABORT_ADC_RESTORE;
        }
        break;
    }

    case HW_METROLOGY_MEASURE_ABORT_RELEASE_GUARD:
        if (deadline_reached(now_ms, measure->wait_deadline_ms))
        {
            measure->k1_owned = false;
            *phase = HW_METROLOGY_MEASURE_ABORT_ADC_RESTORE;
        }
        break;

    case HW_METROLOGY_MEASURE_ABORT_ADC_RESTORE:
        if (measure->adc_owned || measure->aux_paused)
        {
            bsp_status_t restore = BSP_STATUS_ERROR;
            if (measure->io.adc_restore != NULL)
            {
                restore = measure->io.adc_restore(now_ms, measure->io.user);
            }
            note_adc_restore_result(measure, restore);
            measure->adc_owned = false;
            if ((restore == BSP_STATUS_OK) && measure->aux_paused)
            {
                *phase = HW_METROLOGY_MEASURE_ABORT_AUX_RESUME;
            }
            else
            {
                *phase = HW_METROLOGY_MEASURE_ABORT_QUIET_OFF;
            }
        }
        else
        {
            *phase = HW_METROLOGY_MEASURE_ABORT_QUIET_OFF;
        }
        break;

    case HW_METROLOGY_MEASURE_ABORT_AUX_RESUME:
        if (measure->aux_paused && (measure->io.aux_resume != NULL))
        {
            measure->io.aux_resume(now_ms, measure->io.user);
            measure->aux_paused = false;
        }
        *phase = HW_METROLOGY_MEASURE_ABORT_QUIET_OFF;
        break;

    case HW_METROLOGY_MEASURE_ABORT_QUIET_OFF:
        if (measure->quiet_held && (measure->io.quiet_request != NULL))
        {
            measure->io.quiet_request(false, measure->io.user);
            measure->quiet_held = false;
        }
        measure->dumpable = false;
        measure->block.valid = false;
        measure->state = HW_METROLOGY_MEASURE_IDLE;
        *phase = HW_METROLOGY_MEASURE_ABORT_NONE;
        break;

    case HW_METROLOGY_MEASURE_ABORT_NONE:
    default:
        break;
    }
}

bsp_status_t hw_metrology_measure_init(hw_metrology_measure_t *measure,
                                       const hw_metrology_measure_io_t *io,
                                       uint32_t *raw_words,
                                       uint32_t raw_word_count)
{
    if ((measure == NULL) || !io_complete(io) || (raw_words == NULL) ||
        (raw_word_count != HW_METROLOGY_RAW_WORD_COUNT))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    measure->io = *io;
    measure->state = HW_METROLOGY_MEASURE_IDLE;
    measure->error = HW_METROLOGY_MEASURE_OK;
    measure->raw_words = raw_words;
    measure->raw_word_count = raw_word_count;
    measure->wait_deadline_ms = 0u;
    measure->sequence = 0u;
    measure->quiet_held = false;
    measure->aux_paused = false;
    measure->adc_owned = false;
    measure->dumpable = false;
    measure->adc_restore_failed = false;
    measure->permit_validated = false;
    measure->k1_owned = false;
    measure->k1_reached_measure = false;
    measure->abort_release_guard_pending = false;
    measure->abort_phase = (uint8_t)HW_METROLOGY_MEASURE_ABORT_NONE;
    measure->block.valid = false;
    hw_measure_permit_init(&measure->permit);
    sync_k1_globals(measure);
    return BSP_STATUS_OK;
}

bsp_status_t hw_metrology_measure_start(hw_metrology_measure_t *measure,
                                        const hw_metrology_measure_request_t *request,
                                        uint32_t now_ms)
{
    (void)now_ms;
    if ((measure == NULL) || (request == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (hw_metrology_measure_active(measure))
    {
        return BSP_STATUS_BUSY;
    }

    measure->dumpable = false;
    measure->adc_restore_failed = false;
    measure->error = HW_METROLOGY_MEASURE_OK;
    measure->block.valid = false;
    measure->permit_validated = false;
    measure->k1_owned = false;
    measure->k1_reached_measure = false;
    measure->abort_release_guard_pending = false;
    measure->abort_phase = (uint8_t)HW_METROLOGY_MEASURE_ABORT_NONE;
    hw_measure_permit_init(&measure->permit);

    if (!hw_metrology_clock_ready(request->clock_summary, request->clock_init_status))
    {
        measure->error = HW_METROLOGY_MEASURE_ERR_CLOCK;
        measure->state = HW_METROLOGY_MEASURE_IDLE;
        (void)call_status(measure->io.k1_force_safe, measure->io.user);
        (void)call_status(measure->io.excitation_off, measure->io.user);
        sync_k1_globals(measure);
        return BSP_STATUS_ERROR;
    }

    const bsp_status_t amplitude_status =
        hw_excitation_validate_amplitude(request->range_id, request->amplitude);
    if (amplitude_status == BSP_STATUS_NOT_SUPPORTED)
    {
        measure->error = HW_METROLOGY_MEASURE_ERR_FORBIDDEN_AMPLITUDE;
        (void)call_status(measure->io.k1_force_safe, measure->io.user);
        (void)call_status(measure->io.excitation_off, measure->io.user);
        sync_k1_globals(measure);
        return BSP_STATUS_NOT_SUPPORTED;
    }
    if ((amplitude_status != BSP_STATUS_OK) ||
        (hw_excitation_freq_profile(request->frequency, &measure->exc_profile) != BSP_STATUS_OK) ||
        (hw_metrology_adc_profile(request->frequency, &measure->adc_profile) != BSP_STATUS_OK))
    {
        measure->error = HW_METROLOGY_MEASURE_ERR_INVALID;
        sync_k1_globals(measure);
        return BSP_STATUS_INVALID_ARG;
    }

    measure->request = *request;
    measure->sequence++;
    prepare_block_metadata(measure);
    measure->state = HW_METROLOGY_MEASURE_FORCE_K1_SAFE;
    sync_k1_globals(measure);
    return BSP_STATUS_BUSY;
}

static bsp_status_t step_success_path(hw_metrology_measure_t *measure, uint32_t now_ms)
{
    if (measure_dynamic_abort(measure))
    {
        enter_abort(measure, HW_METROLOGY_MEASURE_ERR_ABORT);
        return BSP_STATUS_BUSY;
    }

    switch (measure->state)
    {
    case HW_METROLOGY_MEASURE_FORCE_K1_SAFE:
        if (call_status(measure->io.k1_force_safe, measure->io.user) != BSP_STATUS_OK)
        {
            if (measure->io.latch_k1_io_fault != NULL)
            {
                measure->io.latch_k1_io_fault(measure->io.user);
            }
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_K1);
            break;
        }
        measure->state = HW_METROLOGY_MEASURE_RANGE_REQUEST;
        break;

    case HW_METROLOGY_MEASURE_RANGE_REQUEST:
    {
        const bsp_status_t status =
            measure->io.range_request(measure->request.range_id, now_ms, measure->io.user);
        if ((status != BSP_STATUS_OK) && (status != BSP_STATUS_BUSY))
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_RANGE);
            break;
        }
        measure->wait_deadline_ms = now_ms + HW_METROLOGY_MEASURE_RANGE_TIMEOUT_MS;
        measure->state = HW_METROLOGY_MEASURE_RANGE_WAIT;
        break;
    }

    case HW_METROLOGY_MEASURE_RANGE_WAIT:
    {
        const bsp_status_t status = measure->io.range_step(now_ms, measure->io.user);
        if ((status != BSP_STATUS_OK) && (status != BSP_STATUS_BUSY))
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_RANGE);
            break;
        }
        if (measure->io.range_is_ready(measure->io.user))
        {
            measure->state = HW_METROLOGY_MEASURE_EXC_NEUTRAL;
            break;
        }
        if (deadline_reached(now_ms, measure->wait_deadline_ms))
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_RANGE);
        }
        break;
    }

    case HW_METROLOGY_MEASURE_EXC_NEUTRAL:
        if (measure->io.excitation_neutral(measure->io.user) != BSP_STATUS_OK)
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_EXCITATION);
            break;
        }
        measure->wait_deadline_ms = now_ms + HW_EXCITATION_NEUTRAL_SETTLE_MS;
        measure->state = HW_METROLOGY_MEASURE_EXC_NEUTRAL_WAIT;
        break;

    case HW_METROLOGY_MEASURE_EXC_NEUTRAL_WAIT:
        if (deadline_reached(now_ms, measure->wait_deadline_ms))
        {
            measure->state = HW_METROLOGY_MEASURE_QUIET_ENTER;
        }
        break;

    case HW_METROLOGY_MEASURE_QUIET_ENTER:
        measure->io.quiet_request(true, measure->io.user);
        measure->quiet_held = true;
        measure->state = HW_METROLOGY_MEASURE_PERMIT_ISSUE;
        break;

    case HW_METROLOGY_MEASURE_PERMIT_ISSUE:
    {
        hw_measure_permit_issue_input_t issue_input;
        if (measure->io.permit_issue_input(&issue_input, measure->io.user) != BSP_STATUS_OK)
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_PERMIT);
            break;
        }
        const hw_measure_permit_issue_result_t issue_result =
            hw_measure_permit_issue(&measure->permit, &issue_input, now_ms);
        if (!issue_result.issued)
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_PERMIT);
            break;
        }
        measure->block.permit_issue_ms = now_ms;
        measure->state = HW_METROLOGY_MEASURE_PERMIT_VALIDATE;
        break;
    }

    case HW_METROLOGY_MEASURE_PERMIT_VALIDATE:
    {
        hw_measure_permit_validate_input_t validate_input;
        if (measure->io.permit_validate_input(&validate_input, measure->io.user) != BSP_STATUS_OK)
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_PERMIT);
            break;
        }
        const hw_measure_permit_validate_result_t validate_result =
            hw_measure_permit_validate(&measure->permit, &validate_input, now_ms);
        if (!validate_result.allowed)
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_PERMIT);
            break;
        }
        measure->permit_validated = true;
        measure->block.permit_validate_ms = now_ms;
        measure->state = HW_METROLOGY_MEASURE_AUX_PAUSE;
        break;
    }

    case HW_METROLOGY_MEASURE_AUX_PAUSE:
        measure->io.aux_pause(measure->io.user);
        measure->aux_paused = true;
        measure->state = HW_METROLOGY_MEASURE_K1_REQUEST;
        break;

    case HW_METROLOGY_MEASURE_K1_REQUEST:
    {
        if (!measure->permit_validated)
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_PERMIT);
            break;
        }
        hw_measure_permit_issue_input_t issue_input;
        if (measure->io.permit_issue_input(&issue_input, measure->io.user) != BSP_STATUS_OK)
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_PERMIT);
            break;
        }
        const hw_safety_result_t permission = build_measure_permission(&issue_input);
        const bsp_status_t k1_status =
            measure->io.k1_request_measure(&permission, measure->io.user);
        if ((k1_status != BSP_STATUS_OK) ||
            (measure->io.k1_commanded_state(measure->io.user) != HW_K1_STATE_MEASURE))
        {
            if (measure->io.latch_k1_io_fault != NULL)
            {
                measure->io.latch_k1_io_fault(measure->io.user);
            }
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_K1);
            break;
        }
        measure->k1_owned = true;
        measure->k1_reached_measure = true;
        measure->wait_deadline_ms = now_ms + HW_METROLOGY_MEASURE_K1_OPERATE_GUARD_MS;
        measure->state = HW_METROLOGY_MEASURE_K1_OPERATE_GUARD;
        break;
    }

    case HW_METROLOGY_MEASURE_K1_OPERATE_GUARD:
        if (deadline_reached(now_ms, measure->wait_deadline_ms))
        {
            measure->state = HW_METROLOGY_MEASURE_ADC_ACQUIRE;
        }
        break;

    case HW_METROLOGY_MEASURE_ADC_ACQUIRE:
        measure->adc_owned = true;
        if (measure->io.adc_acquire(now_ms, measure->io.user) != BSP_STATUS_OK)
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_ADC_ACQUIRE);
            break;
        }
        measure->state = HW_METROLOGY_MEASURE_EXC_SINE_START;
        break;

    case HW_METROLOGY_MEASURE_EXC_SINE_START:
        if (measure->io.excitation_sine(measure->request.frequency,
                                        measure->request.amplitude,
                                        measure->io.user) != BSP_STATUS_OK)
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_EXCITATION);
            break;
        }
        if (measure->io.excitation_mode(measure->io.user) != HW_EXCITATION_MODE_SINE)
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_EXCITATION);
            break;
        }
        measure->wait_deadline_ms = now_ms + measure->exc_profile.sine_settle_ms;
        measure->state = HW_METROLOGY_MEASURE_EXC_SETTLE;
        break;

    case HW_METROLOGY_MEASURE_EXC_SETTLE:
        if (measure->io.excitation_dma_error(measure->io.user))
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_EXCITATION);
            break;
        }
        if (deadline_reached(now_ms, measure->wait_deadline_ms))
        {
            measure->state = HW_METROLOGY_MEASURE_ADC_DMA_START;
        }
        break;

    case HW_METROLOGY_MEASURE_ADC_DMA_START:
        if (measure->io.adc_start_capture(measure->raw_words,
                                          measure->raw_word_count,
                                          &measure->adc_profile,
                                          measure->io.user) != BSP_STATUS_OK)
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_ADC_ACQUIRE);
            break;
        }
        measure->wait_deadline_ms = now_ms + dma_timeout_ms(&measure->adc_profile);
        measure->state = HW_METROLOGY_MEASURE_ADC_DMA_WAIT;
        break;

    case HW_METROLOGY_MEASURE_ADC_DMA_WAIT:
        if (measure->io.adc_dma_error(measure->io.user) ||
            measure->io.excitation_dma_error(measure->io.user))
        {
            measure->block.dma_error = true;
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_DMA);
            break;
        }
        if (measure->io.adc_dma_complete(measure->io.user))
        {
            measure->block.dma_complete = true;
            measure->state = HW_METROLOGY_MEASURE_ADC_DMA_COMPLETE;
            break;
        }
        if (deadline_reached(now_ms, measure->wait_deadline_ms))
        {
            measure->block.timeout = true;
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_TIMEOUT);
        }
        break;

    case HW_METROLOGY_MEASURE_ADC_DMA_COMPLETE:
        if (measure->io.adc_stop != NULL)
        {
            measure->io.adc_stop(measure->io.user);
        }
        measure->state = HW_METROLOGY_MEASURE_EXC_NEUTRAL_POST;
        break;

    case HW_METROLOGY_MEASURE_EXC_NEUTRAL_POST:
        if (measure->io.excitation_neutral(measure->io.user) != BSP_STATUS_OK)
        {
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_EXCITATION);
            break;
        }
        measure->wait_deadline_ms = now_ms + HW_EXCITATION_NEUTRAL_SETTLE_MS;
        measure->state = HW_METROLOGY_MEASURE_EXC_NEUTRAL_POST_WAIT;
        break;

    case HW_METROLOGY_MEASURE_EXC_NEUTRAL_POST_WAIT:
        if (deadline_reached(now_ms, measure->wait_deadline_ms))
        {
            measure->state = HW_METROLOGY_MEASURE_K1_SAFE;
        }
        break;

    case HW_METROLOGY_MEASURE_K1_SAFE:
    {
        const bsp_status_t k1_status = call_status(measure->io.k1_force_safe, measure->io.user);
        if (k1_status != BSP_STATUS_OK)
        {
            if (measure->io.latch_k1_io_fault != NULL)
            {
                measure->io.latch_k1_io_fault(measure->io.user);
            }
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_K1);
            break;
        }
        measure->wait_deadline_ms = now_ms + HW_METROLOGY_MEASURE_K1_RELEASE_GUARD_MS;
        measure->state = HW_METROLOGY_MEASURE_K1_RELEASE_GUARD;
        break;
    }

    case HW_METROLOGY_MEASURE_K1_RELEASE_GUARD:
        if (deadline_reached(now_ms, measure->wait_deadline_ms))
        {
            measure->k1_owned = false;
            measure->state = HW_METROLOGY_MEASURE_EXC_OFF;
        }
        break;

    case HW_METROLOGY_MEASURE_EXC_OFF:
        if (measure->io.excitation_off(measure->io.user) != BSP_STATUS_OK)
        {
            if (measure->io.latch_metrology_runtime_fault != NULL)
            {
                measure->io.latch_metrology_runtime_fault(measure->io.user);
            }
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_EXCITATION);
            break;
        }
        measure->state = HW_METROLOGY_MEASURE_RANGE_DISABLE;
        break;

    case HW_METROLOGY_MEASURE_RANGE_DISABLE:
        if (measure->io.range_force_disabled(measure->io.user) != BSP_STATUS_OK)
        {
            if (measure->io.latch_range_io_fault != NULL)
            {
                measure->io.latch_range_io_fault(measure->io.user);
            }
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_RANGE);
            break;
        }
        measure->state = HW_METROLOGY_MEASURE_ADC_RELEASE;
        break;

    case HW_METROLOGY_MEASURE_ADC_RELEASE:
        if (measure->io.adc_stop != NULL)
        {
            measure->io.adc_stop(measure->io.user);
        }
        measure->state = HW_METROLOGY_MEASURE_ADC_RESTORE;
        break;

    case HW_METROLOGY_MEASURE_ADC_RESTORE:
    {
        const bsp_status_t restore = measure->io.adc_restore(now_ms, measure->io.user);
        if (restore != BSP_STATUS_OK)
        {
            note_adc_restore_result(measure, restore);
            enter_abort(measure, HW_METROLOGY_MEASURE_ERR_AUX_RESTORE);
            break;
        }
        measure->adc_owned = false;
        measure->state = HW_METROLOGY_MEASURE_AUX_RESUME;
        break;
    }

    case HW_METROLOGY_MEASURE_AUX_RESUME:
        measure->io.aux_resume(now_ms, measure->io.user);
        measure->aux_paused = false;
        measure->state = HW_METROLOGY_MEASURE_QUIET_EXIT;
        break;

    case HW_METROLOGY_MEASURE_QUIET_EXIT:
        measure->io.quiet_request(false, measure->io.user);
        measure->quiet_held = false;
        measure->state = HW_METROLOGY_MEASURE_ANALYZE;
        break;

    case HW_METROLOGY_MEASURE_ANALYZE:
        hw_metrology_analyze_block(measure->raw_words, HW_METROLOGY_SAMPLES_PER_BLOCK, &measure->block);
        finalize_block_validity(measure);
        measure->state = HW_METROLOGY_MEASURE_DONE;
        break;

    case HW_METROLOGY_MEASURE_DONE:
    case HW_METROLOGY_MEASURE_IDLE:
    case HW_METROLOGY_MEASURE_ABORT:
        break;

    default:
        enter_abort(measure, HW_METROLOGY_MEASURE_ERR_INVALID);
        break;
    }

    return BSP_STATUS_BUSY;
}

bsp_status_t hw_metrology_measure_step(hw_metrology_measure_t *measure, uint32_t now_ms)
{
    if (measure == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    if ((measure->state == HW_METROLOGY_MEASURE_IDLE) ||
        (measure->state == HW_METROLOGY_MEASURE_DONE))
    {
        sync_k1_globals(measure);
        return BSP_STATUS_OK;
    }

    if (measure->state == HW_METROLOGY_MEASURE_ABORT)
    {
        hw_metrology_measure_abort_phase_t phase =
            (hw_metrology_measure_abort_phase_t)measure->abort_phase;
        abort_cleanup_step(measure, &phase, now_ms);
        measure->abort_phase = (uint8_t)phase;
        sync_k1_globals(measure);
        return BSP_STATUS_ERROR;
    }

    const bsp_status_t status = step_success_path(measure, now_ms);
    if (measure->state == HW_METROLOGY_MEASURE_ABORT)
    {
        sync_k1_globals(measure);
        return BSP_STATUS_ERROR;
    }
    if (measure->state == HW_METROLOGY_MEASURE_DONE)
    {
        sync_k1_globals(measure);
        return BSP_STATUS_OK;
    }
    sync_k1_globals(measure);
    return status;
}

hw_metrology_measure_state_t hw_metrology_measure_state(const hw_metrology_measure_t *measure)
{
    return (measure == NULL) ? HW_METROLOGY_MEASURE_IDLE : measure->state;
}

hw_metrology_measure_error_t hw_metrology_measure_error(const hw_metrology_measure_t *measure)
{
    return (measure == NULL) ? HW_METROLOGY_MEASURE_ERR_INVALID : measure->error;
}

bool hw_metrology_measure_active(const hw_metrology_measure_t *measure)
{
    if (measure == NULL)
    {
        return false;
    }
    return (measure->state != HW_METROLOGY_MEASURE_IDLE) &&
           (measure->state != HW_METROLOGY_MEASURE_DONE);
}

bool hw_metrology_measure_dumpable(const hw_metrology_measure_t *measure)
{
    return (measure != NULL) && measure->dumpable && measure->block.valid;
}

bool hw_metrology_measure_k1_owned(void)
{
    return g_k1_owned;
}

bool hw_metrology_measure_k1_measure_active(void)
{
    return g_k1_measure_active;
}

const hw_metrology_block_t *hw_metrology_measure_block(const hw_metrology_measure_t *measure)
{
    return (measure == NULL) ? NULL : &measure->block;
}

void hw_metrology_measure_acknowledge(hw_metrology_measure_t *measure)
{
    if (measure == NULL)
    {
        return;
    }
    measure->state = HW_METROLOGY_MEASURE_IDLE;
    measure->dumpable = false;
    measure->k1_owned = false;
    sync_k1_globals(measure);
}

const char *hw_metrology_measure_state_string(hw_metrology_measure_state_t state)
{
    switch (state)
    {
    case HW_METROLOGY_MEASURE_IDLE:
        return "IDLE";
    case HW_METROLOGY_MEASURE_FORCE_K1_SAFE:
        return "FORCE_K1_SAFE";
    case HW_METROLOGY_MEASURE_RANGE_REQUEST:
        return "RANGE_REQUEST";
    case HW_METROLOGY_MEASURE_RANGE_WAIT:
        return "RANGE_WAIT";
    case HW_METROLOGY_MEASURE_EXC_NEUTRAL:
        return "EXC_NEUTRAL";
    case HW_METROLOGY_MEASURE_EXC_NEUTRAL_WAIT:
        return "EXC_NEUTRAL_WAIT";
    case HW_METROLOGY_MEASURE_QUIET_ENTER:
        return "QUIET_ENTER";
    case HW_METROLOGY_MEASURE_PERMIT_ISSUE:
        return "PERMIT_ISSUE";
    case HW_METROLOGY_MEASURE_PERMIT_VALIDATE:
        return "PERMIT_VALIDATE";
    case HW_METROLOGY_MEASURE_AUX_PAUSE:
        return "AUX_PAUSE";
    case HW_METROLOGY_MEASURE_K1_REQUEST:
        return "K1_REQUEST";
    case HW_METROLOGY_MEASURE_K1_OPERATE_GUARD:
        return "K1_OPERATE_GUARD";
    case HW_METROLOGY_MEASURE_ADC_ACQUIRE:
        return "ADC_ACQUIRE";
    case HW_METROLOGY_MEASURE_EXC_SINE_START:
        return "EXC_SINE_START";
    case HW_METROLOGY_MEASURE_EXC_SETTLE:
        return "EXC_SETTLE";
    case HW_METROLOGY_MEASURE_ADC_DMA_START:
        return "ADC_DMA_START";
    case HW_METROLOGY_MEASURE_ADC_DMA_WAIT:
        return "ADC_DMA_WAIT";
    case HW_METROLOGY_MEASURE_ADC_DMA_COMPLETE:
        return "ADC_DMA_COMPLETE";
    case HW_METROLOGY_MEASURE_EXC_NEUTRAL_POST:
        return "EXC_NEUTRAL_POST";
    case HW_METROLOGY_MEASURE_EXC_NEUTRAL_POST_WAIT:
        return "EXC_NEUTRAL_POST_WAIT";
    case HW_METROLOGY_MEASURE_K1_SAFE:
        return "K1_SAFE";
    case HW_METROLOGY_MEASURE_K1_RELEASE_GUARD:
        return "K1_RELEASE_GUARD";
    case HW_METROLOGY_MEASURE_EXC_OFF:
        return "EXC_OFF";
    case HW_METROLOGY_MEASURE_RANGE_DISABLE:
        return "RANGE_DISABLE";
    case HW_METROLOGY_MEASURE_ADC_RELEASE:
        return "ADC_RELEASE";
    case HW_METROLOGY_MEASURE_ADC_RESTORE:
        return "ADC_RESTORE";
    case HW_METROLOGY_MEASURE_AUX_RESUME:
        return "AUX_RESUME";
    case HW_METROLOGY_MEASURE_QUIET_EXIT:
        return "QUIET_EXIT";
    case HW_METROLOGY_MEASURE_ANALYZE:
        return "ANALYZE";
    case HW_METROLOGY_MEASURE_DONE:
        return "DONE";
    case HW_METROLOGY_MEASURE_ABORT:
        return "ABORT";
    default:
        return "UNKNOWN";
    }
}

const char *hw_metrology_measure_error_string(hw_metrology_measure_error_t error)
{
    switch (error)
    {
    case HW_METROLOGY_MEASURE_OK:
        return "OK";
    case HW_METROLOGY_MEASURE_ERR_CLOCK:
        return "CLOCK";
    case HW_METROLOGY_MEASURE_ERR_BUSY:
        return "BUSY";
    case HW_METROLOGY_MEASURE_ERR_INVALID:
        return "INVALID";
    case HW_METROLOGY_MEASURE_ERR_FORBIDDEN_AMPLITUDE:
        return "FORBIDDEN_AMPLITUDE";
    case HW_METROLOGY_MEASURE_ERR_RANGE:
        return "RANGE";
    case HW_METROLOGY_MEASURE_ERR_PERMIT:
        return "PERMIT";
    case HW_METROLOGY_MEASURE_ERR_K1:
        return "K1";
    case HW_METROLOGY_MEASURE_ERR_ADC_ACQUIRE:
        return "ADC_ACQUIRE";
    case HW_METROLOGY_MEASURE_ERR_EXCITATION:
        return "EXCITATION";
    case HW_METROLOGY_MEASURE_ERR_DMA:
        return "DMA";
    case HW_METROLOGY_MEASURE_ERR_TIMEOUT:
        return "TIMEOUT";
    case HW_METROLOGY_MEASURE_ERR_AUX_RESTORE:
        return "AUX_RESTORE";
    case HW_METROLOGY_MEASURE_ERR_ABORT:
        return "ABORT";
    default:
        return "UNKNOWN";
    }
}
