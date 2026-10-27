#ifndef WTK_APP_FEEDBACK_H
#define WTK_APP_FEEDBACK_H
#include <stdbool.h>
#include <stdint.h>
#include "bsp/bsp_status.h"

typedef enum
{
    APP_FEEDBACK_NONE = 0,
    APP_FEEDBACK_NAVIGATE,
    APP_FEEDBACK_BACK,
    APP_FEEDBACK_CONFIRM,
    APP_FEEDBACK_COMPLETE,
    APP_FEEDBACK_SAVED,
    APP_FEEDBACK_ERROR,
    APP_FEEDBACK_FAULT,
    APP_FEEDBACK_COUNT
} app_feedback_event_t;
typedef struct
{
    uint32_t due_ms;
    uint32_t retry_ms;
    uint32_t dropped;
    bsp_status_t last_error;
    uint8_t queue[4];
    uint8_t count;
    uint8_t phase;
    bool active;
    bool retrying;
} app_feedback_t;
typedef struct
{
    bsp_status_t (*play)(uint16_t hz, uint16_t duration_ms, uint32_t now_ms, void *user);
    void (*stop)(void *user);
    void *user;
} app_feedback_io_t;

void app_feedback_init(app_feedback_t *feedback);
/* Bounded queue; higher priority may evict the oldest lower-priority request. */
bool app_feedback_request(app_feedback_t *feedback, app_feedback_event_t event);
void app_feedback_step(app_feedback_t *feedback, const app_feedback_io_t *io,
                       bool enabled, bool quiet, uint32_t now_ms);
#endif
