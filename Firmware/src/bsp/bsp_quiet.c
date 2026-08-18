#include "bsp/bsp_quiet.h"

static bool g_quiet_requested = false;

void bsp_quiet_request(bool requested)
{
    g_quiet_requested = requested;
}

bool bsp_quiet_requested(void)
{
    return g_quiet_requested;
}
