#include "bsp/bsp_uart.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_clock.h"
#include "stm32f1xx.h"

enum
{
    UART_TX_TIMEOUT_POLLS = 200000u,
};

static bool g_uart_ready = false;

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

static size_t cstr_length(const char *text)
{
    size_t length = 0u;

    if (text == NULL)
    {
        return 0u;
    }

    while (text[length] != '\0')
    {
        length++;
    }

    return length;
}

bsp_status_t bsp_uart_init(uint32_t baud_rate)
{
    const bsp_clock_summary_t *const clock = bsp_clock_get_summary();

    if (baud_rate == 0u)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    gpio_config_pin(GPIOA, 9u, 0xAu);  /* PA9 USART1_TX, AF push-pull, 2 MHz */
    gpio_config_pin(GPIOA, 10u, 0x4u); /* PA10 USART1_RX, floating input */

    USART1->CR1 = 0u;
    USART1->CR2 = 0u;
    USART1->CR3 = 0u;
    USART1->BRR = (clock->pclk2_hz + (baud_rate / 2u)) / baud_rate;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    g_uart_ready = true;
    return BSP_STATUS_OK;
}

bsp_status_t bsp_uart_write(const char *data, size_t length)
{
    if ((data == NULL) && (length > 0u))
    {
        return BSP_STATUS_INVALID_ARG;
    }

    if (!g_uart_ready)
    {
        return BSP_STATUS_ERROR;
    }

    for (size_t i = 0u; i < length; i++)
    {
        uint32_t timeout = UART_TX_TIMEOUT_POLLS;
        while (((USART1->SR & USART_SR_TXE) == 0u) && (timeout > 0u))
        {
            timeout--;
        }

        if (timeout == 0u)
        {
            return BSP_STATUS_TIMEOUT;
        }

        USART1->DR = (uint32_t)(uint8_t)data[i];
    }

    return BSP_STATUS_OK;
}

bsp_status_t bsp_uart_write_cstr(const char *text)
{
    return bsp_uart_write(text, cstr_length(text));
}
