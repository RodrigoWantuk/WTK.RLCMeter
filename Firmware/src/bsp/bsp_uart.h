#ifndef WTK_BSP_UART_H
#define WTK_BSP_UART_H

#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "bsp/bsp_uart_rx.h"

bsp_status_t bsp_uart_init(uint32_t baud_rate);
bsp_status_t bsp_uart_write(const char *data, size_t length);
bsp_status_t bsp_uart_write_cstr(const char *text);
bsp_status_t bsp_uart_try_read_byte(uint8_t *byte);
bsp_status_t bsp_uart_try_write_byte(uint8_t byte);
void bsp_uart_rx_set_blocked(uint8_t reason, bool blocked);
uint32_t bsp_uart_rx_epoch(void);
void bsp_uart_rx_counters(uint32_t *errors, uint32_t *overflows);

#endif
