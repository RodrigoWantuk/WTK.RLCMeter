#include "bsp/bsp_adc.h"

#include "bsp/bsp_adc_core.h"
#include "bsp/bsp_time.h"
#include "stm32f1xx.h"

static bsp_adc_core_t g_adc_core;
static bool g_adc_initialized = false;

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

static void clear_stale_conversion(void)
{
    (void)ADC1->DR;
    ADC1->SR = 0u;
}

static void enable_adc_ready(void)
{
    ADC1->CR2 |= ADC_CR2_ADON;
    delay_adc_stabilization();
    clear_stale_conversion();
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

bsp_status_t bsp_adc_init(uint32_t now_ms)
{
    bsp_adc_core_init(&g_adc_core);
    g_adc_initialized = false;

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_ADC1EN;
    gpio_config_analog(GPIOA, 1u);
    gpio_config_analog(GPIOA, 4u);
    gpio_config_analog(GPIOA, 5u);
    gpio_config_analog(GPIOA, 6u);
    gpio_config_analog(GPIOA, 7u);

    ADC1->CR1 = 0u;
    ADC1->CR2 = 0u;
    ADC1->SQR1 = 0u;
    ADC1->SQR2 = 0u;
    ADC1->SQR3 = 0u;
    ADC1->SMPR2 = (ADC_SMPR2_SMP1 |
                   ADC_SMPR2_SMP4 |
                   ADC_SMPR2_SMP5 |
                   ADC_SMPR2_SMP6 |
                   ADC_SMPR2_SMP7);
    enable_adc_ready();

    ADC1->CR2 |= ADC_CR2_RSTCAL;
    bsp_status_t status = wait_for_clear(&ADC1->CR2, ADC_CR2_RSTCAL, now_ms, BSP_ADC_CALIBRATION_TIMEOUT_MS);
    if (status != BSP_STATUS_OK)
    {
        g_adc_core.last_status = status;
        ADC1->CR2 = 0u;
        bsp_adc_core_cancel(&g_adc_core);
        return status;
    }

    ADC1->CR2 |= ADC_CR2_CAL;
    status = wait_for_clear(&ADC1->CR2, ADC_CR2_CAL, now_ms, BSP_ADC_CALIBRATION_TIMEOUT_MS);
    if (status != BSP_STATUS_OK)
    {
        g_adc_core.last_status = status;
        ADC1->CR2 = 0u;
        bsp_adc_core_cancel(&g_adc_core);
        return status;
    }

    ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL;
    clear_stale_conversion();
    g_adc_initialized = true;
    g_adc_core.last_status = BSP_STATUS_OK;
    return BSP_STATUS_OK;
}

bsp_status_t bsp_adc_start(bsp_adc_channel_t channel, uint32_t now_ms)
{
    uint8_t channel_number = 0u;
    if (!g_adc_initialized || (bsp_adc_channel_number(channel, &channel_number) != BSP_STATUS_OK))
    {
        g_adc_core.last_status = BSP_STATUS_INVALID_ARG;
        return BSP_STATUS_INVALID_ARG;
    }

    const bsp_status_t status = bsp_adc_core_start(&g_adc_core, channel, now_ms);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }

    ADC1->SQR1 = 0u;
    ADC1->SQR2 = 0u;
    ADC1->SQR3 = channel_number;
    ADC1->SR = 0u;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    return BSP_STATUS_OK;
}

bsp_status_t bsp_adc_poll(uint16_t *raw, uint32_t now_ms)
{
    uint16_t completed_raw = 0u;
    const bool complete = ((ADC1->SR & ADC_SR_EOC) != 0u);
    if (complete)
    {
        completed_raw = (uint16_t)(ADC1->DR & BSP_ADC_RAW_MAX);
    }

    const bsp_status_t status = bsp_adc_core_poll(&g_adc_core, complete, completed_raw, raw, now_ms);
    if (status == BSP_STATUS_TIMEOUT)
    {
        bsp_adc_cancel();
        g_adc_core.last_status = BSP_STATUS_TIMEOUT;
    }
    return status;
}

void bsp_adc_cancel(void)
{
    ADC1->CR2 &= ~ADC_CR2_ADON;
    ADC1->SR = 0u;
    if (g_adc_initialized)
    {
        ADC1->CR2 = ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL;
        enable_adc_ready();
    }
    bsp_adc_core_cancel(&g_adc_core);
}

bool bsp_adc_is_busy(void)
{
    return g_adc_core.state == BSP_ADC_STATE_BUSY;
}

bsp_adc_channel_t bsp_adc_current_channel(void)
{
    return g_adc_core.channel;
}

bsp_status_t bsp_adc_last_status(void)
{
    return g_adc_core.last_status;
}
