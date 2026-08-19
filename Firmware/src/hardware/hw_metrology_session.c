#include "hardware/hw_metrology_session.h"

#include <stddef.h>

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (now_ms - deadline_ms) < 0x80000000u;
}

static uint32_t dma_timeout_ms(const hw_metrology_adc_profile_t *profile)
{
    const uint32_t capture_ms = (profile->capture_us + 999u) / 1000u;
    return capture_ms + HW_METROLOGY_SESSION_DMA_TIMEOUT_MARGIN_MS;
}

static void note_k1(hw_metrology_session_t *session)
{
    if ((session->io.k1_commanded_state != NULL) &&
        (session->io.k1_commanded_state(session->io.user) != HW_K1_STATE_SAFE))
    {
        session->k1_left_safe = true;
    }
}

static bsp_status_t call_status(bsp_status_t (*fn)(void *user), void *user)
{
    return (fn == NULL) ? BSP_STATUS_INVALID_ARG : fn(user);
}

static void enter_abort(hw_metrology_session_t *session, hw_metrology_session_error_t error)
{
    session->error = error;
    session->state = HW_METROLOGY_SESSION_ABORT;
    session->dumpable = false;
    session->block.valid = false;
}

static void fail_safe_shutdown(hw_metrology_session_t *session, uint32_t now_ms)
{
    if (session->io.adc_stop != NULL)
    {
        session->io.adc_stop(session->io.user);
    }
    (void)call_status(session->io.excitation_off, session->io.user);
    (void)call_status(session->io.k1_force_safe, session->io.user);
    note_k1(session);
    (void)call_status(session->io.range_force_disabled, session->io.user);

    if (session->adc_owned || session->aux_paused)
    {
        bsp_status_t restore = BSP_STATUS_ERROR;
        if (session->io.adc_restore != NULL)
        {
            restore = session->io.adc_restore(now_ms, session->io.user);
        }
        if (restore != BSP_STATUS_OK)
        {
            session->error = HW_METROLOGY_SESSION_ERR_AUX_RESTORE;
            if (session->io.latch_adc_runtime_fault != NULL)
            {
                session->io.latch_adc_runtime_fault(session->io.user);
            }
            session->adc_owned = false;
            /* Leave auxiliary paused; residual remains UNKNOWN. */
        }
        else
        {
            session->adc_owned = false;
            if (session->aux_paused && (session->io.aux_resume != NULL))
            {
                session->io.aux_resume(now_ms, session->io.user);
            }
            session->aux_paused = false;
        }
    }

    if (session->quiet_held && (session->io.quiet_request != NULL))
    {
        session->io.quiet_request(false, session->io.user);
        session->quiet_held = false;
    }

    note_k1(session);
}

static void prepare_block_metadata(hw_metrology_session_t *session)
{
    hw_metrology_block_t *block = &session->block;
    block->valid = false;
    block->sequence = session->sequence;
    block->excitation_frequency_hz = session->exc_profile.frequency_hz;
    block->requested_amplitude_mvrms = hw_excitation_amplitude_mvrms(session->request.amplitude);
    block->range_id = session->request.range_id;
    block->charger = HW_CHARGER_UNKNOWN;
    if (session->io.charger_state != NULL)
    {
        block->charger = session->io.charger_state(session->io.user);
    }
    block->adc_clock_hz = HW_METROLOGY_ADC_HZ;
    block->sample_time_cycles_x2 = HW_METROLOGY_ADC_SAMPLE_TIME_CYCLES_X2;
    block->sample_rate_hz = session->adc_profile.sample_rate_hz;
    block->samples_per_cycle = session->adc_profile.samples_per_cycle;
    block->cycles_per_block = session->adc_profile.cycles_per_block;
    block->sample_count = (uint16_t)HW_METROLOGY_SAMPLES_PER_BLOCK;
    block->words_per_sample = (uint16_t)HW_METROLOGY_WORDS_PER_SAMPLE;
    block->raw_words = session->raw_words;
    block->dma_complete = false;
    block->dma_error = false;
    block->timeout = false;
    block->clipped = false;
}

static bool io_complete(const hw_metrology_session_io_t *io)
{
    return (io != NULL) &&
           (io->k1_force_safe != NULL) &&
           (io->k1_commanded_state != NULL) &&
           (io->range_request != NULL) &&
           (io->range_step != NULL) &&
           (io->range_is_ready != NULL) &&
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
           (io->excitation_dma_error != NULL);
}

