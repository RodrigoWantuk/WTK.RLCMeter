#include "bsp/bsp_timers.h"

#include <stdbool.h>

#include "bsp/bsp_clock.h"
#include "bsp/bsp_gpio.h"
#include "stm32f1xx.h"

enum
{
    GPIO_MODE_AF_PP_2MHZ = 0xAu,
    TIMER_BASE_HZ = 1000000u,
};

static uint16_t g_tim3_ch3_steps = 0u;
static bool g_buzzer_pin_high = false;

static void gpio_config_pin(GPIO_TypeDef *const port, uint32_t pin, uint32_t mode)
{
    volatile uint32_t *reg = &port->CRL;
    uint32_t shift = pin * 4u;

    if (pin >= 8u)
    {
        reg = &port->CRH;
        shift = (pin - 8u) * 4u;
    }

    *reg = (*reg & ~(0xFu << shift)) | ((mode & 0xFu) << shift);
}

static bsp_status_t set_timer_prescaler(TIM_TypeDef *timer, uint32_t timer_hz)
{
    const bsp_clock_summary_t *const clock = bsp_clock_get_summary();

    if ((timer_hz == 0u) || (clock->tim_apb1_hz < timer_hz))
    {
        return BSP_STATUS_ERROR;
    }

    timer->PSC = (uint16_t)((clock->tim_apb1_hz / timer_hz) - 1u);
    return BSP_STATUS_OK;
}

bsp_status_t bsp_timer3_pwm_ch3_init(uint32_t pwm_hz, uint16_t steps)
{
    if ((pwm_hz == 0u) || (steps == 0u))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    gpio_config_pin(GPIOB, 0u, GPIO_MODE_AF_PP_2MHZ);

    TIM3->CR1 = 0u;
    const bsp_status_t status = set_timer_prescaler(TIM3, (uint32_t)pwm_hz * (uint32_t)steps);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }

    TIM3->ARR = (uint16_t)(steps - 1u);
    TIM3->CCR3 = 0u;
    TIM3->CCMR2 = (TIM3->CCMR2 & ~(TIM_CCMR2_OC3M | TIM_CCMR2_CC3S)) |
                  TIM_CCMR2_OC3M_1 |
                  TIM_CCMR2_OC3M_2 |
                  TIM_CCMR2_OC3PE;
    TIM3->CCER |= TIM_CCER_CC3E;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
    g_tim3_ch3_steps = steps;

    return BSP_STATUS_OK;
}

bsp_status_t bsp_timer3_pwm_ch3_set_duty(uint16_t duty_steps)
{
    if (g_tim3_ch3_steps == 0u)
    {
        return BSP_STATUS_ERROR;
    }

    if (duty_steps > g_tim3_ch3_steps)
    {
        duty_steps = g_tim3_ch3_steps;
    }

    TIM3->CCR3 = duty_steps;
    return BSP_STATUS_OK;
}

bsp_status_t bsp_timer4_buzzer_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    TIM4->CR1 = 0u;
    const bsp_status_t status = set_timer_prescaler(TIM4, TIMER_BASE_HZ);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }

    TIM4->ARR = 999u;
    TIM4->EGR = TIM_EGR_UG;
    NVIC_EnableIRQ(TIM4_IRQn);
    bsp_timer4_buzzer_stop();
    return BSP_STATUS_OK;
}

bsp_status_t bsp_timer4_buzzer_start(uint16_t frequency_hz)
{
    if (frequency_hz == 0u)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    const uint32_t half_period_ticks = TIMER_BASE_HZ / ((uint32_t)frequency_hz * 2u);
    if ((half_period_ticks == 0u) || (half_period_ticks > 65536u))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    TIM4->CR1 = 0u;
    TIM4->ARR = (uint16_t)(half_period_ticks - 1u);
    TIM4->CNT = 0u;
    TIM4->SR = 0u;
    TIM4->DIER = TIM_DIER_UIE;
    TIM4->EGR = TIM_EGR_UG;
    TIM4->CR1 = TIM_CR1_CEN;

    return BSP_STATUS_OK;
}

void bsp_timer4_buzzer_stop(void)
{
    TIM4->CR1 = 0u;
    TIM4->DIER = 0u;
    (void)bsp_gpio_write_output(BSP_GPIO_OUTPUT_BUZZER, false);
    g_buzzer_pin_high = false;
}

void TIM4_IRQHandler(void)
{
    if ((TIM4->SR & TIM_SR_UIF) != 0u)
    {
        TIM4->SR &= ~TIM_SR_UIF;
        g_buzzer_pin_high = !g_buzzer_pin_high;
        (void)bsp_gpio_write_output(BSP_GPIO_OUTPUT_BUZZER, g_buzzer_pin_high);
    }
}
