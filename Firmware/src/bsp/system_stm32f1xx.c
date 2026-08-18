#include "stm32f1xx.h"

uint32_t SystemCoreClock = 8000000u;

void SystemInit(void)
{
    RCC->CR |= RCC_CR_HSION;
    RCC->CFGR = 0u;
    RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);
    RCC->CR &= ~RCC_CR_HSEBYP;
    RCC->CIR = 0u;
    SystemCoreClock = 8000000u;
}

void SystemCoreClockUpdate(void)
{
    if ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL)
    {
        SystemCoreClock = 72000000u;
    }
    else
    {
        SystemCoreClock = 8000000u;
    }
}
