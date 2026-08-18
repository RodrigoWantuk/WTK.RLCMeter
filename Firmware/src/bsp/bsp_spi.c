#include "bsp/bsp_spi.h"

#include "bsp/bsp_clock.h"
#include "stm32f1xx.h"

enum
{
    GPIO_MODE_INPUT_FLOATING = 0x4u,
    GPIO_MODE_AF_PP_10MHZ = 0x9u,
    SPI_TIMEOUT_POLLS_PER_MS = 1000u,
    SPI_TIMEOUT_BASE_POLLS = 1000u,
};

static bool g_spi_ready = false;

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

static uint32_t spi_timeout_budget(uint32_t timeout_ms)
{
    return SPI_TIMEOUT_BASE_POLLS + (timeout_ms * SPI_TIMEOUT_POLLS_PER_MS);
}

static uint32_t spi_prescaler_bits(bsp_spi_prescaler_t prescaler)
{
    if (prescaler > BSP_SPI_PRESCALER_DIV256)
    {
        prescaler = BSP_SPI_PRESCALER_DIV256;
    }

    return ((uint32_t)prescaler << SPI_CR1_BR_Pos) & SPI_CR1_BR;
}

bsp_status_t bsp_spi2_configure(const bsp_spi_config_t *config)
{
    uint32_t cr1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | spi_prescaler_bits(BSP_SPI_PRESCALER_DIV8);

    if (config != NULL)
    {
        cr1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | spi_prescaler_bits(config->prescaler);
        if (config->cpol)
        {
            cr1 |= SPI_CR1_CPOL;
        }
        if (config->cpha)
        {
            cr1 |= SPI_CR1_CPHA;
        }
    }

    SPI2->CR1 = cr1;
    SPI2->CR2 = 0u;
    SPI2->CR1 |= SPI_CR1_SPE;

    return BSP_STATUS_OK;
}

bsp_status_t bsp_spi2_init(const bsp_spi_config_t *config)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    gpio_config_pin(GPIOB, 13u, GPIO_MODE_AF_PP_10MHZ);
    gpio_config_pin(GPIOB, 14u, GPIO_MODE_INPUT_FLOATING);
    gpio_config_pin(GPIOB, 15u, GPIO_MODE_AF_PP_10MHZ);

    (void)bsp_clock_get_summary();
    (void)bsp_spi2_configure(config);
    g_spi_ready = true;

    return BSP_STATUS_OK;
}

bsp_status_t bsp_spi2_transfer(const uint8_t *tx, uint8_t *rx, size_t length, uint32_t timeout_ms)
{
    if (((tx == NULL) && (rx == NULL) && (length > 0u)) || !g_spi_ready)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    for (size_t i = 0u; i < length; i++)
    {
        uint32_t timeout = spi_timeout_budget(timeout_ms);
        while (((SPI2->SR & SPI_SR_TXE) == 0u) && (timeout > 0u))
        {
            timeout--;
        }
        if (timeout == 0u)
        {
            return BSP_STATUS_TIMEOUT;
        }

        SPI2->DR = (tx != NULL) ? tx[i] : 0xFFu;

        timeout = spi_timeout_budget(timeout_ms);
        while (((SPI2->SR & SPI_SR_RXNE) == 0u) && (timeout > 0u))
        {
            timeout--;
        }
        if (timeout == 0u)
        {
            return BSP_STATUS_TIMEOUT;
        }

        const uint8_t received = (uint8_t)SPI2->DR;
        if (rx != NULL)
        {
            rx[i] = received;
        }
    }

    uint32_t timeout = spi_timeout_budget(timeout_ms);
    while (((SPI2->SR & SPI_SR_BSY) != 0u) && (timeout > 0u))
    {
        timeout--;
    }

    return (timeout > 0u) ? BSP_STATUS_OK : BSP_STATUS_TIMEOUT;
}
