#include "bsp/bsp_gpio.h"

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx.h"

enum
{
    GPIO_MODE_INPUT_FLOATING = 0x4u,
    GPIO_MODE_INPUT_PULL = 0x8u,
    GPIO_MODE_OUTPUT_PP_2MHZ = 0x2u,
    GPIO_MODE_AF_PP_2MHZ = 0xAu,
};

static bool g_swd_preserved = false;

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

static void gpio_set(GPIO_TypeDef *const port, uint32_t pin)
{
    port->BSRR = 1u << pin;
}

static void gpio_clear(GPIO_TypeDef *const port, uint32_t pin)
{
    port->BSRR = 1u << (pin + 16u);
}

bsp_status_t bsp_gpio_init_safe(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN |
                   RCC_APB2ENR_IOPAEN |
                   RCC_APB2ENR_IOPBEN |
                   RCC_APB2ENR_IOPCEN;

    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
    g_swd_preserved = true;

    gpio_clear(GPIOA, 8u);   /* PWM_EXC inactive */
    gpio_clear(GPIOA, 11u);  /* K2_CMD safe/de-energized */
    gpio_set(GPIOA, 12u);    /* FLASH_CS inactive */

    gpio_clear(GPIOB, 0u);   /* TFT_BL off */
    gpio_clear(GPIOB, 1u);   /* BUZZER off */
    gpio_clear(GPIOB, 5u);   /* RANGE_A0 deterministic while RANGE_EN is off */
    gpio_clear(GPIOB, 6u);   /* RANGE_A1 deterministic while RANGE_EN is off */
    gpio_clear(GPIOB, 7u);   /* RANGE_A2 deterministic while RANGE_EN is off */
    gpio_clear(GPIOB, 8u);   /* RANGE_EN disabled */
    gpio_clear(GPIOB, 9u);   /* K1_CMD safe/de-energized */
    gpio_clear(GPIOB, 10u);  /* TFT_RST held low until display phase */
    gpio_clear(GPIOB, 11u);  /* TFT_DC deterministic */
    gpio_set(GPIOB, 12u);    /* TFT_CS inactive */
    gpio_set(GPIOB, 3u);     /* SW_UP pull-up */
    gpio_set(GPIOB, 4u);     /* SW_DOWN pull-up */
    gpio_set(GPIOC, 13u);    /* SW_OK pull-up */

    gpio_config_pin(GPIOA, 8u, GPIO_MODE_OUTPUT_PP_2MHZ);
    gpio_config_pin(GPIOA, 11u, GPIO_MODE_OUTPUT_PP_2MHZ);
    gpio_config_pin(GPIOA, 12u, GPIO_MODE_OUTPUT_PP_2MHZ);

    gpio_config_pin(GPIOB, 0u, GPIO_MODE_OUTPUT_PP_2MHZ);
    gpio_config_pin(GPIOB, 1u, GPIO_MODE_OUTPUT_PP_2MHZ);
    gpio_config_pin(GPIOB, 5u, GPIO_MODE_OUTPUT_PP_2MHZ);
    gpio_config_pin(GPIOB, 6u, GPIO_MODE_OUTPUT_PP_2MHZ);
    gpio_config_pin(GPIOB, 7u, GPIO_MODE_OUTPUT_PP_2MHZ);
    gpio_config_pin(GPIOB, 8u, GPIO_MODE_OUTPUT_PP_2MHZ);
    gpio_config_pin(GPIOB, 9u, GPIO_MODE_OUTPUT_PP_2MHZ);
    gpio_config_pin(GPIOB, 10u, GPIO_MODE_OUTPUT_PP_2MHZ);
    gpio_config_pin(GPIOB, 11u, GPIO_MODE_OUTPUT_PP_2MHZ);
    gpio_config_pin(GPIOB, 12u, GPIO_MODE_OUTPUT_PP_2MHZ);

    gpio_config_pin(GPIOA, 15u, GPIO_MODE_INPUT_FLOATING);
    gpio_config_pin(GPIOB, 3u, GPIO_MODE_INPUT_PULL);
    gpio_config_pin(GPIOB, 4u, GPIO_MODE_INPUT_PULL);
    gpio_config_pin(GPIOC, 13u, GPIO_MODE_INPUT_PULL);

    return BSP_STATUS_OK;
}

bool bsp_gpio_swd_preserved(void)
{
    return g_swd_preserved;
}
