#ifndef WTK_BSP_EXCITATION_H
#define WTK_BSP_EXCITATION_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_status.h"
#include "wtk_build_config.h"

typedef enum
{
    BSP_EXCITATION_MODE_OFF = 0,
    BSP_EXCITATION_MODE_NEUTRAL,
    BSP_EXCITATION_MODE_SINE,
    BSP_EXCITATION_MODE_DC_PILOT,
} bsp_excitation_mode_t;

bsp_status_t bsp_excitation_init(void);
bsp_status_t bsp_excitation_off(void);
bsp_status_t bsp_excitation_neutral(void);
#if WTK_ENABLE_DC_PILOT
bsp_status_t bsp_excitation_dc_pilot(void);
#endif
bsp_status_t bsp_excitation_sine(uint8_t rcr, const uint16_t *ccr, uint32_t count);
bsp_excitation_mode_t bsp_excitation_mode(void);
bool bsp_excitation_dma_error(void);
void DMA1_Channel5_IRQHandler(void);

#endif
