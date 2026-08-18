#include "hardware/hw_buzzer_policy.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int test_quiet_stops_active_tone(void)
{
    hw_buzzer_policy_t policy;
    hw_buzzer_policy_init(&policy);

    int failures = 0;
    failures += expect_true(hw_buzzer_policy_play(&policy, 1000u, 200u, 10u) == BSP_STATUS_OK,
                            "tone starts before quiet mode");
    failures += expect_true(hw_buzzer_policy_is_active(&policy), "tone is active before quiet mode");
    failures += expect_true(hw_buzzer_policy_set_quiet(&policy, true), "quiet request reports stopped tone");
    failures += expect_true(!hw_buzzer_policy_is_active(&policy), "quiet request stops active tone");

    return failures;
}

static int test_quiet_blocks_new_tones(void)
{
    hw_buzzer_policy_t policy;
    hw_buzzer_policy_init(&policy);
    (void)hw_buzzer_policy_set_quiet(&policy, true);

    int failures = 0;
    failures += expect_true(hw_buzzer_policy_play(&policy, 1200u, 100u, 50u) == BSP_STATUS_BUSY,
                            "quiet mode returns BUSY for new tone");
    failures += expect_true(!hw_buzzer_policy_is_active(&policy), "quiet mode keeps buzzer inactive");

    return failures;
}

static int test_leaving_quiet_does_not_replay_tone(void)
{
    hw_buzzer_policy_t policy;
    hw_buzzer_policy_init(&policy);

    int failures = 0;
    failures += expect_true(hw_buzzer_policy_play(&policy, 1800u, 500u, 100u) == BSP_STATUS_OK,
                            "tone starts before quiet entry");
    (void)hw_buzzer_policy_set_quiet(&policy, true);
    failures += expect_true(!hw_buzzer_policy_set_quiet(&policy, false),
                            "leaving quiet does not report another stop");
    failures += expect_true(!hw_buzzer_policy_is_active(&policy), "leaving quiet does not replay interrupted tone");

    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_quiet_stops_active_tone();
    failures += test_quiet_blocks_new_tones();
    failures += test_leaving_quiet_does_not_replay_tone();

    return (failures == 0) ? 0 : 1;
}
