#include "drivers/buttons.h"

#include <stddef.h>

static const buttons_config_t g_default_config = {
    .debounce_ms = 25u,
    .long_press_ms = 700u,
    .repeat_delay_ms = 900u,
    .repeat_period_ms = 175u,
};

static bool button_mask_pressed(uint8_t mask, button_id_t button)
{
    return (mask & (uint8_t)(1u << (uint8_t)button)) != 0u;
}

static void enqueue(buttons_t *buttons, button_id_t button, button_event_type_t type, uint32_t now_ms)
{
    if (buttons->event_count >= BUTTONS_EVENT_QUEUE_CAPACITY)
    {
        return;
    }

    buttons->events[buttons->event_tail].button = button;
    buttons->events[buttons->event_tail].type = type;
    buttons->events[buttons->event_tail].timestamp_ms = now_ms;
    buttons->event_tail = (uint8_t)((buttons->event_tail + 1u) % BUTTONS_EVENT_QUEUE_CAPACITY);
    buttons->event_count++;
}

void buttons_init(buttons_t *buttons, const buttons_config_t *config)
{
    if (buttons == NULL)
    {
        return;
    }

    buttons->config = (config != NULL) ? *config : g_default_config;
    buttons->event_head = 0u;
    buttons->event_tail = 0u;
    buttons->event_count = 0u;

    for (uint8_t i = 0u; i < (uint8_t)BUTTON_ID_COUNT; i++)
    {
        buttons->states[i].raw_pressed = false;
        buttons->states[i].debounced_pressed = false;
        buttons->states[i].long_sent = false;
        buttons->states[i].raw_changed_ms = 0u;
        buttons->states[i].press_started_ms = 0u;
        buttons->states[i].next_repeat_ms = 0u;
    }
}

void buttons_update(buttons_t *buttons, uint8_t raw_pressed_mask, uint32_t now_ms)
{
    if (buttons == NULL)
    {
        return;
    }

    for (uint8_t i = 0u; i < (uint8_t)BUTTON_ID_COUNT; i++)
    {
        button_state_t *const state = &buttons->states[i];
        const button_id_t button = (button_id_t)i;
        const bool raw_pressed = button_mask_pressed(raw_pressed_mask, button);

        if (raw_pressed != state->raw_pressed)
        {
            state->raw_pressed = raw_pressed;
            state->raw_changed_ms = now_ms;
        }

        if ((state->raw_pressed != state->debounced_pressed) &&
            ((now_ms - state->raw_changed_ms) >= buttons->config.debounce_ms))
        {
            state->debounced_pressed = state->raw_pressed;

            if (state->debounced_pressed)
            {
                state->press_started_ms = now_ms;
                state->next_repeat_ms = now_ms + buttons->config.repeat_delay_ms;
                state->long_sent = false;
                enqueue(buttons, button, BUTTON_EVENT_PRESS, now_ms);
            }
            else
            {
                enqueue(buttons, button, BUTTON_EVENT_RELEASE, now_ms);
                state->long_sent = false;
            }
        }

        if (state->debounced_pressed && state->raw_pressed)
        {
            if (!state->long_sent &&
                ((now_ms - state->press_started_ms) >= buttons->config.long_press_ms))
            {
                state->long_sent = true;
                enqueue(buttons, button, BUTTON_EVENT_LONG_PRESS, now_ms);
            }

            if ((now_ms - state->next_repeat_ms) < 0x80000000u)
            {
                enqueue(buttons, button, BUTTON_EVENT_REPEAT, now_ms);
                state->next_repeat_ms += buttons->config.repeat_period_ms;
            }
        }
    }
}

bool buttons_pop_event(buttons_t *buttons, button_event_t *event)
{
    if ((buttons == NULL) || (event == NULL) || (buttons->event_count == 0u))
    {
        return false;
    }

    *event = buttons->events[buttons->event_head];
    buttons->event_head = (uint8_t)((buttons->event_head + 1u) % BUTTONS_EVENT_QUEUE_CAPACITY);
    buttons->event_count--;
    return true;
}

const char *button_event_type_string(button_event_type_t type)
{
    switch (type)
    {
    case BUTTON_EVENT_NONE:
        return "NONE";
    case BUTTON_EVENT_PRESS:
        return "PRESS";
    case BUTTON_EVENT_RELEASE:
        return "RELEASE";
    case BUTTON_EVENT_LONG_PRESS:
        return "LONG_PRESS";
    case BUTTON_EVENT_REPEAT:
        return "REPEAT";
    default:
        return "UNKNOWN";
    }
}
