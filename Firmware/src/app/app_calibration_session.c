#include "app/app_calibration_session.h"

#include <stddef.h>

static bool io_ready(const app_cal_session_io_t *io)
{
    return (io != NULL) &&
           (io->start_capture != NULL) &&
           (io->step_capture != NULL) &&
           (io->capture_active != NULL) &&
           (io->capture_done != NULL) &&
           (io->capture_dumpable != NULL) &&
           (io->capture_block != NULL) &&
           (io->capture_error != NULL) &&
           (io->capture_acknowledge != NULL) &&
           (io->capture_abort != NULL);
}

static app_calibration_workflow_t *workflow(app_calibration_session_t *session)
{
    return (session == NULL) ? NULL : app_calibration_service_workflow(session->service);
}

static const app_calibration_workflow_t *workflow_const(const app_calibration_session_t *session)
{
    return (session == NULL) ? NULL : app_calibration_service_workflow_const(session->service);
}

static app_cal_session_event_t take_event(app_calibration_session_t *session)
{
    const app_cal_session_event_t event = session->pending_event;
    session->pending_event = APP_CAL_SESSION_EVENT_NONE;
    return event;
}

static uint32_t reject_from_measure_error(hw_metrology_measure_error_t error)
{
    if ((error == HW_METROLOGY_MEASURE_ERR_PERMIT) ||
        (error == HW_METROLOGY_MEASURE_ERR_ABORT))
    {
        return APP_CAL_REJECT_SAFETY_ABORT;
    }
    return APP_CAL_REJECT_PHASE05;
}

static bool workflow_terminal(app_cal_workflow_state_t state)
{
    return (state == APP_CAL_WORKFLOW_COMPLETE) ||
           (state == APP_CAL_WORKFLOW_FAILED) ||
           (state == APP_CAL_WORKFLOW_CANCELED);
}

static void queue_terminal_event(app_calibration_session_t *session)
{
    const app_calibration_workflow_t *wf = workflow_const(session);
    const app_cal_workflow_state_t state = app_calibration_workflow_state(wf);
    if (!workflow_terminal(state))
    {
        return;
    }
    session->state = APP_CAL_SESSION_DONE;
    if (state == APP_CAL_WORKFLOW_COMPLETE)
    {
        session->pending_event = APP_CAL_SESSION_EVENT_COMPLETE;
    }
    else if (state == APP_CAL_WORKFLOW_CANCELED)
    {
        session->pending_event = APP_CAL_SESSION_EVENT_CANCELED;
    }
    else
    {
        session->pending_event = APP_CAL_SESSION_EVENT_FAILED;
    }
}

bsp_status_t app_calibration_session_init(app_calibration_session_t *session,
                                          app_calibration_service_t *service,
                                          const app_cal_session_io_t *io)
{
    if ((session == NULL) || (service == NULL) || !io_ready(io))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    *session = (app_calibration_session_t){0};
    session->io = *io;
    session->service = service;
    session->state = APP_CAL_SESSION_IDLE;
    session->last_sample_status = BSP_STATUS_OK;
    return BSP_STATUS_OK;
}

