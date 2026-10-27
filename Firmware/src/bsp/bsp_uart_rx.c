#include "bsp/bsp_uart_rx.h"
#include <stddef.h>
void bsp_uart_rx_init(bsp_uart_rx_t *rx)
{
    if (rx != NULL) *rx = (bsp_uart_rx_t){0};
}
void bsp_uart_rx_push(bsp_uart_rx_t *rx, uint8_t byte, bool error)
{
    if (rx == NULL) return;
    if (error)
    {
        ++rx->errors;
        if (!rx->poisoned) ++rx->epoch;
        rx->poisoned = true;
    }
    if (rx->blocked != 0u || rx->poisoned) { ++rx->discarded; return; }
    const uint16_t next = (uint16_t)((rx->head + 1u) & (BSP_UART_RX_CAPACITY - 1u));
    if (next == rx->tail)
    {
        ++rx->overflows;
        ++rx->epoch;
        rx->poisoned = true;
        return;
    }
    rx->data[rx->head] = byte;
    rx->head = next;
}
bsp_status_t bsp_uart_rx_pop(bsp_uart_rx_t *rx, uint8_t *byte)
{
    if (rx == NULL || byte == NULL) return BSP_STATUS_INVALID_ARG;
    if (rx->poisoned)
    {
        rx->head = rx->tail = 0u;
        rx->poisoned = false;
        return BSP_STATUS_ERROR;
    }
    if (rx->tail == rx->head) return BSP_STATUS_BUSY;
    *byte = rx->data[rx->tail];
    rx->tail = (uint16_t)((rx->tail + 1u) & (BSP_UART_RX_CAPACITY - 1u));
    return BSP_STATUS_OK;
}
void bsp_uart_rx_block(bsp_uart_rx_t *rx, uint8_t reason, bool blocked)
{
    if (rx == NULL) return;
    const uint8_t next = blocked ? (uint8_t)(rx->blocked | reason) : (uint8_t)(rx->blocked & (uint8_t)~reason);
    if (next == rx->blocked) return;
    rx->blocked = next;
    rx->head = rx->tail = 0u;
    rx->poisoned = false;
    ++rx->epoch;
}
