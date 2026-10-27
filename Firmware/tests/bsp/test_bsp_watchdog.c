/* Execute the actual BSP against a deterministic register-update model.
 * Synthetic ordering/timeout evidence; no watchdog timing qualification. */
#include <stdio.h>
#include <string.h>
#include "stm32f1xx.h"
#include "bsp/bsp_watchdog.h"

static wtk_test_rcc_t rcc;
static wtk_test_iwdg_t iwdg;
static bool fail_lsi;
static bool fail_update;
static bool enabled;
static bool configuration_before_enable;
static uint32_t previous_key;
static unsigned rcc_reads;
static unsigned iwdg_reads;

wtk_test_rcc_t *wtk_test_rcc(void)
{
    ++rcc_reads;
    if (!fail_lsi && ((rcc.CSR & RCC_CSR_LSION) != 0u))
    {
        rcc.CSR |= RCC_CSR_LSIRDY;
    }
    return &rcc;
}

wtk_test_iwdg_t *wtk_test_iwdg(void)
{
    ++iwdg_reads;
    if (iwdg.KR != previous_key)
    {
        previous_key = iwdg.KR;
        if (iwdg.KR == UINT32_C(0xCCCC)) enabled = true;
        if ((iwdg.KR == UINT32_C(0x5555)) && !enabled)
            configuration_before_enable = true;
    }
    const bool configured = (iwdg.PR == 4u) && (iwdg.RLR == 1250u);
    iwdg.SR = configured && enabled && !fail_update ? 0u : 3u;
    return &iwdg;
}

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "Failed at line %u: %s\n", (unsigned)__LINE__, #condition); \
    return 1; } } while (0)

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    fail_lsi = strcmp(argv[1], "lsi-timeout") == 0;
    fail_update = strcmp(argv[1], "update-timeout") == 0;
    CHECK(fail_lsi || fail_update || (strcmp(argv[1], "success") == 0));
    CHECK(!bsp_watchdog_is_started());
    bsp_watchdog_service();
    CHECK(iwdg_reads == 0u);
    const bsp_status_t status = bsp_watchdog_start();
    CHECK(rcc_reads < 200010u && iwdg_reads < 200010u);
    CHECK(!configuration_before_enable);
    if (fail_lsi)
    {
        CHECK(status == BSP_STATUS_TIMEOUT && !enabled);
        CHECK(!bsp_watchdog_is_started() && iwdg_reads == 0u);
    }
    else if (fail_update)
    {
        CHECK(status == BSP_STATUS_TIMEOUT && enabled);
        CHECK(!bsp_watchdog_is_started());
        iwdg.KR = 0u;
        bsp_watchdog_service();
        CHECK(iwdg.KR == 0u); /* Failed initialization cannot become a normal kick. */
    }
    else
    {
        CHECK(status == BSP_STATUS_OK && enabled && bsp_watchdog_is_started());
        CHECK(iwdg.PR == 4u && iwdg.RLR == 1250u && iwdg.KR == UINT32_C(0xAAAA));
        iwdg.KR = 0u;
        bsp_watchdog_service();
        CHECK(iwdg.KR == UINT32_C(0xAAAA));
    }
    return 0;
}