bsp_status_t hw_metrology_session_init(hw_metrology_session_t *session,
                                       const hw_metrology_session_io_t *io,
                                       uint32_t *raw_words,
                                       uint32_t raw_word_count)
{
    if ((session == NULL) || !io_complete(io) || (raw_words == NULL) ||
        (raw_word_count != HW_METROLOGY_RAW_WORD_COUNT))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    session->io = *io;
    session->state = HW_METROLOGY_SESSION_IDLE;
    session->error = HW_METROLOGY_SESSION_OK;
    session->raw_words = raw_words;
    session->raw_word_count = raw_word_count;
    session->wait_deadline_ms = 0u;
    session->sequence = 0u;
    session->quiet_held = false;
    session->aux_paused = false;
    session->adc_owned = false;
    session->k1_left_safe = false;
    session->dumpable = false;
    session->block.valid = false;
    return BSP_STATUS_OK;
}

bsp_status_t hw_metrology_session_start(hw_metrology_session_t *session,
                                        const hw_metrology_session_request_t *request,
                                        uint32_t now_ms)
{
    (void)now_ms;
    if ((session == NULL) || (request == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (hw_metrology_session_active(session))
    {
        return BSP_STATUS_BUSY;
    }

    session->k1_left_safe = false;
    session->dumpable = false;
    session->error = HW_METROLOGY_SESSION_OK;
    session->block.valid = false;

    if (!hw_metrology_clock_ready(request->clock_summary, request->clock_init_status))
    {
        session->error = HW_METROLOGY_SESSION_ERR_CLOCK;
        session->state = HW_METROLOGY_SESSION_IDLE;
        (void)call_status(session->io.k1_force_safe, session->io.user);
        (void)call_status(session->io.excitation_off, session->io.user);
        note_k1(session);
        return BSP_STATUS_ERROR;
    }

    const bsp_status_t amplitude_status =
        hw_excitation_validate_amplitude(request->range_id, request->amplitude);
    if (amplitude_status == BSP_STATUS_NOT_SUPPORTED)
    {
        session->error = HW_METROLOGY_SESSION_ERR_FORBIDDEN_AMPLITUDE;
        (void)call_status(session->io.k1_force_safe, session->io.user);
        (void)call_status(session->io.excitation_off, session->io.user);
        note_k1(session);
        return BSP_STATUS_NOT_SUPPORTED;
    }
    if ((amplitude_status != BSP_STATUS_OK) ||
        (hw_excitation_freq_profile(request->frequency, &session->exc_profile) != BSP_STATUS_OK) ||
        (hw_metrology_adc_profile(request->frequency, &session->adc_profile) != BSP_STATUS_OK))
    {
        session->error = HW_METROLOGY_SESSION_ERR_INVALID;
        return BSP_STATUS_INVALID_ARG;
    }

    session->request = *request;
    session->sequence++;
    prepare_block_metadata(session);
    session->state = HW_METROLOGY_SESSION_FORCE_K1_SAFE;
    return BSP_STATUS_BUSY;
}

static bsp_status_t step_success_path(hw_metrology_session_t *session, uint32_t now_ms)
{
    switch (session->state)
    {
    case HW_METROLOGY_SESSION_FORCE_K1_SAFE:
        if (call_status(session->io.k1_force_safe, session->io.user) != BSP_STATUS_OK)
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_INVALID);
            break;
        }
        note_k1(session);
        session->state = HW_METROLOGY_SESSION_RANGE_REQUEST;
        break;

    case HW_METROLOGY_SESSION_RANGE_REQUEST:
    {
        const bsp_status_t status =
            session->io.range_request(session->request.range_id, now_ms, session->io.user);
        if ((status != BSP_STATUS_OK) && (status != BSP_STATUS_BUSY))
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_RANGE);
            break;
        }
        session->wait_deadline_ms = now_ms + HW_METROLOGY_SESSION_RANGE_TIMEOUT_MS;
        session->state = HW_METROLOGY_SESSION_RANGE_WAIT;
        break;
    }

    case HW_METROLOGY_SESSION_RANGE_WAIT:
    {
        const bsp_status_t status = session->io.range_step(now_ms, session->io.user);
        if ((status != BSP_STATUS_OK) && (status != BSP_STATUS_BUSY))
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_RANGE);
            break;
        }
        if (session->io.range_is_ready(session->io.user))
        {
            session->state = HW_METROLOGY_SESSION_QUIET_ENTER;
            break;
        }
        if (deadline_reached(now_ms, session->wait_deadline_ms))
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_RANGE);
        }
        break;
    }

    case HW_METROLOGY_SESSION_QUIET_ENTER:
        session->io.quiet_request(true, session->io.user);
        session->quiet_held = true;
        session->state = HW_METROLOGY_SESSION_AUX_PAUSE;
        break;

    case HW_METROLOGY_SESSION_AUX_PAUSE:
        session->io.aux_pause(session->io.user);
        session->aux_paused = true;
        session->state = HW_METROLOGY_SESSION_ADC_ACQUIRE;
        break;

    case HW_METROLOGY_SESSION_ADC_ACQUIRE:
        session->adc_owned = true;
        if (session->io.adc_acquire(now_ms, session->io.user) != BSP_STATUS_OK)
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_ADC_ACQUIRE);
            break;
        }
        session->state = HW_METROLOGY_SESSION_EXC_NEUTRAL;
        break;

    case HW_METROLOGY_SESSION_EXC_NEUTRAL:
        if (session->io.excitation_neutral(session->io.user) != BSP_STATUS_OK)
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_EXCITATION);
            break;
        }
        session->wait_deadline_ms = now_ms + HW_EXCITATION_NEUTRAL_SETTLE_MS;
        session->state = HW_METROLOGY_SESSION_EXC_NEUTRAL_WAIT;
        break;

    case HW_METROLOGY_SESSION_EXC_NEUTRAL_WAIT:
        if (deadline_reached(now_ms, session->wait_deadline_ms))
        {
            session->state = HW_METROLOGY_SESSION_EXC_SINE_START;
        }
        break;

    case HW_METROLOGY_SESSION_EXC_SINE_START:
        if (session->io.excitation_sine(session->request.frequency,
                                        session->request.amplitude,
                                        session->io.user) != BSP_STATUS_OK)
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_EXCITATION);
            break;
        }
        if (session->io.excitation_mode(session->io.user) != HW_EXCITATION_MODE_SINE)
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_EXCITATION);
            break;
        }
        session->wait_deadline_ms = now_ms + session->exc_profile.sine_settle_ms;
        session->state = HW_METROLOGY_SESSION_EXC_SETTLE;
        break;

    case HW_METROLOGY_SESSION_EXC_SETTLE:
        if (session->io.excitation_dma_error(session->io.user))
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_EXCITATION);
            break;
        }
        if (deadline_reached(now_ms, session->wait_deadline_ms))
        {
            session->state = HW_METROLOGY_SESSION_ADC_DMA_START;
        }
        break;

    case HW_METROLOGY_SESSION_ADC_DMA_START:
        if (session->io.adc_start_capture(session->raw_words,
                                          session->raw_word_count,
                                          &session->adc_profile,
                                          session->io.user) != BSP_STATUS_OK)
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_ADC_ACQUIRE);
            break;
        }
        session->wait_deadline_ms = now_ms + dma_timeout_ms(&session->adc_profile);
        session->state = HW_METROLOGY_SESSION_ADC_DMA_WAIT;
        break;

    case HW_METROLOGY_SESSION_ADC_DMA_WAIT:
        if (session->io.adc_dma_error(session->io.user) ||
            session->io.excitation_dma_error(session->io.user))
        {
            session->block.dma_error = true;
            enter_abort(session, HW_METROLOGY_SESSION_ERR_DMA);
            break;
        }
        if (session->io.adc_dma_complete(session->io.user))
        {
            session->block.dma_complete = true;
            session->state = HW_METROLOGY_SESSION_ADC_DMA_COMPLETE;
            break;
        }
        if (deadline_reached(now_ms, session->wait_deadline_ms))
        {
            session->block.timeout = true;
            enter_abort(session, HW_METROLOGY_SESSION_ERR_TIMEOUT);
        }
        break;

    case HW_METROLOGY_SESSION_ADC_DMA_COMPLETE:
        session->io.adc_stop(session->io.user);
        session->state = HW_METROLOGY_SESSION_EXC_OFF;
        break;

    case HW_METROLOGY_SESSION_EXC_OFF:
        if (session->io.excitation_off(session->io.user) != BSP_STATUS_OK)
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_EXCITATION);
            break;
        }
        session->state = HW_METROLOGY_SESSION_ADC_RELEASE;
        break;

    case HW_METROLOGY_SESSION_ADC_RELEASE:
        session->io.adc_stop(session->io.user);
        session->state = HW_METROLOGY_SESSION_AUX_RESTORE;
        break;

    case HW_METROLOGY_SESSION_AUX_RESTORE:
        if (session->io.adc_restore(now_ms, session->io.user) != BSP_STATUS_OK)
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_AUX_RESTORE);
            break;
        }
        session->adc_owned = false;
        session->state = HW_METROLOGY_SESSION_AUX_RESUME;
        break;

    case HW_METROLOGY_SESSION_AUX_RESUME:
        session->io.aux_resume(now_ms, session->io.user);
        session->aux_paused = false;
        session->state = HW_METROLOGY_SESSION_RANGE_DISABLE;
        break;

    case HW_METROLOGY_SESSION_RANGE_DISABLE:
        if (session->io.range_force_disabled(session->io.user) != BSP_STATUS_OK)
        {
            enter_abort(session, HW_METROLOGY_SESSION_ERR_RANGE);
            break;
        }
        session->state = HW_METROLOGY_SESSION_QUIET_EXIT;
        break;

    case HW_METROLOGY_SESSION_QUIET_EXIT:
        session->io.quiet_request(false, session->io.user);
        session->quiet_held = false;
        session->state = HW_METROLOGY_SESSION_ANALYZE;
        break;

    case HW_METROLOGY_SESSION_ANALYZE:
        hw_metrology_analyze_block(session->raw_words, HW_METROLOGY_SAMPLES_PER_BLOCK, &session->block);
        session->block.valid = session->block.dma_complete && !session->block.dma_error &&
                               !session->block.timeout;
        session->dumpable = session->block.valid;
        session->state = HW_METROLOGY_SESSION_DONE;
        break;

    case HW_METROLOGY_SESSION_DONE:
    case HW_METROLOGY_SESSION_IDLE:
    case HW_METROLOGY_SESSION_ABORT:
        break;

    default:
        enter_abort(session, HW_METROLOGY_SESSION_ERR_INVALID);
        break;
    }

    note_k1(session);
    return BSP_STATUS_BUSY;
}

