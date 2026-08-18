#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_ll_adc.h"
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_dma.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_iwdg.h"
#include "stm32f1xx_ll_rcc.h"
#include "stm32f1xx_ll_spi.h"
#include "stm32f1xx_ll_system.h"
#include "stm32f1xx_ll_tim.h"
#include "stm32f1xx_ll_usart.h"

#ifndef STM32F103xB
#error "WTK.RLCMeter Rev.1 requires the STM32F103xB CMSIS device definition."
#endif

#ifndef USE_HAL_DRIVER
#error "STM32 HAL/LL headers must be configured for the HAL driver boundary."
#endif

void wtk_stm32_dependency_probe(void)
{
    (void)GPIOA;
    (void)ADC1;
    (void)DMA1;
    (void)SPI2;
    (void)TIM1;
    (void)USART1;
    (void)IWDG;
}
