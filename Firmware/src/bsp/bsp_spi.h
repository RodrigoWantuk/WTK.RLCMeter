#ifndef WTK_BSP_SPI_H
#define WTK_BSP_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp/bsp_status.h"

typedef enum
{
    BSP_SPI_PRESCALER_DIV2 = 0,
    BSP_SPI_PRESCALER_DIV4,
    BSP_SPI_PRESCALER_DIV8,
    BSP_SPI_PRESCALER_DIV16,
    BSP_SPI_PRESCALER_DIV32,
    BSP_SPI_PRESCALER_DIV64,
    BSP_SPI_PRESCALER_DIV128,
    BSP_SPI_PRESCALER_DIV256,
} bsp_spi_prescaler_t;

typedef struct
{
    bsp_spi_prescaler_t prescaler;
    bool cpol;
    bool cpha;
} bsp_spi_config_t;

bsp_status_t bsp_spi2_init(const bsp_spi_config_t *config);
bsp_status_t bsp_spi2_configure(const bsp_spi_config_t *config);
bsp_status_t bsp_spi2_transfer(const uint8_t *tx, uint8_t *rx, size_t length, uint32_t timeout_ms);

#endif
