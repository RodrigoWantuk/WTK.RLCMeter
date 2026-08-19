#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f1xx.h"

enum
{
    GPIO_MODE_INPUT_FLOATING = 0x4u,
    GPIO_MODE_OUTPUT_PP_10MHZ = 0x1u,
    GPIO_MODE_AF_PP_10MHZ = 0x9u,
    GPIO_MODE_AF_PP_2MHZ = 0xAu,
    SPI_TIMEOUT_POLLS = 200000u,
    UART_TX_TIMEOUT_POLLS = 200000u,
    UART_BAUD = 115200u,
    HSI_HZ = 8000000u,
};

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

static void uart_write(const char *text)
{
    for (size_t i = 0u; text[i] != '\0'; i++)
    {
        uint32_t timeout = UART_TX_TIMEOUT_POLLS;
        while (((USART1->SR & USART_SR_TXE) == 0u) && (timeout > 0u))
        {
            timeout--;
        }
        USART1->DR = (uint32_t)(uint8_t)text[i];
    }
}

static void uart_write_hex8(uint8_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[3];
    text[0] = digits[(value >> 4u) & 0x0Fu];
    text[1] = digits[value & 0x0Fu];
    text[2] = '\0';
    uart_write(text);
}

static void uart_write_hex16(uint16_t value)
{
    uart_write_hex8((uint8_t)(value >> 8u));
    uart_write_hex8((uint8_t)value);
}

static void spi_transfer(SPI_TypeDef *spi, const uint8_t *tx, uint16_t *rx16, uint8_t *rx8, size_t length)
{
    for (size_t i = 0u; i < length; i++)
    {
        uint32_t timeout = SPI_TIMEOUT_POLLS;
        while (((spi->SR & SPI_SR_TXE) == 0u) && (timeout > 0u))
        {
            timeout--;
        }
        spi->DR = tx[i];
        timeout = SPI_TIMEOUT_POLLS;
        while (((spi->SR & SPI_SR_RXNE) == 0u) && (timeout > 0u))
        {
            timeout--;
        }
        const uint16_t raw = (uint16_t)spi->DR;
        rx16[i] = raw;
        rx8[i] = (uint8_t)raw;
    }
    uint32_t timeout = SPI_TIMEOUT_POLLS;
    while (((spi->SR & SPI_SR_BSY) != 0u) && (timeout > 0u))
    {
        timeout--;
    }
}

static void report_rx(const char *label, const uint8_t *tx, const uint16_t *rx16, const uint8_t *rx8, size_t length)
{
    uart_write(label);
    uart_write(" TX:");
    for (size_t i = 0u; i < length; i++)
    {
        uart_write(" ");
        uart_write_hex8(tx[i]);
    }
    uart_write("\r\n");
    uart_write(label);
    uart_write(" RX8:");
    for (size_t i = 0u; i < length; i++)
    {
        uart_write(" ");
        uart_write_hex8(rx8[i]);
    }
    uart_write("\r\n");
    uart_write(label);
    uart_write(" DR16:");
    for (size_t i = 0u; i < length; i++)
    {
        uart_write(" ");
        uart_write_hex16(rx16[i]);
    }
    uart_write("\r\n");
}

int main(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN |
                    RCC_APB2ENR_SPI1EN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    gpio_config_pin(GPIOA, 9u, GPIO_MODE_AF_PP_2MHZ);
    gpio_config_pin(GPIOA, 10u, GPIO_MODE_INPUT_FLOATING);
    USART1->BRR = (HSI_HZ + (UART_BAUD / 2u)) / UART_BAUD;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    uart_write("spi2-rx-repro\r\n");

    gpio_config_pin(GPIOB, 13u, GPIO_MODE_AF_PP_10MHZ);
    gpio_config_pin(GPIOB, 14u, GPIO_MODE_INPUT_FLOATING);
    gpio_config_pin(GPIOB, 15u, GPIO_MODE_AF_PP_10MHZ);
    gpio_config_pin(GPIOA, 12u, GPIO_MODE_OUTPUT_PP_10MHZ);
    GPIOA->BSRR = (1u << 12u);

    SPI2->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (2u << SPI_CR1_BR_Pos);
    SPI2->CR2 = 0u;
    SPI2->CR1 |= SPI_CR1_SPE;

    const uint8_t tx[4] = {0x00u, 0x00u, 0x00u, 0x00u};
    uint16_t rx16[4] = {0};
    uint8_t rx8[4] = {0};
    GPIOA->BRR = (1u << 12u);
    spi_transfer(SPI2, tx, rx16, rx8, 4u);
    GPIOA->BSRR = (1u << 12u);
    report_rx("SPI2", tx, rx16, rx8, 4u);

    const bool spi2_zero = (rx8[0] | rx8[1] | rx8[2] | rx8[3]) == 0u;
    if (spi2_zero)
    {
        uart_write("SPI2 RX all-zero; running SPI1 comparison\r\n");
        gpio_config_pin(GPIOA, 5u, GPIO_MODE_AF_PP_10MHZ);
        gpio_config_pin(GPIOA, 6u, GPIO_MODE_INPUT_FLOATING);
        gpio_config_pin(GPIOA, 7u, GPIO_MODE_AF_PP_10MHZ);
        gpio_config_pin(GPIOA, 4u, GPIO_MODE_OUTPUT_PP_10MHZ);
        GPIOA->BSRR = (1u << 4u);
        SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (2u << SPI_CR1_BR_Pos);
        SPI1->CR2 = 0u;
        SPI1->CR1 |= SPI_CR1_SPE;
        uint16_t spi1_rx16[4] = {0};
        uint8_t spi1_rx8[4] = {0};
        GPIOA->BRR = (1u << 4u);
        spi_transfer(SPI1, tx, spi1_rx16, spi1_rx8, 4u);
        GPIOA->BSRR = (1u << 4u);
        report_rx("SPI1", tx, spi1_rx16, spi1_rx8, 4u);
    }

    uart_write("done\r\n");
    for (;;)
    {
    }
}
