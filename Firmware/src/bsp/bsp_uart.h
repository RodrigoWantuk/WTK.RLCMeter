#ifndef WTK_BSP_UART_H
#define WTK_BSP_UART_H

#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_status.h"

bsp_status_t bsp_uart_init(uint32_t baud_rate);
bsp_status_t bsp_uart_write(const char *data, size_t length);
bsp_status_t bsp_uart_write_cstr(const char *text);

#endif
