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
static bsp_uart_rx_t g_uart_rx;

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

    if (baud_rate == 0u || clock == NULL || clock->pclk2_hz == 0u)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    const uint32_t masked = __get_PRIMASK();
    __disable_irq();
    NVIC_DisableIRQ(USART1_IRQn);
    g_uart_ready = false;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    gpio_config_pin(GPIOA, 9u, 0xAu);  /* PA9 USART1_TX, AF push-pull, 2 MHz */
    gpio_config_pin(GPIOA, 10u, 0x4u); /* PA10 USART1_RX, floating input */

    USART1->CR1 = 0u;
    bsp_uart_rx_init(&g_uart_rx);
    USART1->CR2 = 0u;
    USART1->CR3 = USART_CR3_EIE;
    USART1->BRR = (clock->pclk2_hz + (baud_rate / 2u)) / baud_rate;
    (void)USART1->SR;
    (void)USART1->DR;
    NVIC_ClearPendingIRQ(USART1_IRQn);
    NVIC_SetPriority(USART1_IRQn, 3u); /* Lower priority than both acquisition DMA channels. */
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE | USART_CR1_RXNEIE;
    NVIC_EnableIRQ(USART1_IRQn);

    g_uart_ready = true;
    __set_PRIMASK(masked);
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

bsp_status_t bsp_uart_try_read_byte(uint8_t *byte)
{
    if (byte == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }

    if (!g_uart_ready)
    {
        return BSP_STATUS_ERROR;
    }

    const uint32_t masked = __get_PRIMASK();
    __disable_irq();
    const bsp_status_t status = bsp_uart_rx_pop(&g_uart_rx, byte);
    __set_PRIMASK(masked);
    return status;
}

void USART1_IRQHandler(void)
{
    const uint32_t status = USART1->SR;
    const uint32_t errors = status & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE);
    if ((status & USART_SR_RXNE) != 0u || errors != 0u)
    {
        const uint8_t byte = (uint8_t)USART1->DR; /* SR then DR clears hardware error flags. */
        bsp_uart_rx_push(&g_uart_rx, byte, errors != 0u);
    }
}
void bsp_uart_rx_set_blocked(uint8_t reason, bool blocked)
{
    const uint32_t masked = __get_PRIMASK();
    __disable_irq();
    const uint32_t before = g_uart_rx.epoch;
    bsp_uart_rx_block(&g_uart_rx, reason, blocked);
    if (before != g_uart_rx.epoch && g_uart_ready)
    {
        (void)USART1->SR;
        (void)USART1->DR;
        NVIC_ClearPendingIRQ(USART1_IRQn);
    }
    __set_PRIMASK(masked);
}
uint32_t bsp_uart_rx_epoch(void) { return g_uart_rx.epoch; }
void bsp_uart_rx_counters(uint32_t *errors, uint32_t *overflows)
{
    const uint32_t masked = __get_PRIMASK();
    __disable_irq();
    if (errors != NULL) *errors = g_uart_rx.errors;
    if (overflows != NULL) *overflows = g_uart_rx.overflows;
    __set_PRIMASK(masked);
}

bsp_status_t bsp_uart_try_write_byte(uint8_t byte)
{
    if (!g_uart_ready) return BSP_STATUS_ERROR;
    if ((USART1->SR & USART_SR_TXE) == 0u) return BSP_STATUS_BUSY;
    USART1->DR = byte;
    return BSP_STATUS_OK;
}
