#ifndef WTK_BSP_UART_RX_H
#define WTK_BSP_UART_RX_H
#include <stdbool.h>
#include <stdint.h>
#include "bsp/bsp_status.h"
enum { BSP_UART_RX_CAPACITY = 256u, BSP_UART_RX_QUIET = 1u, BSP_UART_RX_APPLICATION = 2u };
/* ISR producer; caller masks the UART ISR around main-loop pop/block/snapshot.
 * Errors poison the whole pending frame rather than publishing damaged bytes. */
typedef struct
{
    uint8_t data[BSP_UART_RX_CAPACITY];
    volatile uint16_t head, tail;
    volatile uint32_t epoch, errors, overflows, discarded;
    volatile uint8_t blocked;
    volatile bool poisoned;
} bsp_uart_rx_t;
void bsp_uart_rx_init(bsp_uart_rx_t *rx);
void bsp_uart_rx_push(bsp_uart_rx_t *rx, uint8_t byte, bool error);
bsp_status_t bsp_uart_rx_pop(bsp_uart_rx_t *rx, uint8_t *byte);
void bsp_uart_rx_block(bsp_uart_rx_t *rx, uint8_t reason, bool blocked);
#endif
