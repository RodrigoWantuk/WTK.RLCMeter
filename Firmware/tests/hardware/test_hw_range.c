#include "hardware/hw_range.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum
{
    EVENT_ENABLE = 0,
    EVENT_ADDRESS,
} event_type_t;

typedef struct
{
    event_type_t type;
    bool enable;
    uint8_t address;
} range_event_t;

typedef struct
{
    bool enabled;
    uint8_t address;
    range_event_t events[32];
    uint8_t count;
    bool address_changed_while_enabled;
} fake_range_io_t;

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static void record_event(fake_range_io_t *io, range_event_t event)
{
    if (io->count < (uint8_t)(sizeof(io->events) / sizeof(io->events[0])))
    {
        io->events[io->count] = event;
        io->count++;
    }
}

static bsp_status_t fake_write_enable(bool high, void *user_data)
{
    fake_range_io_t *io = (fake_range_io_t *)user_data;
    io->enabled = high;
    record_event(io, (range_event_t){
                         .type = EVENT_ENABLE,
                         .enable = high,
                         .address = io->address,
                     });
    return BSP_STATUS_OK;
}

static bsp_status_t fake_write_address(uint8_t address, void *user_data)
{
    fake_range_io_t *io = (fake_range_io_t *)user_data;
    if (io->enabled && (address != io->address))
    {
        io->address_changed_while_enabled = true;
    }
    io->address = address;
    record_event(io, (range_event_t){
                         .type = EVENT_ADDRESS,
                         .enable = io->enabled,
                         .address = address,
                     });
    return BSP_STATUS_OK;
}

static hw_range_io_t fake_io(fake_range_io_t *io)
{
    return (hw_range_io_t){
        .write_enable = fake_write_enable,
        .write_address = fake_write_address,
        .user_data = io,
    };
}

static int test_exact_mapping(void)
{
    int failures = 0;
    uint8_t address = 0u;
    failures += expect_true(hw_range_id_to_address(HW_RANGE_ID_10R, &address) == BSP_STATUS_OK && address == 0u,
                            "10R maps to address 0");
    failures += expect_true(hw_range_id_to_address(HW_RANGE_ID_100R, &address) == BSP_STATUS_OK && address == 1u,
                            "100R maps to address 1");
    failures += expect_true(hw_range_id_to_address(HW_RANGE_ID_1K, &address) == BSP_STATUS_OK && address == 2u,
                            "1K maps to address 2");
    failures += expect_true(hw_range_id_to_address(HW_RANGE_ID_10K, &address) == BSP_STATUS_OK && address == 3u,
                            "10K maps to address 3");
    failures += expect_true(hw_range_id_to_address(HW_RANGE_ID_100K, &address) == BSP_STATUS_OK && address == 4u,
                            "100K maps to address 4");
    failures += expect_true(hw_range_id_to_address(HW_RANGE_ID_1M, &address) == BSP_STATUS_OK && address == 5u,
                            "1M maps to address 5");
    failures += expect_true(hw_range_id_to_address((hw_range_id_t)6u, &address) == BSP_STATUS_INVALID_ARG,
                            "address 6 is rejected");
    failures += expect_true(hw_range_id_to_address((hw_range_id_t)7u, &address) == BSP_STATUS_INVALID_ARG,
                            "address 7 is rejected");
    return failures;
}

