#include <assert.h>
#include "app/app_feedback.h"
typedef struct { unsigned plays, stops, fault_tones; uint16_t hz; bsp_status_t status; } fake_t;
static bsp_status_t play(uint16_t hz, uint16_t ms, uint32_t now, void *user)
{
    (void)ms; (void)now;
    fake_t *fake = user;
    ++fake->plays;
    fake->hz = hz;
    if (hz == 500u && fake->status == BSP_STATUS_OK) ++fake->fault_tones;
    return fake->status;
}
static void stop(void *user) { ++((fake_t *)user)->stops; }
int main(void)
{
    fake_t fake = {0};
    app_feedback_t feedback;
    app_feedback_init(&feedback);
    const app_feedback_io_t io = {.play = play, .stop = stop, .user = &fake};
    assert(app_feedback_request(&feedback, APP_FEEDBACK_FAULT));
    for (uint32_t ms = 0u; ms <= 1000u; ms += 10u) app_feedback_step(&feedback, &io, true, false, ms);
    assert(fake.plays == 3u && feedback.count == 0u && !feedback.active);
    assert(app_feedback_request(&feedback, APP_FEEDBACK_COMPLETE));
    app_feedback_step(&feedback, &io, true, false, UINT32_MAX - 5u);
    assert(feedback.active);
    const unsigned started = fake.plays;
    app_feedback_step(&feedback, &io, true, true, 0u);
    assert(feedback.count == 0u && !feedback.active);
    app_feedback_step(&feedback, &io, true, false, 100u);
    assert(fake.plays == started); /* Quiet cancellation never replays old tones. */
    assert(app_feedback_request(&feedback, APP_FEEDBACK_NAVIGATE));
    assert(app_feedback_request(&feedback, APP_FEEDBACK_NAVIGATE) && feedback.count == 1u);
    app_feedback_step(&feedback, &io, false, false, 101u);
    app_feedback_step(&feedback, &io, true, false, 102u);
    assert(fake.plays == started && feedback.count == 0u); /* No unmute backlog. */
    fake.status = BSP_STATUS_BUSY;
    assert(app_feedback_request(&feedback, APP_FEEDBACK_CONFIRM));
    app_feedback_step(&feedback, &io, true, false, UINT32_MAX - 100u);
    app_feedback_step(&feedback, &io, true, false, 200u);
    assert(feedback.count == 0u && feedback.last_error == BSP_STATUS_BUSY);
    assert(feedback.dropped >= 3u);
    app_feedback_init(&feedback);
    fake = (fake_t){0};
    assert(app_feedback_request(&feedback, APP_FEEDBACK_COMPLETE));
    assert(app_feedback_request(&feedback, APP_FEEDBACK_NAVIGATE));
    assert(app_feedback_request(&feedback, APP_FEEDBACK_BACK));
    assert(app_feedback_request(&feedback, APP_FEEDBACK_CONFIRM));
    app_feedback_step(&feedback, &io, true, false, 0u);
    fake.status = BSP_STATUS_BUSY;
    app_feedback_step(&feedback, &io, true, false, 65u);
    assert(app_feedback_request(&feedback, APP_FEEDBACK_FAULT));
    fake.status = BSP_STATUS_OK;
    for (uint32_t ms = 70u; ms < 1000u; ms += 10u) app_feedback_step(&feedback, &io, true, false, ms);
    assert(fake.fault_tones == 3u); /* Replacement starts the complete high-priority pattern. */
    return 0;
}
