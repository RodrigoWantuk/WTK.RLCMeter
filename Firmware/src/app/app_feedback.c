#include "app/app_feedback.h"
#include <stddef.h>

typedef struct { uint16_t hz, ms; } tone_t;
static const tone_t patterns[APP_FEEDBACK_COUNT][3] = {
    {{0u,0u}}, {{1200u,15u}}, {{900u,25u}}, {{2000u,25u}},
    {{1760u,35u},{2340u,45u}}, {{1320u,30u},{1760u,45u}},
    {{440u,65u},{330u,85u}}, {{500u,90u},{500u,90u},{500u,90u}}
};

void app_feedback_init(app_feedback_t *feedback)
{
    if (feedback != NULL) *feedback = (app_feedback_t){0};
}
static void pop(app_feedback_t *feedback)
{
    for (unsigned i = 1u; i < feedback->count; ++i) feedback->queue[i-1u] = feedback->queue[i];
    if (feedback->count != 0u) --feedback->count;
    feedback->phase = 0u;
    feedback->active = false;
    feedback->retrying = false;
}
bool app_feedback_request(app_feedback_t *feedback, app_feedback_event_t event)
{
    if (feedback == NULL || event <= APP_FEEDBACK_NONE || event >= APP_FEEDBACK_COUNT) return false;
    for (unsigned i = 0u; i < feedback->count; ++i)
        if (feedback->queue[i] == (uint8_t)event) return true;
    if (feedback->count == sizeof(feedback->queue))
    {
        unsigned victim = feedback->count;
        /* Never replace the event whose tone is currently playing. */
        for (unsigned i = feedback->active ? 1u : 0u; i < feedback->count; ++i)
            if (feedback->queue[i] < (uint8_t)event) { victim = i; break; }
        ++feedback->dropped;
        if (victim == feedback->count) return false;
        feedback->queue[victim] = (uint8_t)event;
        if (victim == 0u) { feedback->phase = 0u; feedback->retrying = false; }
        return true;
    }
    feedback->queue[feedback->count++] = (uint8_t)event;
    return true;
}
void app_feedback_step(app_feedback_t *feedback, const app_feedback_io_t *io,
                       bool enabled, bool quiet, uint32_t now_ms)
{
    if (feedback == NULL || io == NULL || io->play == NULL || io->stop == NULL) return;
    if (!enabled || quiet)
    {
        if (feedback->active) io->stop(io->user);
        feedback->dropped += feedback->count;
        feedback->count = 0u;
        feedback->phase = 0u;
        feedback->active = false;
        feedback->retrying = false;
        return;
    }
    if (feedback->count == 0u) return;
    if (feedback->active)
    {
        if ((uint32_t)(now_ms - feedback->due_ms) >= 0x80000000u) return;
        io->stop(io->user);
        feedback->active = false;
    }
    if (feedback->phase >= 3u) { pop(feedback); return; }
    const tone_t tone = patterns[feedback->queue[0]][feedback->phase];
    if (tone.hz == 0u) { pop(feedback); return; }
    const bsp_status_t status = io->play(tone.hz, tone.ms, now_ms, io->user);
    feedback->last_error = status;
    if (status == BSP_STATUS_BUSY)
    {
        if (!feedback->retrying) { feedback->retry_ms = now_ms; feedback->retrying = true; }
        if ((uint32_t)(now_ms - feedback->retry_ms) < 250u) return;
    }
    if (status != BSP_STATUS_OK) { ++feedback->dropped; pop(feedback); return; }
    feedback->retrying = false;
    feedback->active = true;
    feedback->due_ms = now_ms + tone.ms + 30u;
    ++feedback->phase;
}