static int test_transition_timing(void)
{
    int failures = 0;
    fake_range_io_t fake = {0};
    hw_range_t range;
    hw_range_io_t io = fake_io(&fake);

    failures += expect_true(hw_range_init(&range, &io) == BSP_STATUS_OK, "range init succeeds");
    failures += expect_true(!fake.enabled, "range starts disabled");
    failures += expect_true(!hw_range_is_ready(&range), "range not ready after init");
    failures += expect_true(hw_range_request(&range, HW_RANGE_ID_1K, 10u) == BSP_STATUS_BUSY,
                            "range request starts transition");
    failures += expect_true(!fake.enabled, "transition disables bank first");
    failures += expect_true(fake.address == 2u, "transition sets requested address while disabled");
    failures += expect_true(hw_range_get_state(&range) == HW_RANGE_FSM_DEAD_TIME, "dead-time state entered");
    hw_range_step(&range, 11u);
    failures += expect_true(!fake.enabled, "dead time is observed before 2 ms");
    hw_range_step(&range, 12u);
    failures += expect_true(fake.enabled, "range enables after dead time");
    failures += expect_true(hw_range_get_state(&range) == HW_RANGE_FSM_SETTLING, "settling state entered");
    hw_range_step(&range, 16u);
    failures += expect_true(!hw_range_is_ready(&range), "settling time not ready early");
    hw_range_step(&range, 17u);
    failures += expect_true(hw_range_is_ready(&range), "range ready after settling");
    failures += expect_true(hw_range_get_current(&range) == HW_RANGE_ID_1K, "current range is committed");
    failures += expect_true(!fake.address_changed_while_enabled, "address never changed while enabled");
    return failures;
}

static int test_force_disable_from_every_state(void)
{
    int failures = 0;
    for (uint8_t state_index = 0u; state_index < 4u; state_index++)
    {
        fake_range_io_t fake = {0};
        hw_range_t range;
        hw_range_io_t io = fake_io(&fake);
        (void)hw_range_init(&range, &io);
        if (state_index == 0u)
        {
            (void)hw_range_request(&range, HW_RANGE_ID_10R, 0u);
        }
        else if (state_index == 1u)
        {
            (void)hw_range_request(&range, HW_RANGE_ID_100R, 0u);
            hw_range_step(&range, HW_RANGE_DEAD_TIME_MS);
        }
        else if (state_index == 2u)
        {
            (void)hw_range_request(&range, HW_RANGE_ID_1M, 0u);
            hw_range_step(&range, HW_RANGE_DEAD_TIME_MS);
            hw_range_step(&range, HW_RANGE_DEAD_TIME_MS + HW_RANGE_SETTLE_TIME_MS);
        }
        else
        {
            (void)hw_range_request(&range, (hw_range_id_t)7u, 0u);
        }
        hw_range_force_disabled(&range);
        failures += expect_true(!fake.enabled, "force-disable drives RANGE_EN low");
        failures += expect_true(hw_range_get_state(&range) == HW_RANGE_FSM_DISABLED, "force-disable state disabled");
        failures += expect_true(!hw_range_is_ready(&range), "force-disable clears ready");
    }
    return failures;
}

static int test_invalid_and_replacement_request(void)
{
    int failures = 0;
    fake_range_io_t fake = {0};
    hw_range_t range;
    hw_range_io_t io = fake_io(&fake);
    (void)hw_range_init(&range, &io);

    failures += expect_true(hw_range_request(&range, (hw_range_id_t)6u, 100u) == BSP_STATUS_INVALID_ARG,
                            "invalid request rejected");
    failures += expect_true(!fake.enabled, "invalid request leaves bank disabled");
    failures += expect_true(hw_range_get_state(&range) == HW_RANGE_FSM_INVALID, "invalid request marks state");

    (void)hw_range_init(&range, &io);
    failures += expect_true(hw_range_request(&range, HW_RANGE_ID_10R, 200u) == BSP_STATUS_BUSY,
                            "first transition starts");
    hw_range_step(&range, 202u);
    failures += expect_true(fake.enabled, "first request reached enabled settling");
    failures += expect_true(hw_range_request(&range, HW_RANGE_ID_100K, 203u) == BSP_STATUS_BUSY,
                            "new request during transition restarts");
    failures += expect_true(!fake.enabled, "replacement request disables bank");
    failures += expect_true(fake.address == 4u, "replacement request updates newest address after disable");
    hw_range_step(&range, 205u);
    hw_range_step(&range, 210u);
    failures += expect_true(hw_range_is_ready(&range), "replacement request reaches ready");
    failures += expect_true(hw_range_get_current(&range) == HW_RANGE_ID_100K, "newest requested range wins");
    failures += expect_true(!fake.address_changed_while_enabled, "replacement did not change address while enabled");

    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_exact_mapping();
    failures += test_transition_timing();
    failures += test_force_disable_from_every_state();
    failures += test_invalid_and_replacement_request();
    return (failures == 0) ? 0 : 1;
}
