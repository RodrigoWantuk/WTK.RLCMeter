#ifndef WTK_BUTTONS_H
#define WTK_BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

enum
{
    BUTTONS_EVENT_QUEUE_CAPACITY = 8u,
};

typedef enum
{
    BUTTON_ID_UP = 0,
    BUTTON_ID_OK,
    BUTTON_ID_DOWN,
    BUTTON_ID_COUNT,
} button_id_t;

typedef enum
{
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_PRESS,
    BUTTON_EVENT_RELEASE,
    BUTTON_EVENT_LONG_PRESS,
    BUTTON_EVENT_REPEAT,
} button_event_type_t;

typedef struct
{
    button_id_t button;
    button_event_type_t type;
    uint32_t timestamp_ms;
} button_event_t;

typedef struct
{
    uint16_t debounce_ms;
    uint16_t long_press_ms;
    uint16_t repeat_delay_ms;
    uint16_t repeat_period_ms;
} buttons_config_t;

typedef struct
{
    bool raw_pressed;
    bool debounced_pressed;
    bool long_sent;
    uint32_t raw_changed_ms;
    uint32_t press_started_ms;
    uint32_t next_repeat_ms;
} button_state_t;

typedef struct
{
    buttons_config_t config;
    button_state_t states[BUTTON_ID_COUNT];
    button_event_t events[BUTTONS_EVENT_QUEUE_CAPACITY];
    uint8_t event_head;
    uint8_t event_tail;
    uint8_t event_count;
} buttons_t;

void buttons_init(buttons_t *buttons, const buttons_config_t *config);
void buttons_update(buttons_t *buttons, uint8_t raw_pressed_mask, uint32_t now_ms);
bool buttons_pop_event(buttons_t *buttons, button_event_t *event);
const char *button_event_type_string(button_event_type_t type);

#endif
