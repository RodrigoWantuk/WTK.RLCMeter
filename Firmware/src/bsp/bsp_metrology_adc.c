#include "bsp/bsp_metrology_adc.h"

#include "bsp/bsp_adc.h"
#include "bsp/bsp_time.h"
#include "stm32f1xx.h"

extern uint32_t SystemCoreClock;

enum
{
    METROLOGY_SAMPLE_SMP_7P5 = 1u, /* SMP=001 -> 7.5 ADC cycles */
};

static volatile bool g_dma_complete = false;
static volatile bool g_dma_error = false;
static bool g_acquired = false;

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (now_ms - deadline_ms) < 0x80000000u;
}

static void delay_adc_stabilization(void)
{
    uint32_t cycles_per_us = SystemCoreClock / 1000000u;
    if (cycles_per_us == 0u)
    {
        cycles_per_us = 1u;
    }
    for (uint32_t us = 0u; us < BSP_ADC_POWER_STABILIZATION_US; us++)
    {
        for (uint32_t cycle = 0u; cycle < cycles_per_us; cycle++)
        {
            __NOP();
        }
    }
}

static bsp_status_t wait_for_clear(volatile uint32_t *reg, uint32_t mask, uint32_t now_ms, uint32_t timeout_ms)
{
    const uint32_t deadline_ms = now_ms + timeout_ms;
    while ((*reg & mask) != 0u)
    {
        if (deadline_reached(bsp_time_now_ms(), deadline_ms))
        {
            return BSP_STATUS_TIMEOUT;
        }
    }
    return BSP_STATUS_OK;
}

static void gpio_config_analog(GPIO_TypeDef *const port, uint32_t pin)
{
    volatile uint32_t *reg = &port->CRL;
    uint32_t shift = pin * 4u;
    if (pin >= 8u)
    {
        reg = &port->CRH;
        shift = (pin - 8u) * 4u;
    }
    *reg &= ~(0xFu << shift);
}

static bsp_status_t calibrate_adc(ADC_TypeDef *adc, uint32_t now_ms)
{
    adc->CR2 |= ADC_CR2_ADON;
    delay_adc_stabilization();
    (void)adc->DR;
    adc->SR = 0u;
    adc->CR2 |= ADC_CR2_RSTCAL;
    bsp_status_t status = wait_for_clear(&adc->CR2, ADC_CR2_RSTCAL, now_ms, BSP_ADC_CALIBRATION_TIMEOUT_MS);
    if (status != BSP_STATUS_OK)
    {
        adc->CR2 = 0u;
        return status;
    }
    adc->CR2 |= ADC_CR2_CAL;
    status = wait_for_clear(&adc->CR2, ADC_CR2_CAL, now_ms, BSP_ADC_CALIBRATION_TIMEOUT_MS);
    if (status != BSP_STATUS_OK)
    {
        adc->CR2 = 0u;
    }
    return status;
}

static void stop_tim2(void)
{
    TIM2->CR1 &= ~TIM_CR1_CEN;
    TIM2->DIER = 0u;
    TIM2->CCER &= ~TIM_CCER_CC2E; /* never drive PA1 */
}

static void disable_adc_dma(void)
{
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
    ADC1->CR2 &= ~ADC_CR2_DMA;
    DMA1->IFCR = DMA_IFCR_CGIF1 | DMA_IFCR_CTCIF1 | DMA_IFCR_CTEIF1 | DMA_IFCR_CHTIF1;
}

void bsp_metrology_adc_stop(void)
{
    stop_tim2();
    disable_adc_dma();
}

bool bsp_metrology_adc_dma_complete(void)
{
    return g_dma_complete;
}

bool bsp_metrology_adc_dma_error(void)
{
    return g_dma_error;
}