bsp_status_t hw_metrology_session_step(hw_metrology_session_t *session, uint32_t now_ms)
{
    if (session == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    if ((session->state == HW_METROLOGY_SESSION_IDLE) ||
        (session->state == HW_METROLOGY_SESSION_DONE))
    {
        return BSP_STATUS_OK;
    }

    if (session->state == HW_METROLOGY_SESSION_ABORT)
    {
        fail_safe_shutdown(session, now_ms);
        session->dumpable = false;
        session->block.valid = false;
        note_k1(session);
        session->state = HW_METROLOGY_SESSION_IDLE;
        return BSP_STATUS_ERROR;
    }

    const bsp_status_t status = step_success_path(session, now_ms);
    if (session->state == HW_METROLOGY_SESSION_ABORT)
    {
        fail_safe_shutdown(session, now_ms);
        session->dumpable = false;
        session->block.valid = false;
        note_k1(session);
        session->state = HW_METROLOGY_SESSION_IDLE;
        return BSP_STATUS_ERROR;
    }
    if (session->state == HW_METROLOGY_SESSION_DONE)
    {
        return BSP_STATUS_OK;
    }
    return status;
}

hw_metrology_session_state_t hw_metrology_session_state(const hw_metrology_session_t *session)
{
    return (session == NULL) ? HW_METROLOGY_SESSION_IDLE : session->state;
}

hw_metrology_session_error_t hw_metrology_session_error(const hw_metrology_session_t *session)
{
    return (session == NULL) ? HW_METROLOGY_SESSION_ERR_INVALID : session->error;
}

bool hw_metrology_session_active(const hw_metrology_session_t *session)
{
    if (session == NULL)
    {
        return false;
    }
    return (session->state != HW_METROLOGY_SESSION_IDLE) &&
           (session->state != HW_METROLOGY_SESSION_DONE);
}

bool hw_metrology_session_dumpable(const hw_metrology_session_t *session)
{
    return (session != NULL) && session->dumpable && session->block.valid;
}

bool hw_metrology_session_k1_left_safe(const hw_metrology_session_t *session)
{
    return (session != NULL) && session->k1_left_safe;
}

const hw_metrology_block_t *hw_metrology_session_block(const hw_metrology_session_t *session)
{
    return (session == NULL) ? NULL : &session->block;
}

void hw_metrology_session_acknowledge(hw_metrology_session_t *session)
{
    if (session == NULL)
    {
        return;
    }
    session->state = HW_METROLOGY_SESSION_IDLE;
    session->dumpable = false;
}

const char *hw_metrology_session_state_string(hw_metrology_session_state_t state)
{
    switch (state)
    {
    case HW_METROLOGY_SESSION_IDLE:
        return "IDLE";
    case HW_METROLOGY_SESSION_FORCE_K1_SAFE:
        return "FORCE_K1_SAFE";
    case HW_METROLOGY_SESSION_RANGE_REQUEST:
        return "RANGE_REQUEST";
    case HW_METROLOGY_SESSION_RANGE_WAIT:
        return "RANGE_WAIT";
    case HW_METROLOGY_SESSION_QUIET_ENTER:
        return "QUIET_ENTER";
    case HW_METROLOGY_SESSION_AUX_PAUSE:
        return "AUX_PAUSE";
    case HW_METROLOGY_SESSION_ADC_ACQUIRE:
        return "ADC_ACQUIRE";
    case HW_METROLOGY_SESSION_EXC_NEUTRAL:
        return "EXC_NEUTRAL";
    case HW_METROLOGY_SESSION_EXC_NEUTRAL_WAIT:
        return "EXC_NEUTRAL_WAIT";
    case HW_METROLOGY_SESSION_EXC_SINE_START:
        return "EXC_SINE_START";
    case HW_METROLOGY_SESSION_EXC_SETTLE:
        return "EXC_SETTLE";
    case HW_METROLOGY_SESSION_ADC_DMA_START:
        return "ADC_DMA_START";
    case HW_METROLOGY_SESSION_ADC_DMA_WAIT:
        return "ADC_DMA_WAIT";
    case HW_METROLOGY_SESSION_ADC_DMA_COMPLETE:
        return "ADC_DMA_COMPLETE";
    case HW_METROLOGY_SESSION_EXC_OFF:
        return "EXC_OFF";
    case HW_METROLOGY_SESSION_ADC_RELEASE:
        return "ADC_RELEASE";
    case HW_METROLOGY_SESSION_AUX_RESTORE:
        return "AUX_RESTORE";
    case HW_METROLOGY_SESSION_AUX_RESUME:
        return "AUX_RESUME";
    case HW_METROLOGY_SESSION_RANGE_DISABLE:
        return "RANGE_DISABLE";
    case HW_METROLOGY_SESSION_QUIET_EXIT:
        return "QUIET_EXIT";
    case HW_METROLOGY_SESSION_ANALYZE:
        return "ANALYZE";
    case HW_METROLOGY_SESSION_DONE:
        return "DONE";
    case HW_METROLOGY_SESSION_ABORT:
        return "ABORT";
    default:
        return "UNKNOWN";
    }
}

const char *hw_metrology_session_error_string(hw_metrology_session_error_t error)
{
    switch (error)
    {
    case HW_METROLOGY_SESSION_OK:
        return "OK";
    case HW_METROLOGY_SESSION_ERR_CLOCK:
        return "CLOCK";
    case HW_METROLOGY_SESSION_ERR_BUSY:
        return "BUSY";
    case HW_METROLOGY_SESSION_ERR_INVALID:
        return "INVALID";
    case HW_METROLOGY_SESSION_ERR_FORBIDDEN_AMPLITUDE:
        return "FORBIDDEN_AMPLITUDE";
    case HW_METROLOGY_SESSION_ERR_RANGE:
        return "RANGE";
    case HW_METROLOGY_SESSION_ERR_ADC_ACQUIRE:
        return "ADC_ACQUIRE";
    case HW_METROLOGY_SESSION_ERR_EXCITATION:
        return "EXCITATION";
    case HW_METROLOGY_SESSION_ERR_DMA:
        return "DMA";
    case HW_METROLOGY_SESSION_ERR_TIMEOUT:
        return "TIMEOUT";
    case HW_METROLOGY_SESSION_ERR_AUX_RESTORE:
        return "AUX_RESTORE";
    default:
        return "UNKNOWN";
    }
}
