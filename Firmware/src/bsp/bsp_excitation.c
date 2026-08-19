#include "bsp/bsp_excitation.h"

#include <stddef.h>

#include "stm32f1xx.h"

enum
{
    GPIO_MODE_OUTPUT_PP_2MHZ = 0x2u,
    GPIO_MODE_AF_PP_50MHZ = 0xBu,
    EXC_PWM_PSC = 0u,
    EXC_PWM_ARR = 159u,
    EXC_PWM_CENTER = 80u,
    EXC_LUT_POINTS = 45u,
};

static uint16_t g_ccr_table[EXC_LUT_POINTS];
static bsp_excitation_mode_t g_mode = BSP_EXCITATION_MODE_OFF;
static volatile bool g_dma_error = false;

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

static void force_pa8_low(void)
{
    GPIOA->BSRR = (1u << (8u + 16u));
    gpio_config_pin(GPIOA, 8u, GPIO_MODE_OUTPUT_PP_2MHZ);
}

static void disable_excitation_dma(void)
{
    DMA1_Channel5->CCR &= ~DMA_CCR_EN;
    TIM1->DIER &= ~TIM_DIER_UDE;
    DMA1->IFCR = DMA_IFCR_CGIF5 | DMA_IFCR_CTEIF5 | DMA_IFCR_CTCIF5 | DMA_IFCR_CHTIF5;
}

static void stop_tim1_waveform(void)
{
    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->BDTR &= ~TIM_BDTR_MOE;
    TIM1->CCER &= ~TIM_CCER_CC1E;
    TIM1->CNT = 0u;
}

bsp_status_t bsp_excitation_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_TIM1EN | RCC_APB2ENR_AFIOEN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    disable_excitation_dma();
    stop_tim1_waveform();
    force_pa8_low();
    g_mode = BSP_EXCITATION_MODE_OFF;
    g_dma_error = false;

    NVIC_SetPriority(DMA1_Channel5_IRQn, 2u);
    NVIC_EnableIRQ(DMA1_Channel5_IRQn);
    return BSP_STATUS_OK;
}

bsp_status_t bsp_excitation_off(void)
{
    disable_excitation_dma();
    stop_tim1_waveform();
    force_pa8_low();
    g_mode = BSP_EXCITATION_MODE_OFF;
    return BSP_STATUS_OK;
}

static void configure_tim1_pwm(void)
{
    TIM1->CR1 = 0u;
    TIM1->PSC = EXC_PWM_PSC;
    TIM1->ARR = EXC_PWM_ARR;
    TIM1->CCR1 = EXC_PWM_CENTER;
    TIM1->CCMR1 = (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE);
    TIM1->CCER = TIM_CCER_CC1E;
    TIM1->RCR = 0u;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->BDTR = TIM_BDTR_MOE;
    TIM1->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

bsp_status_t bsp_excitation_neutral(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_TIM1EN | RCC_APB2ENR_AFIOEN;
    disable_excitation_dma();
    g_dma_error = false;
    gpio_config_pin(GPIOA, 8u, GPIO_MODE_AF_PP_50MHZ);
    configure_tim1_pwm();
    TIM1->CCR1 = EXC_PWM_CENTER;
    g_mode = BSP_EXCITATION_MODE_NEUTRAL;
    return BSP_STATUS_OK;
}

bsp_status_t bsp_excitation_sine(uint8_t rcr, const uint16_t *ccr, uint32_t count)
{
    if ((ccr == NULL) || (count != EXC_LUT_POINTS))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (g_mode != BSP_EXCITATION_MODE_NEUTRAL)
    {
        return BSP_STATUS_ERROR;
    }

    for (uint32_t i = 0u; i < EXC_LUT_POINTS; i++)
    {
        g_ccr_table[i] = ccr[i];
    }

    disable_excitation_dma();
    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->DIER &= ~TIM_DIER_UDE;
    TIM1->PSC = EXC_PWM_PSC;
    TIM1->ARR = EXC_PWM_ARR;
    TIM1->RCR = rcr;
    TIM1->CNT = 0u;
    TIM1->CCR1 = g_ccr_table[0];
    TIM1->EGR = TIM_EGR_UG;

    DMA1_Channel5->CCR = 0u;
    DMA1_Channel5->CNDTR = EXC_LUT_POINTS;
    DMA1_Channel5->CPAR = (uint32_t)&TIM1->CCR1;
    DMA1_Channel5->CMAR = (uint32_t)g_ccr_table;
    DMA1_Channel5->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0 |
                         DMA_CCR_CIRC | DMA_CCR_PL_1 | DMA_CCR_TEIE | DMA_CCR_EN;
    TIM1->DIER |= TIM_DIER_UDE;
    TIM1->BDTR = TIM_BDTR_MOE;
    TIM1->CCER |= TIM_CCER_CC1E;
    TIM1->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
    g_mode = BSP_EXCITATION_MODE_SINE;
    g_dma_error = false;
    return BSP_STATUS_OK;
}

bsp_excitation_mode_t bsp_excitation_mode(void)
{
    return g_mode;
}

bool bsp_excitation_dma_error(void)
{
    return g_dma_error;
}

void DMA1_Channel5_IRQHandler(void)
{
    if ((DMA1->ISR & DMA_ISR_TEIF5) != 0u)
    {
        g_dma_error = true;
        disable_excitation_dma();
        stop_tim1_waveform();
        force_pa8_low();
        g_mode = BSP_EXCITATION_MODE_OFF;
        DMA1->IFCR = DMA_IFCR_CTEIF5 | DMA_IFCR_CGIF5;
    }
}
