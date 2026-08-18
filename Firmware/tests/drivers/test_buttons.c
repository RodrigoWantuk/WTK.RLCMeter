#include "drivers/buttons.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static int g_failures = 0;

static void expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        g_failures++;
    }
}

static void expect_event(buttons_t *buttons, button_id_t button, button_event_type_t type)
{
    button_event_t event = {0};
    expect_true(buttons_pop_event(buttons, &event), "expected button event");
    expect_true(event.button == button, "unexpected button id");
    if (event.type != type)
    {
        (void)fprintf(stderr,
                      "FAIL: unexpected event type got=%s expected=%s time=%lu\n",
                      button_event_type_string(event.type),
                      button_event_type_string(type),
                      (unsigned long)event.timestamp_ms);
        g_failures++;
    }
}

static void test_press_release_debounce(void)
{
    const buttons_config_t config = {
        .debounce_ms = 10u,
        .long_press_ms = 50u,
        .repeat_delay_ms = 80u,
        .repeat_period_ms = 20u,
    };
    buttons_t buttons;
    buttons_init(&buttons, &config);

    buttons_update(&buttons, (uint8_t)(1u << (uint8_t)BUTTON_ID_OK), 0u);
    expect_true(!buttons_pop_event(&buttons, &(button_event_t){0}), "bounce must not emit immediately");
    buttons_update(&buttons, 0u, 5u);
    buttons_update(&buttons, (uint8_t)(1u << (uint8_t)BUTTON_ID_OK), 8u);
    buttons_update(&buttons, (uint8_t)(1u << (uint8_t)BUTTON_ID_OK), 18u);
    expect_event(&buttons, BUTTON_ID_OK, BUTTON_EVENT_PRESS);

    buttons_update(&buttons, 0u, 20u);
    buttons_update(&buttons, 0u, 29u);
    expect_true(!buttons_pop_event(&buttons, &(button_event_t){0}), "release before debounce must wait");
    buttons_update(&buttons, 0u, 30u);
    expect_event(&buttons, BUTTON_ID_OK, BUTTON_EVENT_RELEASE);
}

static void test_long_and_repeat_order(void)
{
    const buttons_config_t config = {
        .debounce_ms = 5u,
        .long_press_ms = 30u,
        .repeat_delay_ms = 45u,
        .repeat_period_ms = 10u,
    };
    buttons_t buttons;
    buttons_init(&buttons, &config);

    const uint8_t down_mask = (uint8_t)(1u << (uint8_t)BUTTON_ID_DOWN);
    buttons_update(&buttons, down_mask, 0u);
    buttons_update(&buttons, down_mask, 5u);
    expect_event(&buttons, BUTTON_ID_DOWN, BUTTON_EVENT_PRESS);

    buttons_update(&buttons, down_mask, 34u);
    expect_true(!buttons_pop_event(&buttons, &(button_event_t){0}), "long threshold not reached");
    buttons_update(&buttons, down_mask, 35u);
    expect_event(&buttons, BUTTON_ID_DOWN, BUTTON_EVENT_LONG_PRESS);

    buttons_update(&buttons, down_mask, 49u);
    expect_true(!buttons_pop_event(&buttons, &(button_event_t){0}), "repeat threshold not reached");
    buttons_update(&buttons, down_mask, 50u);
    expect_event(&buttons, BUTTON_ID_DOWN, BUTTON_EVENT_REPEAT);
    buttons_update(&buttons, down_mask, 60u);
    expect_event(&buttons, BUTTON_ID_DOWN, BUTTON_EVENT_REPEAT);

    buttons_update(&buttons, 0u, 70u);
    buttons_update(&buttons, 0u, 75u);
    expect_event(&buttons, BUTTON_ID_DOWN, BUTTON_EVENT_RELEASE);
}

static void test_simultaneous_order(void)
{
    const buttons_config_t config = {
        .debounce_ms = 1u,
        .long_press_ms = 100u,
        .repeat_delay_ms = 200u,
        .repeat_period_ms = 50u,
    };
    buttons_t buttons;
    buttons_init(&buttons, &config);

    const uint8_t mask = (uint8_t)((1u << (uint8_t)BUTTON_ID_UP) | (1u << (uint8_t)BUTTON_ID_OK));
    buttons_update(&buttons, mask, 0u);
    buttons_update(&buttons, mask, 1u);
    expect_event(&buttons, BUTTON_ID_UP, BUTTON_EVENT_PRESS);
    expect_event(&buttons, BUTTON_ID_OK, BUTTON_EVENT_PRESS);
}

int main(void)
{
    test_press_release_debounce();
    test_long_and_repeat_order();
    test_simultaneous_order();
    return (g_failures == 0) ? 0 : 1;
}