bsp_status_t bsp_metrology_adc_acquire(uint32_t now_ms)
{
    g_dma_complete = false;
    g_dma_error = false;
    g_acquired = false;
    bsp_metrology_adc_stop();

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    /* PA1 remains analog ADC_VMID. TIM2 must stay on default mapping and CC2 GPIO off. */
    AFIO->MAPR &= ~AFIO_MAPR_TIM2_REMAP;
    gpio_config_analog(GPIOA, 0u);
    gpio_config_analog(GPIOA, 1u);
    gpio_config_analog(GPIOA, 2u);
    gpio_config_analog(GPIOA, 3u);

    ADC1->CR1 = 0u;
    ADC1->CR2 = 0u;
    ADC2->CR1 = 0u;
    ADC2->CR2 = 0u;

    bsp_status_t status = calibrate_adc(ADC1, now_ms);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    status = calibrate_adc(ADC2, now_ms);
    if (status != BSP_STATUS_OK)
    {
        ADC1->CR2 = 0u;
        return status;
    }

    const uint32_t smp_7p5 = (METROLOGY_SAMPLE_SMP_7P5 << ADC_SMPR2_SMP0_Pos) |
                             (METROLOGY_SAMPLE_SMP_7P5 << ADC_SMPR2_SMP1_Pos) |
                             (METROLOGY_SAMPLE_SMP_7P5 << ADC_SMPR2_SMP2_Pos) |
                             (METROLOGY_SAMPLE_SMP_7P5 << ADC_SMPR2_SMP3_Pos);
    ADC1->SMPR1 = 0u;
    ADC1->SMPR2 = smp_7p5;
    ADC2->SMPR1 = 0u;
    ADC2->SMPR2 = smp_7p5;

    /* 3 ranks: L = N-1 = 2 */
    ADC1->SQR1 = ADC_SQR1_L_1;
    ADC1->SQR2 = 0u;
    ADC1->SQR3 = (0u << ADC_SQR3_SQ1_Pos) | (0u << ADC_SQR3_SQ2_Pos) | (1u << ADC_SQR3_SQ3_Pos);

    ADC2->SQR1 = ADC_SQR1_L_1;
    ADC2->SQR2 = 0u;
    ADC2->SQR3 = (2u << ADC_SQR3_SQ1_Pos) | (3u << ADC_SQR3_SQ2_Pos) | (1u << ADC_SQR3_SQ3_Pos);

    ADC1->CR1 = ADC_CR1_SCAN | ADC_CR1_DUALMOD_2 | ADC_CR1_DUALMOD_1;
    ADC2->CR1 = ADC_CR1_SCAN;

    /* TIM2_CC2 external trigger, 12-bit right aligned, dual DMA from ADC1->DR */
    ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL_0 | ADC_CR2_EXTSEL_1;
    ADC2->CR2 = ADC_CR2_ADON;
    delay_adc_stabilization();

    NVIC_SetPriority(DMA1_Channel1_IRQn, 1u);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    g_acquired = true;
    return BSP_STATUS_OK;
}

bsp_status_t bsp_metrology_adc_start_capture(uint32_t *raw_words,
                                             uint32_t word_count,
                                             uint16_t tim2_arr,
                                             uint16_t tim2_ccr2)
{
    if (!g_acquired || (raw_words == NULL) || (word_count != BSP_METROLOGY_RAW_WORD_COUNT) ||
        (tim2_ccr2 > tim2_arr))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    g_dma_complete = false;
    g_dma_error = false;
    bsp_metrology_adc_stop();

    DMA1_Channel1->CCR = 0u;
    DMA1_Channel1->CNDTR = word_count;
    DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR;
    DMA1_Channel1->CMAR = (uint32_t)raw_words;
    DMA1_Channel1->CCR = DMA_CCR_MINC | DMA_CCR_PSIZE_1 | DMA_CCR_MSIZE_1 | DMA_CCR_TCIE |
                         DMA_CCR_TEIE | DMA_CCR_PL_0 | DMA_CCR_PL_1 | DMA_CCR_EN;

    ADC1->SR = 0u;
    ADC2->SR = 0u;
    ADC1->CR2 |= ADC_CR2_DMA;

    /*
     * TIM2_CH2 default pin is PA1 (ADC_VMID). CC2 is used only as an internal
     * compare event. CC2E stays 0 and TIM2 is not remapped.
     */
    TIM2->CR1 = 0u;
    TIM2->DIER = 0u;
    TIM2->PSC = 0u;
    TIM2->ARR = tim2_arr;
    TIM2->CCR2 = tim2_ccr2;
    TIM2->CCMR1 = TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2PE;
    TIM2->CCER = 0u;
    TIM2->CNT = 0u;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
    return BSP_STATUS_OK;
}

bsp_status_t bsp_metrology_adc_restore(uint32_t now_ms)
{
    bsp_metrology_adc_stop();
    ADC1->CR1 = 0u;
    ADC1->CR2 = 0u;
    ADC2->CR1 = 0u;
    ADC2->CR2 = 0u;
    g_acquired = false;

    gpio_config_analog(GPIOA, 1u);
    AFIO->MAPR &= ~AFIO_MAPR_TIM2_REMAP;

    const bsp_status_t status = bsp_adc_init(now_ms);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    return BSP_STATUS_OK;
}

void DMA1_Channel1_IRQHandler(void)
{
    if ((DMA1->ISR & DMA_ISR_TEIF1) != 0u)
    {
        g_dma_error = true;
        g_dma_complete = false;
        stop_tim2();
        disable_adc_dma();
        DMA1->IFCR = DMA_IFCR_CTEIF1 | DMA_IFCR_CGIF1;
        return;
    }
    if ((DMA1->ISR & DMA_ISR_TCIF1) != 0u)
    {
        g_dma_complete = true;
        g_dma_error = false;
        stop_tim2();
        disable_adc_dma();
        DMA1->IFCR = DMA_IFCR_CTCIF1 | DMA_IFCR_CGIF1;
    }
}
