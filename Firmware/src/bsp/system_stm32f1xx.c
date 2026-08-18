#include <stdint.h>

uint32_t SystemCoreClock = 8000000u;

void SystemInit(void)
{
}

void SystemCoreClockUpdate(void)
{
    SystemCoreClock = 8000000u;
}
