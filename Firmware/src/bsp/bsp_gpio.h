#ifndef WTK_BSP_GPIO_H
#define WTK_BSP_GPIO_H

#include <stdbool.h>

#include "bsp/bsp_status.h"

typedef enum
{
    BSP_GPIO_OUTPUT_FLASH_CS = 0,
    BSP_GPIO_OUTPUT_TFT_CS,
    BSP_GPIO_OUTPUT_TFT_DC,
    BSP_GPIO_OUTPUT_TFT_RST,
    BSP_GPIO_OUTPUT_TFT_BL,
    BSP_GPIO_OUTPUT_BUZZER,
} bsp_gpio_output_t;

typedef enum
{
    BSP_GPIO_INPUT_BUTTON_UP = 0,
    BSP_GPIO_INPUT_BUTTON_OK,
    BSP_GPIO_INPUT_BUTTON_DOWN,
    BSP_GPIO_INPUT_CHARGER_DETECT,
} bsp_gpio_input_t;

bsp_status_t bsp_gpio_init_safe(void);
bool bsp_gpio_swd_preserved(void);
bsp_status_t bsp_gpio_write_output(bsp_gpio_output_t output, bool high);
bsp_status_t bsp_gpio_read_input(bsp_gpio_input_t input, bool *active);

#endif