bsp_status_t app_calibration_session_start(app_calibration_session_t *session,
                                           const app_cal_workflow_request_t *request,
                                           const bsp_clock_summary_t *clock_summary,
                                           bsp_status_t clock_status,
                                           uint32_t now_ms)
{
    (void)now_ms;
    if ((session == NULL) || (request == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (app_calibration_session_active(session))
    {
        return BSP_STATUS_BUSY;
    }
    const bsp_status_t status = app_calibration_service_start_workflow(session->service, request);
    if (status != BSP_STATUS_BUSY)
    {
        return status;
    }
    session->clock_summary = clock_summary;
    session->clock_status = clock_status;
    session->state = APP_CAL_SESSION_START_CAPTURE;
    session->pending_event = APP_CAL_SESSION_EVENT_BEGIN;
    session->last_reject_flags = APP_CAL_REJECT_NONE;
    session->last_sample_status = BSP_STATUS_OK;
    session->started_capture = false;
    return BSP_STATUS_BUSY;
}

app_cal_session_event_t app_calibration_session_step(app_calibration_session_t *session,
                                                     uint32_t now_ms)
{
    if (session == NULL)
    {
        return APP_CAL_SESSION_EVENT_ERROR;
    }
    if (session->pending_event != APP_CAL_SESSION_EVENT_NONE)
    {
        return take_event(session);
    }

    app_calibration_workflow_t *wf = workflow(session);
    if (wf == NULL)
    {
        session->state = APP_CAL_SESSION_DONE;
        return APP_CAL_SESSION_EVENT_ERROR;
    }

    switch (session->state)
    {
    case APP_CAL_SESSION_IDLE:
    case APP_CAL_SESSION_DONE:
        return APP_CAL_SESSION_EVENT_NONE;

    case APP_CAL_SESSION_START_CAPTURE:
    {
        measurement_cal_key_t key = {0};
        if (app_calibration_workflow_capture_request(wf, &key) != BSP_STATUS_OK)
        {
            queue_terminal_event(session);
            return take_event(session);
        }
        const hw_metrology_measure_request_t request = {
            .clock_summary = session->clock_summary,
            .clock_init_status = session->clock_status,
            .frequency = key.frequency,
            .amplitude = key.amplitude,
            .range_id = key.range_id,
        };
        const bsp_status_t started = session->io.start_capture(&request, now_ms, session->io.user);
        (void)app_calibration_workflow_mark_capture_started(wf);
        if ((started != BSP_STATUS_OK) && (started != BSP_STATUS_BUSY))
        {
            (void)app_calibration_workflow_submit_failure(wf, APP_CAL_REJECT_PHASE05);
            session->last_reject_flags = APP_CAL_REJECT_PHASE05;
            session->state = APP_CAL_SESSION_START_CAPTURE;
            session->pending_event = APP_CAL_SESSION_EVENT_CAPTURE_REJECTED;
            return take_event(session);
        }
        session->started_capture = true;
        session->state = APP_CAL_SESSION_WAIT_CAPTURE;
        session->pending_event = APP_CAL_SESSION_EVENT_CAPTURE_BEGIN;
        return take_event(session);
    }

    case APP_CAL_SESSION_WAIT_CAPTURE:
    {
        const bsp_status_t step_status = session->io.step_capture(now_ms, session->io.user);
        if (!session->io.capture_done(session->io.user))
        {
            (void)step_status;
            return APP_CAL_SESSION_EVENT_NONE;
        }
        if (session->io.capture_dumpable(session->io.user))
        {
            session->last_sample_status =
                app_calibration_workflow_sample_from_block(session->io.capture_block(session->io.user),
                                                           &wf->request,
                                                           &session->last_sample);
            (void)app_calibration_workflow_submit_sample(wf, &session->last_sample);
            session->last_reject_flags = app_calibration_workflow_last_reject_flags(wf);
        }
        else
        {
            session->last_reject_flags = reject_from_measure_error(session->io.capture_error(session->io.user));
            (void)app_calibration_workflow_submit_failure(wf, session->last_reject_flags);
            session->last_sample_status = BSP_STATUS_ERROR;
        }
        session->io.capture_acknowledge(session->io.user);
        session->state = APP_CAL_SESSION_START_CAPTURE;
        session->pending_event = (session->last_reject_flags == APP_CAL_REJECT_NONE) ?
                                     APP_CAL_SESSION_EVENT_CAPTURE_ACCEPTED :
                                     APP_CAL_SESSION_EVENT_CAPTURE_REJECTED;
        return take_event(session);
    }

    case APP_CAL_SESSION_CANCELING:
    {
        (void)session->io.step_capture(now_ms, session->io.user);
        if (!session->io.capture_active(session->io.user) ||
            session->io.capture_done(session->io.user))
        {
            session->io.capture_acknowledge(session->io.user);
            app_calibration_workflow_cancel_complete(wf);
            queue_terminal_event(session);
            return take_event(session);
        }
        return APP_CAL_SESSION_EVENT_NONE;
    }

    default:
        session->state = APP_CAL_SESSION_DONE;
        return APP_CAL_SESSION_EVENT_ERROR;
    }
}

bsp_status_t app_calibration_session_cancel(app_calibration_session_t *session)
{
    if (session == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    app_calibration_workflow_t *wf = workflow(session);
    if (wf == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!app_calibration_session_active(session))
    {
        return BSP_STATUS_OK;
    }
    const bsp_status_t cancel_status = app_calibration_workflow_cancel(wf);
    if ((session->state == APP_CAL_SESSION_WAIT_CAPTURE) && session->io.capture_active(session->io.user))
    {
        (void)session->io.capture_abort(session->io.user);
        session->state = APP_CAL_SESSION_CANCELING;
        return BSP_STATUS_BUSY;
    }
    if (cancel_status != BSP_STATUS_BUSY)
    {
        app_calibration_workflow_cancel_complete(wf);
        queue_terminal_event(session);
        return BSP_STATUS_OK;
    }
    session->state = APP_CAL_SESSION_CANCELING;
    return BSP_STATUS_BUSY;
}

bool app_calibration_session_active(const app_calibration_session_t *session)
{
    return (session != NULL) &&
           (session->state != APP_CAL_SESSION_IDLE) &&
           (session->state != APP_CAL_SESSION_DONE);
}

app_cal_session_state_t app_calibration_session_state(const app_calibration_session_t *session)
{
    return (session == NULL) ? APP_CAL_SESSION_IDLE : session->state;
}

const app_cal_evidence_t *app_calibration_session_evidence(const app_calibration_session_t *session)
{
    return (session == NULL) ? NULL : app_calibration_workflow_evidence(workflow_const(session));
}

uint32_t app_calibration_session_last_reject_flags(const app_calibration_session_t *session)
{
    return (session == NULL) ? APP_CAL_REJECT_DSP : session->last_reject_flags;
}

bsp_status_t app_calibration_session_last_sample_status(const app_calibration_session_t *session)
{
    return (session == NULL) ? BSP_STATUS_INVALID_ARG : session->last_sample_status;
}

uint32_t app_calibration_session_context_size_bytes(void)
{
    return (uint32_t)sizeof(app_calibration_session_t);
}

const char *app_cal_session_event_string(app_cal_session_event_t event)
{
    switch (event)
    {
    case APP_CAL_SESSION_EVENT_BEGIN:
        return "BEGIN";
    case APP_CAL_SESSION_EVENT_CAPTURE_BEGIN:
        return "CAPTURE_BEGIN";
    case APP_CAL_SESSION_EVENT_CAPTURE_ACCEPTED:
        return "CAPTURE_ACCEPTED";
    case APP_CAL_SESSION_EVENT_CAPTURE_REJECTED:
        return "CAPTURE_REJECTED";
    case APP_CAL_SESSION_EVENT_COMPLETE:
        return "COMPLETE";
    case APP_CAL_SESSION_EVENT_FAILED:
        return "FAILED";
    case APP_CAL_SESSION_EVENT_CANCELED:
        return "CANCELED";
    case APP_CAL_SESSION_EVENT_ERROR:
        return "ERROR";
    case APP_CAL_SESSION_EVENT_NONE:
    default:
        return "NONE";
    }
}

const char *app_cal_session_state_string(app_cal_session_state_t state)
{
    switch (state)
    {
    case APP_CAL_SESSION_IDLE:
        return "IDLE";
    case APP_CAL_SESSION_START_CAPTURE:
        return "START_CAPTURE";
    case APP_CAL_SESSION_WAIT_CAPTURE:
        return "WAIT_CAPTURE";
    case APP_CAL_SESSION_CANCELING:
        return "CANCELING";
    case APP_CAL_SESSION_DONE:
    default:
        return "DONE";
    }
}
