#include <assert.h>
#include "bsp/bsp_uart_rx.h"
int main(void)
{
    bsp_uart_rx_t rx;
    uint8_t byte = 0u;
    bsp_uart_rx_init(&rx);
    for (unsigned run = 0u; run < 10u; ++run)
    {
        for (unsigned i = 0u; i < 200u; ++i) bsp_uart_rx_push(&rx, (uint8_t)i, false);
        for (unsigned i = 0u; i < 200u; ++i)
            assert(bsp_uart_rx_pop(&rx, &byte) == BSP_STATUS_OK && byte == (uint8_t)i);
        assert(bsp_uart_rx_pop(&rx, &byte) == BSP_STATUS_BUSY);
    }
    for (unsigned i = 0u; i < 256u; ++i) bsp_uart_rx_push(&rx, (uint8_t)i, false);
    assert(rx.overflows == 1u && rx.epoch == 1u);
    assert(bsp_uart_rx_pop(&rx, &byte) == BSP_STATUS_ERROR);
    assert(bsp_uart_rx_pop(&rx, &byte) == BSP_STATUS_BUSY);
    bsp_uart_rx_push(&rx, 0x42u, false);
    assert(bsp_uart_rx_pop(&rx, &byte) == BSP_STATUS_OK && byte == 0x42u);
    bsp_uart_rx_push(&rx, 1u, false);
    bsp_uart_rx_push(&rx, 2u, true);
    bsp_uart_rx_push(&rx, 3u, false);
    assert(bsp_uart_rx_pop(&rx, &byte) == BSP_STATUS_ERROR && rx.errors == 1u);
    bsp_uart_rx_push(&rx, 4u, false);
    const uint32_t epoch = rx.epoch;
    bsp_uart_rx_block(&rx, BSP_UART_RX_APPLICATION, true);
    assert(rx.epoch != epoch && bsp_uart_rx_pop(&rx, &byte) == BSP_STATUS_BUSY);
    bsp_uart_rx_block(&rx, BSP_UART_RX_QUIET, true);
    bsp_uart_rx_block(&rx, BSP_UART_RX_QUIET, false);
    for (unsigned i = 0u; i < 28u; ++i) bsp_uart_rx_push(&rx, (uint8_t)i, false);
    assert(bsp_uart_rx_pop(&rx, &byte) == BSP_STATUS_BUSY);
    bsp_uart_rx_block(&rx, BSP_UART_RX_APPLICATION, false);
    assert(bsp_uart_rx_pop(&rx, &byte) == BSP_STATUS_BUSY); /* Busy-time request cannot replay. */
    bsp_uart_rx_push(&rx, 0x50u, false);
    assert(bsp_uart_rx_pop(&rx, &byte) == BSP_STATUS_OK && byte == 0x50u);
    return 0;
}
